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

// File: main.cc

#include <ilang/util/log.h>

#include <flex/interface.h>
#include <relay/interface.h>

#include <pffc/ischecker.h>

using namespace ilang;

int main() {

  auto data_dir = fs::current_path() / ".." / "data";

  auto flex_ila = flex::GetFlexIla("flex");
  auto relay_ila = relay::GetRelayIla("relay");

  auto checker = IsChecker(flex_ila, relay_ila);
  checker.SetInstrSeq(0, data_dir / "instr_seq_flex_small.json");
  checker.SetInstrSeq(1, data_dir / "instr_seq_relay_small.json");

  auto res = checker.Check();
  ILA_INFO << "Result: " << res;

  return 0;
}
