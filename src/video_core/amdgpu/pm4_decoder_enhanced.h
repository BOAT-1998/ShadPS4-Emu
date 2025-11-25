#pragma once

#include <vector>
#include "common/types.h"

namespace VideoCore { namespace AMD {

struct Pm4Packet {
    u32 opcode{};
    std::vector<u32> words;
};

class Pm4DecoderEnhanced {
public:
    bool Decode(const u32* words, size_t dword_count);
    const std::vector<Pm4Packet>& Packets() const {
        return packets;
    }

private:
    std::vector<Pm4Packet> packets;
};

} } // namespace VideoCore::AMD
