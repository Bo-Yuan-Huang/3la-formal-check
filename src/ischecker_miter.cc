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

// File: ischecker_flex_miter.cc

#include <fstream>

#include <fmt/format.h>
#include <ilang/util/log.h>
#include <ilang/util/str_util.h>
#include <nlohmann/json.hpp>

#include <flex/gb_core.h>
#include <flex/top_config.h>
#include <flex/uninterpreted_func.h>
#include <relay/relay_func_call.h>
#include <relay/relay_maxpooling.h>
#include <relay/relay_top_config.h>
#include <relay/uninterpreted_func.h>

#include <pffc/ischecker_flex_relay.h>

using json = nlohmann::json;

namespace ilang {

void IsCheckerFlexRelay::SetAddrMapping(const fs::path& mapping) {
  json mapping_reader;

  std::ifstream fin(mapping);
  ILA_ASSERT(fin.is_open());
  fin >> mapping_reader;
  fin.close();

  for (const auto& pair : mapping_reader.at("address mapping")) {
    auto flex_addr_str = pair.at("flex_addr").get<std::string>();
    auto relay_addr_str = pair.at("relay_addr").get<std::string>();
    auto flex_addr = StrToULongLong(RemoveHexPrefix(flex_addr_str), 16);
    auto relay_addr = StrToULongLong(RemoveHexPrefix(relay_addr_str), 16);
    auto [it, status] = addr_mapping_.insert({flex_addr, relay_addr});
    ILA_ASSERT(status);
  }
}

z3::expr IsCheckerFlexRelay::GetMiter() {
  ILA_INFO << "Setting memory relation (miter)";

  auto flex_mem = m0_.state(GB_CORE_LARGE_BUFFER);
  auto relay_mem = m1_.state(RELAY_TENSOR_MEM);

  // start
  auto flex_start = unroller_m0_->CurrState(flex_mem, 0);
  auto relay_start = unroller_m1_->CurrState(relay_mem, 0);
  auto same_start = (flex_start == relay_start);
  ILA_DLOG("3LA") << fmt::format("{} @ 0 == {} @ 0", flex_mem.name(),
                                 relay_mem.name());

  // store
  ILA_ASSERT(!store_flex_.empty());
  ILA_ASSERT(!store_relay_.empty());
  ILA_ASSERT(store_flex_.size() * 16 == store_relay_.size());

  auto relay_in_data = m1_.input(RELAY_DATA_IN);
  auto same_store = ctx_.bool_val(true);

  for (auto flex_iter : store_flex_) {
    auto flex_addr = flex_iter.first;
    auto flex_step = flex_iter.second;

    for (auto i = 0; i < 16; i++) {
      auto flex_in_data = m0_.input(k_flex_in_data.at(i));
      auto flex_data = unroller_m0_->CurrState(flex_in_data, flex_step);

      auto relay_addr = addr_mapping_.at(flex_addr + i);
      auto relay_step = store_relay_.at(relay_addr);
      auto relay_data = unroller_m1_->CurrState(relay_in_data, relay_step);

      same_store = (same_store && (flex_data == relay_data));
      ILA_DLOG("3LA") << fmt::format("{} @ {} == {} @ {}", flex_in_data.name(),
                                     flex_step, relay_in_data.name(),
                                     relay_step);
    }
  }

  // end
  auto same_end = ctx_.bool_val(true);

  for (auto flex_iter : store_flex_) {
    auto flex_addr = flex_iter.first;
    auto flex_data = Load(flex_mem, flex_addr);
    auto end_f = unroller_m0_->GetZ3Expr(flex_data, instr_seq_m0_.size());

    for (auto i = 0; i < 16; i++) {
      auto relay_addr = addr_mapping_.at(flex_addr + i);
      auto relay_data = Load(relay_mem, relay_addr);
      auto end_r = unroller_m1_->GetZ3Expr(relay_data, instr_seq_m1_.size());

      same_end = (same_end && (end_f == end_r));
    }
  }

  return same_start && same_store && !same_end;
}

z3::expr IsCheckerFlexRelay::GetUninterpFunc() {
  auto interp = ctx_.bool_val(true);

  auto flex_func_max = unroller_m0_->GetZ3FuncDecl(flex::GBAdpfloat_max);
  auto relay_func_max = unroller_m1_->GetZ3FuncDecl(relay::adpfloat_max);

  auto a = ctx_.bv_const("a", TOP_DATA_IN_WIDTH);
  auto b = ctx_.bv_const("b", TOP_DATA_IN_WIDTH);

#if 1
  interp = interp && //
           z3::forall(a, b, flex_func_max(a, b) == relay_func_max(a, b)) &&
           z3::forall(a, b, flex_func_max(a, b) == relay_func_max(b, a)) &&
           z3::forall(a, b,
                      (flex_func_max(a, b) == a) || (flex_func_max(a, b) == b));
#else
  interp = interp &&
           z3::forall(a, b, flex_func_max(a, b) == z3::ite(a >= b, a, b)) &&
           z3::forall(a, b, relay_func_max(a, b) == z3::ite(a >= b, a, b));
#endif

  return interp;
}

void IsCheckerFlexRelay::Debug(z3::model& model) {
  auto flex_mem = m0_.state(GB_CORE_LARGE_BUFFER);
  auto relay_mem = m1_.state(RELAY_TENSOR_MEM);
  auto flex_end = unroller_m0_->CurrState(flex_mem, instr_seq_m0_.size());
  auto relay_end = unroller_m1_->CurrState(relay_mem, instr_seq_m1_.size());

  //
  std::ofstream flex_out("flex_out.txt");
  for (auto i = 0; i <= instr_seq_m0_.size(); i++) {
    auto flex_i = unroller_m0_->GetZ3Expr(Load(flex_mem, 0), i);
    flex_out << i << ": " << model.eval(flex_i) << "\n";
  }
  flex_out << "complete mem:\n";
  flex_out << model.eval(flex_end);
  flex_out.close();

  //
  std::ofstream relay_out("relay_out.txt");
  for (auto i = 0; i <= instr_seq_m1_.size(); i++) {
    auto relay_i = unroller_m1_->GetZ3Expr(Load(relay_mem, 0), i);
    relay_out << i << ": " << model.eval(relay_i) << "\n";
  }

  relay_out << "complete mem:\n";
  relay_out << model.eval(relay_end);
  relay_out.close();
}

} // namespace ilang
