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

// File: ischecker_relay.cc

#include <fstream>
#include <set>

#include <ilang/util/log.h>
#include <ilang/util/str_util.h>
#include <nlohmann/json.hpp>

#include <relay/relay_func_call.h>
#include <relay/relay_maxpooling.h>

#include <pffc/ischecker_flex_relay.h>

using json = nlohmann::json;

namespace ilang {

void IsCheckerFlexRelay::SetRelayCmd(const fs::path& cmd_file) {
  ILA_ASSERT(fs::is_regular_file(cmd_file)) << cmd_file;
  ILA_WARN_IF(!cmd_seq_relay_.empty()) << "Relay command not empty";

  std::ifstream fin(cmd_file);
  json cmd_reader;
  fin >> cmd_reader;
  fin.close();

  static const std::set<std::string> relay_cmd_fields = {
      "data_in",     "data_in_x",   "data_in_y", "func_id", "func_run",
      "pool_size_x", "pool_size_y", "stride_x",  "stride_y"};

  for (auto& cmd : cmd_reader.at("command inputs")) {
    cmd_seq_relay_.push_back(CmdType());
    auto& curr = cmd_seq_relay_.back();

    try {
      for (auto& field : relay_cmd_fields) {
        auto value_str = cmd.at(field).get<std::string>();
        auto value = StrToULongLong(RemoveHexPrefix(value_str), 16);
        curr[field] = value;
      }
    } catch (...) {
      ILA_ERROR << "Fail parsing command " << cmd;
    }
  }
}

void IsCheckerFlexRelay::AddEnvM1() {
  ILA_INFO << "Adding relay specific constraints";
  ILA_ASSERT(!cmd_seq_relay_.empty()) << "No Relay command provided";
  ILA_ASSERT(instr_seq_m1_.size() >= cmd_seq_relay_.size());

  // constraint input of top-level instr
  for (auto i = 0, j = 0; i < instr_seq_m1_.size(); i++) {
    auto instr = instr_seq_m1_.at(i);

    // only apply to top-level instr
    if (top_instr_m1_.find(instr.name()) == top_instr_m1_.end()) {
      continue;
    }

    // only constrain on non-data parts
    auto data_free_cmd = FilterRelayCmd(instr.name(), j);
    unroller_m1_->AddStepPred(i, data_free_cmd);

    // increment cmd ptr
    j++;
  }
}

ExprRef IsCheckerFlexRelay::FilterRelayCmd(const std::string& instr_name,
                                           size_t cmd_idx) {
  auto& cmd = cmd_seq_relay_[cmd_idx];
  auto func_id = cmd.at("func_id");
  auto func_run = cmd.at("func_run");

  // func_run & func_id
  auto cmd_expr = (m1_.input(RELAY_FUNC_RUN_IN) == func_run) &
                  (m1_.input(RELAY_FUNC_ID_IN) == func_id);

  if (func_id == F_TENSOR_STORE_ID) {
    auto addr = cmd.at("data_in_y");
    cmd_expr = cmd_expr & (m1_.input(DATA_IN_Y) == cmd.at("data_in_y"));
    store_relay_.insert({addr, cmd_idx});

  } else if (func_id == F_MAXPOOLING_2D_ID) {
    cmd_expr = cmd_expr & (m1_.input(RELAY_DATA_IN) == cmd.at("data_in"));
    cmd_expr = cmd_expr & (m1_.input(DATA_IN_Y) == cmd.at("data_in_y"));
    cmd_expr = cmd_expr & (m1_.input(DATA_IN_X) == cmd.at("data_in_x"));
    cmd_expr = cmd_expr & (m1_.input(POOL_SIZE_Y_IN) == cmd.at("pool_size_y"));
    cmd_expr = cmd_expr & (m1_.input(POOL_SIZE_X_IN) == cmd.at("pool_size_x"));
    cmd_expr = cmd_expr & (m1_.input(STRIDES_Y_IN) == cmd.at("stride_y"));
    cmd_expr = cmd_expr & (m1_.input(STRIDES_X_IN) == cmd.at("stride_x"));

  } else if (func_id == F_LSTM_ID) {
    // TODO
  }

  return cmd_expr;
}

} // namespace ilang
