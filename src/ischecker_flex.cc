// =============================================================================
// MIT License
//
// Copyright (c) 2020 Princeton University
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// =============================================================================

// File: ischecker_flex.cc

#include <fstream>
#include <set>

#include <ilang/target-smt/smt_switch_itf.h>
#include <ilang/target-smt/z3_expr_adapter.h>
#include <ilang/util/log.h>
#include <ilang/util/str_util.h>
#include <nlohmann/json.hpp>

#include <flex/top_config.h>

#include <pffc/ischecker_flex_relay.h>

using json = nlohmann::json;

namespace ilang {

#ifdef USE_Z3
template class IsCheckerFlexRelay<Z3ExprAdapter>;
#else
template class IsCheckerFlexRelay<SmtSwitchItf>;
#endif

template <class Generator>
const std::vector<std::string> IsCheckerFlexRelay<Generator>::k_flex_in_data = {
    TOP_DATA_IN_0,  //
    TOP_DATA_IN_1,  //
    TOP_DATA_IN_2,  //
    TOP_DATA_IN_3,  //
    TOP_DATA_IN_4,  //
    TOP_DATA_IN_5,  //
    TOP_DATA_IN_6,  //
    TOP_DATA_IN_7,  //
    TOP_DATA_IN_8,  //
    TOP_DATA_IN_9,  //
    TOP_DATA_IN_10, //
    TOP_DATA_IN_11, //
    TOP_DATA_IN_12, //
    TOP_DATA_IN_13, //
    TOP_DATA_IN_14, //
    TOP_DATA_IN_15  //
};

template <class Generator>
void IsCheckerFlexRelay<Generator>::SetFlexCmd(const fs::path& cmd_file) {
  ILA_ASSERT(fs::is_regular_file(cmd_file)) << cmd_file;
  ILA_WARN_IF(!cmd_seq_flex_.empty()) << "Flex command not empty";

  std::ifstream fin(cmd_file);
  json cmd_reader;
  fin >> cmd_reader;
  fin.close();

  static const std::set<std::string> flex_cmd_fields = {"is_rd", "is_wr",
                                                        "addr"};

  for (auto& cmd : cmd_reader.at("command inputs")) {
    cmd_seq_flex_.push_back(CmdType());
    auto& curr = cmd_seq_flex_.back();

    try {
      for (auto& field : flex_cmd_fields) {
        auto value_str = cmd.at(field).get<std::string>();
        auto value = StrToULongLong(RemoveHexPrefix(value_str), 16);
        curr[field] = value;
      }

      // assign 128-bit data to 16 * 8-bit data ports
      auto data_str = cmd.at("data").get<std::string>();
      data_str = RemoveHexPrefix(data_str);
      if (data_str.size() < 32) { // zero padding
        data_str = std::string(32 - data_str.size(), '0') + data_str;
      }
      for (auto i = 0; i < 16; i++) {
        auto data_val = StrToULongLong(data_str.substr(30 - (i * 2), 2), 16);
        curr[k_flex_in_data.at(i)] = data_val;
      }
    } catch (...) {
      ILA_ERROR << "Fail parsing command " << cmd;
    }
  }
}

template <class Generator> void IsCheckerFlexRelay<Generator>::AddEnvM0() {
  ILA_INFO << "Adding flex specific constraints";
  ILA_ASSERT(!cmd_seq_flex_.empty()) << "No Flex command provided";
  ILA_ASSERT(this->instr_seq_m0_.size() >= cmd_seq_flex_.size());

  // constraint input of top-level instr.
  for (auto i = 0, j = 0; i < this->instr_seq_m0_.size(); i++) {
    auto instr = this->instr_seq_m0_.at(i);

    // only apply to top-level instr
    if (this->top_instr_m0_.find(instr.name()) == this->top_instr_m0_.end()) {
      continue;
    }

    // only constrain on non-data parts
    auto data_free_cmd = FilterFlexCmd(instr.name(), j);
    this->unroller_m0_->AssertStep(data_free_cmd.get(), i);

    // increment cmd ptr
    j++;
  }
}

static const std::set<std::string> k_data_setup_instr = {
    "GB_CORE_STORE_LARGE" //
};

template <class Generator>
ExprRef
IsCheckerFlexRelay<Generator>::FilterFlexCmd(const std::string& instr_name,
                                             size_t cmd_idx) {
  auto& cmd = cmd_seq_flex_[cmd_idx];
  auto& m = this->m0_;

  // read/write
  auto in_axi_wr = m.input(TOP_IF_WR);
  auto in_axi_rd = m.input(TOP_IF_RD);
  auto cmd_expr =
      (in_axi_wr == cmd.at("is_wr")) & (in_axi_rd == cmd.at("is_rd"));

  // address
  auto addr_val = cmd.at("addr");
  auto in_axi_addr = m.input(TOP_ADDR_IN);
  cmd_expr = cmd_expr & (in_axi_addr == addr_val);

  // data setup instr
  if (k_data_setup_instr.find(instr_name) != k_data_setup_instr.end()) {
    store_flex_.insert({addr_val, cmd_idx});
    return cmd_expr;
  }

  // data
  for (auto& data_port : k_flex_in_data) {
    auto data_inp = m.input(data_port);
    auto data_val = cmd.at(data_port);
    cmd_expr = cmd_expr & (data_inp == data_val);
  }

  return cmd_expr;
}

} // namespace ilang
