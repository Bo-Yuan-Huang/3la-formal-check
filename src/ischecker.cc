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

// File: ischecker.cc

#include <fstream>

#include <ilang/ila/instr_lvl_abs.h>
#include <ilang/target-smt/smt_switch_itf.h>
#include <ilang/target-smt/z3_expr_adapter.h>
#include <ilang/util/log.h>
#include <nlohmann/json.hpp>

#include <pffc/ischecker.h>

using json = nlohmann::json;

namespace ilang {

#ifdef USE_Z3
template class IsChecker<Z3ExprAdapter>;
#else
template class IsChecker<SmtSwitchItf>;
#endif

template <class Generator>
IsChecker<Generator>::IsChecker(const Ila& m0, const Ila& m1,
                                SmtShim<Generator>& smt_gen)
    : m0_(m0), m1_(m1), smt_gen_(smt_gen) {
  unroller_m0_ = new PathUnroller<Generator>(smt_gen_);
  unroller_m1_ = new PathUnroller<Generator>(smt_gen_);
  Preprocess();
}

template <class Generator> IsChecker<Generator>::~IsChecker() {
  if (unroller_m0_) {
    delete unroller_m0_;
    unroller_m0_ = nullptr;
  }

  if (unroller_m1_) {
    delete unroller_m1_;
    unroller_m1_ = nullptr;
  }
}

template <class Generator> bool IsChecker<Generator>::Check() {
  // make sure the sequence has been specified
  if (instr_seq_m0_.empty() or instr_seq_m1_.empty()) {
    ILA_ERROR << "Instruction sequence not set";
    return false;
  }

  // optimize
#if 0
  std::vector<Ila::PassID> pass = {Ila::PassID::SIMPLIFY_SYNTACTIC,
                                   Ila::PassID::REWRITE_CONDITIONAL_STORE};
  m0_.ExecutePass(pass);
  m1_.ExecutePass(pass);
#endif

  // unroll the program and get the smt expression
  ILA_NOT_NULL(unroller_m0_);
  ILA_NOT_NULL(unroller_m1_);

  // add design specific constraints
  AddEnvM0();
  AddEnvM1();

  // unroll two instruction sequences
  InstrVec instr_seq_m0;
  InstrVec instr_seq_m1;
  for (const auto& i : instr_seq_m0_) {
    instr_seq_m0.push_back(i.get());
  }
  for (const auto& i : instr_seq_m1_) {
    instr_seq_m1.push_back(i.get());
  }
  auto is0 = unroller_m0_->Unroll(instr_seq_m0);
  auto is1 = unroller_m1_->Unroll(instr_seq_m1);

  // miter
  auto miter = GetMiter();

  // func
  auto uninterp_func = GetUninterpFunc();

  // start solving
  ILA_INFO << "Start solving";

#ifdef USE_Z3
  auto& ctx = smt_gen_.get().context();
  z3::solver solver(ctx);
  solver.add(is0);
  solver.add(is1);
  solver.add(miter);
  solver.add(uninterp_func);

  auto res = solver.check();
  if (res == z3::sat) {
    auto model = solver.get_model();
    Debug(model);
  }
  ILA_INFO << "Result: " << res;
  return res == z3::unsat;

#else // not USE_Z3
  auto& solver = smt_gen_.get().solver();
  solver->assert_formula(is0);
  solver->assert_formula(is1);
  solver->assert_formula(miter);
  solver->assert_formula(uninterp_func);

  auto res = solver->check_sat();
  ILA_INFO << "Result: " << res;
  return res.is_unsat();

#endif
}

template <class Generator>
void IsChecker<Generator>::SetInstrSeq(const int& idx, const fs::path& file) {
  ILA_ASSERT(fs::is_regular_file(file)) << file;
  if (idx == 0) {
    ReadInstrSeq(m0_, file, instr_seq_m0_);
  } else {
    ReadInstrSeq(m1_, file, instr_seq_m1_);
  }
}

template <class Generator> void IsChecker<Generator>::Preprocess() {
  // bookkeeping top-level instructions
  GetTopInstr(m0_, top_instr_m0_);
  GetTopInstr(m1_, top_instr_m1_);

  // flatten hierarchy
  m0_.FlattenHierarchy();
  m1_.FlattenHierarchy();
}

template <class Generator>
void IsChecker<Generator>::ReadInstrSeq(const Ila& m, const fs::path& file,
                                        std::vector<InstrRef>& dst) {
  if (file.extension() != ".json") {
    return;
  }

  // read in instr name seq
  std::ifstream seq_reader(file);
  json instr_seq_name;
  seq_reader >> instr_seq_name;
  seq_reader.close();

  // find the corresponding instr
  ILA_WARN_IF(!dst.empty()) << "Reading instr. seq. into non-empty container";
  for (const auto& n : instr_seq_name) {
    auto instr = m.instr(n.get<std::string>());
    ILA_ASSERT(instr.get()) << "Cannot find instruction " << n;
    dst.push_back(instr);
  }
}

template <class Generator>
void IsChecker<Generator>::GetTopInstr(const Ila& m,
                                       std::set<std::string>& dst) {
  ILA_WARN_IF(!dst.empty()) << "Getting top instr. into non-empty container";
  for (auto i = 0; i < m.instr_num(); i++) {
    dst.emplace(m.instr(i).name());
  }
}

} // namespace ilang
