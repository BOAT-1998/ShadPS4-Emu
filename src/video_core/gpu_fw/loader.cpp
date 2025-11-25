#include "video_core/gpu_fw/loader.h"

#include <filesystem>
#include <stdexcept>

#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/path_util.h"

namespace VideoCore::GPUFW {

namespace {
std::vector<u8> ReadFileBytes(const std::filesystem::path& path) {
    Common::FS::IOFile file(path, Common::FS::FileAccessMode::Read);
    if (!file.IsOpen()) {
        return {};
    }
    const auto size = file.GetSize();
    std::vector<u8> data(size);
    file.Read(data);
    return data;
}
} // namespace

GpuFirmware Load(const std::filesystem::path& firmware_root) {
    GpuFirmware fw{};
    const auto mc_path = firmware_root / "gpu_fw.bin";
    const auto pm4_path = firmware_root / "pm4_opcode_table.bin";

    fw.microcode = ReadFileBytes(mc_path);
    fw.pm4_table = ReadFileBytes(pm4_path);

    if (fw.microcode.empty() || fw.pm4_table.empty()) {
        LOG_ERROR(Render, "GPU firmware missing (microcode {} bytes, pm4 table {} bytes)",
                  fw.microcode.size(), fw.pm4_table.size());
        throw std::runtime_error("GPU firmware missing");
    }

    LOG_INFO(Render, "Loaded GPU firmware: microcode {} bytes, pm4 table {} bytes",
             fw.microcode.size(), fw.pm4_table.size());
    return fw;
}

} // namespace VideoCore::GPUFW
