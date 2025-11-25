#include "common/logging/log.h"
#include "common/types.h"
#include "pm4_decoder_enhanced.h"
#include "pm4_opcodes.h"

namespace VideoCore::AMD {

bool Pm4DecoderEnhanced::Decode(const u32* words, size_t dword_count) {
    packets.clear();
    size_t i = 0;
    while (i < dword_count) {
        const u32 header = words[i];
        const u32 opcode = (header >> 8) & 0xFF;
        const u32 len = (header & 0x3FFF) + 1; // count includes header

        if (len == 0 || i + len > dword_count) {
            LOG_ERROR(Render, "PM4 decode overrun at {} (len {}, total {})", i, len, dword_count);
            return false;
        }

        Pm4Packet pkt{};
        pkt.opcode = opcode;
        pkt.words.assign(words + i, words + i + len);
        packets.push_back(std::move(pkt));

        // Basic handling for specific packets can be added here
        switch (static_cast<AmdGpu::PM4ItOpcode>(opcode)) {
        case AmdGpu::PM4ItOpcode::SetContextReg:
            // Handle context register set
            break;
        case AmdGpu::PM4ItOpcode::DrawIndexAuto:
            // Handle draw index auto
            break;
        }

        i += len;
    }
    return true;
}

} // namespace VideoCore::AMD
