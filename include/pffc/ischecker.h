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

// File: ischecker.h

#ifndef PFFC_ISCHECKER_H__
#define PFFC_ISCHECKER_H__

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <ilang/ilang++.h>
#include <z3++.h>

namespace fs = std::filesystem;

namespace ilang {

class IsChecker {
public:
  // constructor and destructor
  IsChecker(const Ila& m0, const Ila& m1);
  ~IsChecker();

  // start checking
  bool Check();

  // specify the instruction sequence (file) of m0/m1
  void SetInstrSeq(const int& idx, const fs::path& file);

private:
  // underlying z3 context
  z3::context ctx_;

  // ILA model to check
  Ila m0_;
  Ila m1_;

  // instruction sequence
  std::vector<InstrRef> m0_instr_seq_;
  std::vector<InstrRef> m1_instr_seq_;

  std::set<std::string> m0_top_instr_;
  std::set<std::string> m1_top_instr_;

  // instruction sequence unroller (for z3)
  IlaZ3Unroller* unroller_ = NULL;

  // preprocessing before checking, e.g., flattening hierarchy
  void Preprocess();

  // helper - read instruction sequence from file
  static void ReadInstrSeq(const Ila& m, const fs::path& file,
                           std::vector<InstrRef>& dst);

  // helper - collect top-level instructions
  static void GetTopInstr(const Ila& m, std::set<std::string>& dst);

}; // class IsChecker

} // namespace ilang

#endif // PFFC_ISCHECKER_H__
