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
#include <ilang/target-smt/smt_switch_itf.h>
#include <ilang/target-smt/z3_expr_adapter.h>
#include <ilang/util/log.h>
#include <ilang/util/str_util.h>
#include <nlohmann/json.hpp>

#ifdef USE_Z3
#include <z3++.h>
#else
#include <smt-switch/smt.h>
#endif

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

#ifdef USE_Z3
template class IsCheckerFlexRelay<Z3ExprAdapter>;
#else
template class IsCheckerFlexRelay<SmtSwitchItf>;
#endif

template <class Generator>
void IsCheckerFlexRelay<Generator>::SetAddrMapping(const fs::path& mapping) {
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

template <class Generator>
typename IsChecker<Generator>::SmtExpr
IsCheckerFlexRelay<Generator>::GetMiter() {
  ILA_INFO << "Setting memory relation (miter)";

  auto& m0 = this->m0_;
  auto& m1 = this->m1_;
  auto& unroller_m0 = this->unroller_m0_;
  auto& unroller_m1 = this->unroller_m1_;
  auto& instr_seq_m0 = this->instr_seq_m0_;
  auto& instr_seq_m1 = this->instr_seq_m1_;

  auto flex_mem = m0.state(GB_CORE_LARGE_BUFFER);
  auto relay_mem = m1.state(RELAY_TENSOR_MEM);

  // start
  auto flex_start = unroller_m0->GetSmtCurrent(flex_mem.get(), 0);
  auto relay_start = unroller_m1->GetSmtCurrent(relay_mem.get(), 0);
  auto same_start = this->smt_gen_.Equal(flex_start, relay_start);
  ILA_DLOG("3LA") << fmt::format("{} @ 0 == {} @ 0", flex_mem.name(),
                                 relay_mem.name());

  // store
  ILA_ASSERT(!store_flex_.empty());
  ILA_ASSERT(!store_relay_.empty());
  ILA_ASSERT(store_flex_.size() * 16 == store_relay_.size());

  auto relay_in_data = m1.input(RELAY_DATA_IN);
  auto same_store = this->smt_gen_.GetShimExpr(BoolConst(true).get());

  for (auto flex_iter : store_flex_) {
    auto flex_addr = flex_iter.first;
    auto flex_step = flex_iter.second;

    for (auto i = 0; i < 16; i++) {
      auto flex_in_data = m0.input(k_flex_in_data.at(i));
      auto flex_data =
          unroller_m0->GetSmtCurrent(flex_in_data.get(), flex_step);

      auto relay_addr = addr_mapping_.at(flex_addr + i);
      auto relay_step = store_relay_.at(relay_addr);
      auto relay_data =
          unroller_m1->GetSmtCurrent(relay_in_data.get(), relay_step);

      same_store = this->smt_gen_.BoolAnd(
          same_store, this->smt_gen_.Equal(flex_data, relay_data));

      ILA_DLOG("3LA") << fmt::format("{} @ {} == {} @ {}", flex_in_data.name(),
                                     flex_step, relay_in_data.name(),
                                     relay_step);
    }
  }

  // end
  auto same_end = this->smt_gen_.GetShimExpr(BoolConst(true).get());

  for (auto flex_iter : store_flex_) {
    auto flex_addr = flex_iter.first;
    auto flex_data = Load(flex_mem, flex_addr);
    auto end_f =
        unroller_m0->GetSmtCurrent(flex_data.get(), instr_seq_m0.size());

    for (auto i = 0; i < 16; i++) {
      auto relay_addr = addr_mapping_.at(flex_addr + i);
      auto relay_data = Load(relay_mem, relay_addr);
      auto end_r =
          unroller_m1->GetSmtCurrent(relay_data.get(), instr_seq_m1.size());

      same_end =
          this->smt_gen_.BoolAnd(same_end, this->smt_gen_.Equal(end_f, end_r));
    }
  }

#ifdef USE_Z3

  return same_start && same_store && !same_end;

#else

  auto& smt_solver = this->smt_gen_.get().solver();
#if 0 // sanity check - should be sat
  return smt_solver->make_term(
      smt::PrimOp::And, same_start,
      smt_solver->make_term(smt::PrimOp::And, same_store, same_end));
#else
  return smt_solver->make_term(
      smt::PrimOp::And, same_start,
      smt_solver->make_term(smt::PrimOp::And, same_store,
                            smt_solver->make_term(smt::PrimOp::Not, same_end)));
#endif

#endif
}

template <class Generator>
typename IsChecker<Generator>::SmtExpr
IsCheckerFlexRelay<Generator>::GetUninterpFunc() {
  auto& unroller_m0 = this->unroller_m0_;
  auto& unroller_m1 = this->unroller_m1_;

  auto interp = this->smt_gen_.GetShimExpr(BoolConst(true).get());

  auto flex_func_max = unroller_m0->GetSmtFuncDecl(flex::GBAdpfloat_max.get());
  auto relay_func_max = unroller_m1->GetSmtFuncDecl(relay::adpfloat_max.get());

#ifdef USE_Z3
  auto& ctx = this->smt_gen_.get().context();
  auto a = ctx.bv_const("a", TOP_DATA_IN_WIDTH);
  auto b = ctx.bv_const("b", TOP_DATA_IN_WIDTH);

  interp = interp && //
           z3::forall(a, b, flex_func_max(a, b) == relay_func_max(a, b)) &&
           z3::forall(a, b, flex_func_max(a, b) == relay_func_max(b, a)) &&
           z3::forall(a, b,
                      (flex_func_max(a, b) == a) || (flex_func_max(a, b) == b));

#else // not USE_Z3
  auto& solver = this->smt_gen_.get().solver();

  int uninterp_var_cnt = 0;
  auto _new_var = [&uninterp_var_cnt]() {
    return fmt::format("uninterp_var_{}", uninterp_var_cnt++);
  };

  auto _forallab = [&solver, &_new_var](auto& p) {
    auto bvsort = solver->make_sort(smt::BV, TOP_DATA_IN_WIDTH);
    auto a = solver->make_param(_new_var(), bvsort);
    auto b = solver->make_param(_new_var(), bvsort);
    auto forallb = solver->make_term(smt::PrimOp::Forall, b, p(a, b));
    return solver->make_term(smt::PrimOp::Forall, a, forallb);
  };

  auto fabeqrab = [&solver, &flex_func_max, &relay_func_max](auto& a, auto& b) {
    auto fab = solver->make_term(smt::PrimOp::Apply, flex_func_max, a, b);
    auto rab = solver->make_term(smt::PrimOp::Apply, relay_func_max, a, b);
    return solver->make_term(smt::PrimOp::Equal, fab, rab);
  };

  auto fabeqrba = [&solver, &flex_func_max, &relay_func_max](auto& a, auto& b) {
    auto fab = solver->make_term(smt::PrimOp::Apply, flex_func_max, a, b);
    auto rba = solver->make_term(smt::PrimOp::Apply, relay_func_max, b, a);
    return solver->make_term(smt::PrimOp::Equal, fab, rba);
  };

  auto withinab = [&solver, &flex_func_max](auto& a, auto& b) {
    auto fab = solver->make_term(smt::PrimOp::Apply, flex_func_max, a, b);
    return solver->make_term(smt::PrimOp::Or,
                             solver->make_term(smt::PrimOp::Equal, fab, a),
                             solver->make_term(smt::PrimOp::Equal, fab, b));
  };

#if 1
  interp = solver->make_term(smt::PrimOp::Equal, flex_func_max, relay_func_max);
#else
  interp = solver->make_term(smt::PrimOp::And, interp, _forallab(fabeqrab));
  interp = solver->make_term(smt::PrimOp::And, interp, _forallab(fabeqrba));
  interp = solver->make_term(smt::PrimOp::And, interp, _forallab(withinab));
#endif

#endif

  return interp;
}

#ifdef USE_Z3
template <class Generator>
void IsCheckerFlexRelay<Generator>::Debug(z3::model& model) {
  auto& m0 = this->m0_;
  auto& m1 = this->m1_;
  auto& unroller_m0 = this->unroller_m0_;
  auto& unroller_m1 = this->unroller_m1_;
  auto& instr_seq_m0 = this->instr_seq_m0_;
  auto& instr_seq_m1 = this->instr_seq_m1_;

  auto flex_mem = m0.state(GB_CORE_LARGE_BUFFER);
  auto relay_mem = m1.state(RELAY_TENSOR_MEM);
  auto flex_end =
      unroller_m0->GetSmtCurrent(flex_mem.get(), instr_seq_m0.size());
  auto relay_end =
      unroller_m1->GetSmtCurrent(relay_mem.get(), instr_seq_m1.size());

  //
  std::ofstream flex_out("flex_out.txt");
  for (auto i = 0; i <= instr_seq_m0.size(); i++) {
    auto flex_i = unroller_m0->GetSmtCurrent(Load(flex_mem, 0).get(), i);
    flex_out << i << ": " << model.eval(flex_i) << "\n";
  }
  flex_out << "complete mem:\n";
  flex_out << model.eval(flex_end);
  flex_out.close();

  //
  std::ofstream relay_out("relay_out.txt");
  for (auto i = 0; i <= instr_seq_m1.size(); i++) {
    auto relay_i = unroller_m1->GetSmtCurrent(Load(relay_mem, 0).get(), i);
    relay_out << i << ": " << model.eval(relay_i) << "\n";
  }

  relay_out << "complete mem:\n";
  relay_out << model.eval(relay_end);
  relay_out.close();
}
#endif

} // namespace ilang
