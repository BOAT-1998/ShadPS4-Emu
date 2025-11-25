// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations
namespace VideoCore {
class Rasterizer;
}

namespace AmdGpu {
class Liverpool;
}

namespace RendererFactory {

/**
 * Create renderer instance based on user configuration
 *
 * Automatically selects between Vulkan and DirectX 12 based on:
 * 1. User preference (Config::getRendererBackend())
 * 2. Platform availability (DX12 only on Windows)
 * 3. Fallback to Vulkan if DX12 fails
 *
 * @param window_handle Platform-specific window handle (HWND on Windows)
 * @param liverpool PS4 GPU emulation instance
 * @return Unique pointer to Rasterizer (Vulkan or DirectX 12)
 *
 * @throws std::runtime_error if no renderer can be created
 */
std::unique_ptr<VideoCore::Rasterizer> CreateRenderer(void* window_handle,
                                                      AmdGpu::Liverpool* liverpool);

/**
 * Get list of available renderer backends on current platform
 *
 * @return Vector of backend names ("Vulkan", "DirectX12")
 *
 * Example:
 *   Windows: ["Vulkan", "DirectX12"]
 *   Linux:   ["Vulkan"]
 *   macOS:   ["Vulkan"]
 */
std::vector<std::string> GetAvailableBackends();

/**
 * Check if a specific backend is available
 *
 * @param backend Backend name ("Vulkan" or "DirectX12")
 * @return true if backend is available, false otherwise
 */
bool IsBackendAvailable(const std::string& backend);

/**
 * Get recommended backend for current platform
 *
 * @return Recommended backend name
 *
 * Recommendation logic:
 * - Windows: "DirectX12" (better performance on some GPUs)
 * - Linux/macOS: "Vulkan" (only option)
 */
std::string GetRecommendedBackend();

} // namespace RendererFactory
