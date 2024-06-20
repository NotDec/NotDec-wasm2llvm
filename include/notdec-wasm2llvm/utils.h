#ifndef _NOTDEC_WASM2LLVM_UTILS_H_
#define _NOTDEC_WASM2LLVM_UTILS_H_

#include <cstdint>
#include <cstring>
#include <sstream>

template <typename T> static std::string int_to_hex(T i) {
  std::stringstream stream;
  stream << "0x" << std::hex << i;
  return stream.str();
}

namespace notdec::frontend::wasm {

float ieee_float(uint32_t f);
double ieee_double(uint64_t f);

extern const char *MEM_NAME;

enum log_level {
  level_emergent = 0,
  level_alert = 1,
  level_critical = 2,
  level_error = 3,
  level_warning = 4,
  level_notice = 5,
  level_info = 6,
  level_debug = 7
};

} // namespace notdec::frontend::wasm

#endif
