#pragma once

#include <string>
#include <vector>

#include "common/types.h"

namespace VideoCore::Shader {

struct DisasmResult {
    std::string text;
};

DisasmResult DisassemblePssl(const std::vector<u8>& binary);

} // namespace VideoCore::Shader
