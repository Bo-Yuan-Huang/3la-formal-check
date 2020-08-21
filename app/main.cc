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

#include <ilang/ilang++.h>
#include <ilang/target-smt/smt_shim.h>
#include <ilang/target-smt/smt_switch_itf.h>
#include <ilang/target-smt/z3_expr_adapter.h>

#ifdef USE_Z3
#include <z3++.h>
#else
#include <smt-switch/boolector_factory.h>
#include <smt-switch/smt.h>
#endif

#include <pffc/ischecker_flex_relay.h>

using namespace ilang;

int main() {
  EnableDebug("3LA");

  auto data_dir = fs::current_path() / ".." / "data";

#ifdef USE_Z3
  z3::context ctx;
  auto smt_generator = Z3ExprAdapter(ctx);
#else
  auto btor = smt::BoolectorSolverFactory::create(false);
  auto smt_generator = SmtSwitchItf(btor);
#endif

  auto smt_shim = SmtShim(smt_generator);
  auto checker = IsCheckerFlexRelay(smt_shim);

  // instruction sequence to verify
  checker.SetInstrSeq(0, data_dir / "instr_seq_flex_small.json");
  checker.SetInstrSeq(1, data_dir / "instr_seq_relay_small.json");

  // design specific
  checker.SetFlexCmd(data_dir / "prog_frag_flex.json");
  checker.SetRelayCmd(data_dir / "prog_frag_relay.json");
  checker.SetAddrMapping(data_dir / "addr_mapping.json");

  // verify
  checker.Check();

  return 0;
}
