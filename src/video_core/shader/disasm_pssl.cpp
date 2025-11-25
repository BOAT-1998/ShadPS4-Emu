#include "video_core/shader/disasm_pssl.h"

#include <string>

#include "common/logging/log.h"

namespace VideoCore::Shader {

DisasmResult DisassemblePssl(const std::vector<u8>& binary) {
    // Placeholder pretty-printer: in future, decode using OpenOrbis metadata and firmware tables.
    DisasmResult out{};
    out.text = "PSSL shader (" + std::to_string(binary.size()) + " bytes)\n";
    LOG_DEBUG(Render_Recompiler, "Disassembled PSSL shader size {}", binary.size());
    return out;
}

} // namespace VideoCore::Shader
