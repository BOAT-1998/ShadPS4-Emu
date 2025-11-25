#pragma once

#include <filesystem>
#include <vector>

#include "common/types.h"

namespace VideoCore::GPUFW {

struct GpuFirmware {
    std::vector<u8> microcode;
    std::vector<u8> pm4_table;
};

GpuFirmware Load(const std::filesystem::path& firmware_root);

} // namespace VideoCore::GPUFW
