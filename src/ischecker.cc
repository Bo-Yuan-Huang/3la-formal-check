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
#include <ilang/util/log.h>
#include <nlohmann/json.hpp>

#include <pffc/ischecker.h>

using json = nlohmann::json;

namespace ilang {

IsChecker::IsChecker(const Ila& m0, const Ila& m1) : m0_(m0), m1_(m1) {
  unroller_m0_ = new IlaZ3Unroller(ctx_);
  unroller_m1_ = new IlaZ3Unroller(ctx_);
  Preprocess();
}

IsChecker::~IsChecker() {
  if (unroller_m0_) {
    delete unroller_m0_;
    unroller_m0_ = NULL;
  }

  if (unroller_m1_) {
    delete unroller_m1_;
    unroller_m1_ = NULL;
  }
}

bool IsChecker::Check() {
  // make sure the sequence has been specified
  if (instr_seq_m0_.empty() or instr_seq_m1_.empty()) {
    ILA_ERROR << "Instruction sequence not set";
    return false;
  }

  // unroll the program and get the z3 expression
  ILA_NOT_NULL(unroller_m0_);
  ILA_NOT_NULL(unroller_m1_);

  // add design specific constraints
  AddEnvM0();
  AddEnvM1();

  // unroll two instruction sequences
#if 0
  auto is0 = unroller_m0_->UnrollPathConn(instr_seq_m0_);
  auto is1 = unroller_m1_->UnrollPathConn(instr_seq_m1_);
#else
  auto is0 = unroller_m0_->UnrollPathSubs(instr_seq_m0_);
  auto is1 = unroller_m1_->UnrollPathSubs(instr_seq_m1_);
#endif

  // miter
  auto miter = GetMiter();

  // func
  auto uninterp_func = GetUninterpFunc();

  // start solving
  ILA_INFO << "Start solving";
  z3::solver solver(ctx_);
  solver.add(is0);
  solver.add(is1);
  solver.add(miter);
  solver.add(uninterp_func);

  auto res = solver.check();
  ILA_INFO << "Result: " << res;

  if (res == z3::sat) {
    auto model = solver.get_model();
    Debug(model);
  }

  return res == z3::unsat;
}

void IsChecker::SetInstrSeq(const int& idx, const fs::path& file) {
  ILA_ASSERT(fs::is_regular_file(file)) << file;
  if (idx == 0) {
    ReadInstrSeq(m0_, file, instr_seq_m0_);
  } else {
    ReadInstrSeq(m1_, file, instr_seq_m1_);
  }
}

void IsChecker::Preprocess() {
  // bookkeeping top-level instructions
  GetTopInstr(m0_, top_instr_m0_);
  GetTopInstr(m1_, top_instr_m1_);

  // flatten hierarchy
  m0_.FlattenHierarchy();
  m1_.FlattenHierarchy();
}

void IsChecker::ReadInstrSeq(const Ila& m, const fs::path& file,
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

void IsChecker::GetTopInstr(const Ila& m, std::set<std::string>& dst) {
  ILA_WARN_IF(!dst.empty()) << "Getting top instr. into non-empty container";
  for (auto i = 0; i < m.instr_num(); i++) {
    dst.emplace(m.instr(i).name());
  }
}

} // namespace ilang
