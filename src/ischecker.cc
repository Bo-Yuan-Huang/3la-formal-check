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

#include <csv.hpp>
#include <ilang/util/log.h>
#include <nlohmann/json.hpp>

#include <pffc/ischecker.h>

using json = nlohmann::json;

namespace ilang {

IsChecker::IsChecker(const Ila& m0, const Ila& m1) : m0_(m0), m1_(m1) {
  unroller_ = new IlaZ3Unroller(ctx_);
  Preprocess();
}

IsChecker::~IsChecker() {
  if (unroller_) {
    delete unroller_;
    unroller_ = NULL;
  }
}

bool IsChecker::Check() {
  // make sure the sequence has been specified
  if (m0_instr_seq_.empty() or m1_instr_seq_.empty()) {
    ILA_ERROR << "Instruction sequence not set";
    return false;
  }

  // unroll the program and get the z3 expression
  ILA_NOT_NULL(unroller_);

  // TODO constraining inputs of parent instructions

  auto is0 = unroller_->UnrollPathConn(m0_instr_seq_);
  auto is1 = unroller_->UnrollPathConn(m1_instr_seq_);

  // TODO check equivalence (miter unsat)

  z3::solver solver(ctx_);
  solver.add(is0);
  solver.add(is1);

  auto res = solver.check();

  return res == z3::unsat;
}

void IsChecker::SetInstrSeq(const int& idx, const fs::path& file) {
  ILA_ASSERT(fs::is_regular_file(file)) << file;
  if (idx == 0) {
    ReadInstrSeq(m0_, file, m0_instr_seq_);
  } else {
    ReadInstrSeq(m1_, file, m1_instr_seq_);
  }
}

void IsChecker::Preprocess() {
  // bookkeeping top-level instructions
  GetTopInstr(m0_, m0_top_instr_);
  GetTopInstr(m1_, m1_top_instr_);

  // flatten hierarchy
  m0_.FlattenHierarchy();
  m1_.FlattenHierarchy();
}

void IsChecker::ReadInstrSeq(const Ila& m, const fs::path& file,
                             std::vector<InstrRef>& dst) {
  if (file.extension() != ".json") {
    return;
  }

  std::ifstream seq_reader(file);
  json instr_seq_name;
  seq_reader >> instr_seq_name;
  seq_reader.close();

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

#if 0
void IsChecker::ReadInstrCmd() {
  csv::CSVReader cmd_reader(instr_cmd_file);

  for (auto& row : cmd_reader) {
    for (auto& field : row) {
      ILA_INFO << field.get<>();
    }
  }
}
#endif

} // namespace ilang
