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

// File: ischecker_flex_relay.h

#ifndef PFFC_ISCHECKER_FLEX_RELAY_H__
#define PFFC_ISCHECKER_FLEX_RELAY_H__

#include <tuple>

#include <flex/interface.h>
#include <relay/interface.h>

#include <pffc/ischecker.h>

namespace ilang {

template <class Generator>
class IsCheckerFlexRelay : public IsChecker<Generator> {
public:
  IsCheckerFlexRelay(SmtShim<Generator>& gen)
      : IsChecker<Generator>(flex::GetFlexIla(), relay::GetRelayIla(), gen) {}

  void SetFlexCmd(const fs::path& cmd_file);
  void SetRelayCmd(const fs::path& cmd_file);
  void SetAddrMapping(const fs::path& mapping);

protected:
  void AddEnvM0();
  void AddEnvM1();
  typename IsChecker<Generator>::SmtExpr GetMiter();
  typename IsChecker<Generator>::SmtExpr GetUninterpFunc();
#ifdef USE_Z3
  void Debug(z3::model& model);
#endif

private:
  typedef std::map<std::string, unsigned long long> CmdType;

  static const std::vector<std::string> k_flex_in_data;

  std::vector<CmdType> cmd_seq_flex_;
  std::vector<CmdType> cmd_seq_relay_;
  std::map<size_t, size_t> addr_mapping_;
  std::map<size_t, size_t> store_flex_;
  std::map<size_t, size_t> store_relay_;

  ExprRef FilterFlexCmd(const std::string& name, size_t cmd_idx);
  ExprRef FilterRelayCmd(const std::string& name, size_t cmd_idx);

  // helper - remove "0x" prefix if exist
  inline std::string RemoveHexPrefix(const std::string& org) {
    if (org.size() <= 2) {
      return org;
    }
    auto prefix = org.substr(0, 2);
    return (prefix == "0x") ? org.substr(2, org.size() - 2) : org;
  }

}; // IsCheckerFlexRelay

} // namespace ilang

#endif // PFFC_ISCHECKER_FLEX_RELAY_H__
