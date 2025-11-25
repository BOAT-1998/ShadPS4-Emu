# DirectX 12 Backend - Build System Integration Guide

## 🎯 Objective
เพิ่ม DirectX 12 renderer เข้า shadPS4Plus build system เพื่อให้สามารถ compile และใช้งานร่วมกับ Vulkan ได้

---

## 📋 Step 1: เพิ่ม Option ใน CMakeLists.txt

### Location: Line 34-36 (หลัง ENABLE_UPDATER)

```cmake
option(ENABLE_QT_GUI "Enable the Qt GUI. If not selected then the emulator uses a minimal SDL-based UI instead" OFF)
option(ENABLE_DISCORD_RPC "Enable the Discord RPC integration" ON)
option(ENABLE_UPDATER "Enables the options to updater" ON)

# ✨ เพิ่มบรรทัดนี้
option(ENABLE_DX12 "Enable DirectX 12 backend (Windows only)" ON)
```

---

## 📋 Step 2: เพิ่ม DirectX 12 Source Files

### Location: หลัง VIDEOOUT_LIB (ประมาณบรรทัด 507)

```cmake
set(VIDEOOUT_LIB src/core/libraries/videoout/buffer.h
                 src/core/libraries/videoout/driver.cpp
                 src/core/libraries/videoout/driver.h
                 src/core/libraries/videoout/video_out.cpp
                 src/core/libraries/videoout/video_out.h
                 src/core/libraries/videoout/videoout_error.h
)

# ✨ เพิ่มส่วนนี้
# DirectX 12 Renderer Backend (Windows only)
if(WIN32 AND ENABLE_DX12)
    set(DX12_RENDERER
        src/video_core/renderer_dx12/d3d12_device.h
        src/video_core/renderer_dx12/d3d12_device.cpp
        src/video_core/renderer_dx12/d3d12_command_list.h
        src/video_core/renderer_dx12/d3d12_command_list.cpp
        src/video_core/renderer_dx12/d3d12_texture.h
        src/video_core/renderer_dx12/d3d12_texture.cpp
        src/video_core/renderer_dx12/d3d12_pipeline.h
        src/video_core/renderer_dx12/d3d12_pipeline.cpp
        src/video_core/renderer_dx12/d3d12_rasterizer.h
        src/video_core/renderer_dx12/d3d12_rasterizer.cpp
        src/video_core/renderer_dx12/liverpool_to_dx12.h
        src/video_core/renderer_dx12/liverpool_to_dx12.cpp
    )
    
    message(STATUS "DirectX 12 backend enabled")
else()
    set(DX12_RENDERER "")
    if(WIN32)
        message(STATUS "DirectX 12 backend disabled (ENABLE_DX12=OFF)")
    else()
        message(STATUS "DirectX 12 backend not available (non-Windows platform)")
    endif()
endif()
```

---

## 📋 Step 3: เพิ่ม Renderer Interface และ Advanced Graphics

### Location: หลัง DX12_RENDERER

```cmake
# Renderer Interface (Abstract layer for Vulkan and DX12)
set(RENDERER_INTERFACE
    src/video_core/renderer_interface.h
)

# Advanced Graphics Settings
set(GRAPHICS_CONFIG
    src/common/config_graphics.cpp
)

# Frame Limiter
set(FRAME_LIMITER
    src/video_core/renderer_vulkan/vk_frame_limiter.h
    src/video_core/renderer_vulkan/vk_frame_limiter.cpp
)
```

---

## 📋 Step 4: เพิ่มไฟล์เข้า shadps4 Target

### Location: ค้นหา `add_executable(shadps4` หรือ `target_sources(shadps4`

```cmake
target_sources(shadps4 PRIVATE
    # ... existing sources ...
    ${COMMON}
    ${CORE}
    ${VIDEOOUT_LIB}
    # ... other libs ...
    
    # ✨ เพิ่มบรรทัดเหล่านี้
    ${RENDERER_INTERFACE}
    ${GRAPHICS_CONFIG}
    ${FRAME_LIMITER}
    ${DX12_RENDERER}
)
```

---

## 📋 Step 5: Link DirectX 12 Libraries (Windows only)

### Location: หา `target_link_libraries(shadps4`

```cmake
target_link_libraries(shadps4 PRIVATE
    # ... existing libraries ...
    Vulkan::Headers
    VulkanMemoryAllocator::VulkanMemoryAllocator
    # ... other libs ...
)

# ✨ เพิ่มส่วนนี้
# DirectX 12 libraries (Windows only)
if(WIN32 AND ENABLE_DX12)
    target_link_libraries(shadps4 PRIVATE
        d3d12.lib
        dxgi.lib
        dxguid.lib
    )
    
    # Define DX12 enabled
    target_compile_definitions(shadps4 PRIVATE ENABLE_DX12)
    
    message(STATUS "Linking DirectX 12 libraries: d3d12.lib, dxgi.lib, dxguid.lib")
endif()
```

---

## 📋 Step 6: เพิ่ม Compile Definitions

### Location: หา `target_compile_definitions`

```cmake
target_compile_definitions(shadps4 PRIVATE
    # ... existing definitions ...
)

# ✨ เพิ่มส่วนนี้
# DirectX 12 support
if(WIN32 AND ENABLE_DX12)
    target_compile_definitions(shadps4 PRIVATE
        ENABLE_DX12=1
        NOMINMAX  # Prevent Windows.h from defining min/max macros
    )
endif()
```

---

## 📋 Step 7: สร้าง Renderer Factory

### File: `src/video_core/renderer_factory.h` (NEW)

```cpp
// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include <memory>

namespace VideoCore {
class Rasterizer;
}

namespace AmdGpu {
class Liverpool;
}

namespace RendererFactory {

/**
 * Create renderer based on configuration
 * @param window_handle Platform-specific window handle
 * @param liverpool PS4 GPU emulation instance
 * @return Rasterizer instance (Vulkan or DirectX 12)
 */
std::unique_ptr<VideoCore::Rasterizer> CreateRenderer(
    void* window_handle,
    AmdGpu::Liverpool* liverpool
);

/**
 * Get available renderer backends
 * @return List of available backends ("Vulkan", "DirectX12")
 */
std::vector<std::string> GetAvailableBackends();

} // namespace RendererFactory
```

### File: `src/video_core/renderer_factory.cpp` (NEW)

```cpp
// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "renderer_factory.h"
#include "common/config.h"
#include "common/logging/log.h"
#include "renderer_vulkan/vk_instance.h"
#include "renderer_vulkan/vk_rasterizer.h"

#ifdef ENABLE_DX12
#include "renderer_dx12/d3d12_device.h"
#include "renderer_dx12/d3d12_rasterizer.h"
#endif

namespace RendererFactory {

std::unique_ptr<VideoCore::Rasterizer> CreateRenderer(
    void* window_handle,
    AmdGpu::Liverpool* liverpool
) {
    // Check if user wants DirectX 12
#ifdef ENABLE_DX12
    const auto backend = Config::getRendererBackend();
    
    if (backend == "DirectX12" || backend == "DX12") {
        LOG_INFO(Render, "Creating DirectX 12 renderer");
        try {
            auto dx12_device = std::make_unique<DX12::Device>(window_handle);
            return std::make_unique<DX12::Rasterizer>(
                std::move(dx12_device),
                liverpool
            );
        } catch (const std::exception& e) {
            LOG_ERROR(Render, "Failed to create DirectX 12 renderer: {}", e.what());
            LOG_WARNING(Render, "Falling back to Vulkan renderer");
        }
    }
#endif

    // Default to Vulkan
    LOG_INFO(Render, "Creating Vulkan renderer");
    auto vulkan_instance = std::make_unique<Vulkan::Instance>(window_handle);
    return std::make_unique<Vulkan::Rasterizer>(
        *vulkan_instance,
        liverpool
    );
}

std::vector<std::string> GetAvailableBackends() {
    std::vector<std::string> backends;
    
    // Vulkan is always available
    backends.push_back("Vulkan");
    
#ifdef ENABLE_DX12
    // DirectX 12 only on Windows
    backends.push_back("DirectX12");
#endif
    
    return backends;
}

} // namespace RendererFactory
```

---

## 📋 Step 8: เพิ่ม Renderer Backend Config

### File: `src/common/config.h` (เพิ่มใน Advanced Graphics Settings section)

```cpp
// Renderer Backend Selection
enum class RendererBackend {
    Vulkan,
    DirectX12,
    Auto
};

std::string getRendererBackend();
void setRendererBackend(const std::string& backend, bool is_game_specific = false);
```

### File: `src/common/config.cpp` (เพิ่มใน static variables)

```cpp
// Renderer Backend
static ConfigEntry<string> rendererBackend("Vulkan");  // Default to Vulkan

std::string getRendererBackend() {
    return rendererBackend.get();
}

void setRendererBackend(const std::string& backend, bool is_game_specific) {
    rendererBackend.set(backend, is_game_specific);
}
```

---

## 📋 Step 9: แก้ไข Platform Initialization

### File: `src/core/platform.cpp` (หรือที่ที่สร้าง renderer)

```cpp
// เดิม
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"

// ใหม่
#include "video_core/renderer_factory.h"

// ใน constructor หรือ Init()
// เดิม:
// vulkan_instance = std::make_unique<Vulkan::Instance>(window_handle);
// rasterizer = std::make_unique<Vulkan::Rasterizer>(*vulkan_instance, liverpool.get());

// ใหม่:
rasterizer = RendererFactory::CreateRenderer(window_handle, liverpool.get());
```

---

## 📋 Step 10: เพิ่ม UI Selection (Qt GUI)

### File: `src/qt_gui/settings_dialog.ui`

เพิ่ม ComboBox ใน Graphics tab:

```xml
<widget class="QComboBox" name="rendererBackendCombo">
    <item>
        <property name="text">
            <string>Vulkan</string>
        </property>
    </item>
    <item>
        <property name="text">
            <string>DirectX 12 (Windows only)</string>
        </property>
    </item>
</widget>
```

### File: `src/qt_gui/settings_dialog.cpp`

```cpp
void SettingsDialog::LoadSettings() {
    // ... existing code ...
    
    // Renderer Backend
    const auto backend = Config::getRendererBackend();
    if (backend == "Vulkan") {
        ui->rendererBackendCombo->setCurrentIndex(0);
    } else if (backend == "DirectX12" || backend == "DX12") {
        ui->rendererBackendCombo->setCurrentIndex(1);
    }
    
    // Disable DX12 option on non-Windows
#ifndef _WIN32
    ui->rendererBackendCombo->model()->item(1)->setEnabled(false);
#endif
}

void SettingsDialog::SaveSettings() {
    // ... existing code ...
    
    // Renderer Backend
    const int backend_index = ui->rendererBackendCombo->currentIndex();
    if (backend_index == 0) {
        Config::setRendererBackend("Vulkan");
    } else if (backend_index == 1) {
        Config::setRendererBackend("DirectX12");
    }
}
```

---

## 🔧 Complete CMakeLists.txt Additions Summary

### Add after line 36:
```cmake
option(ENABLE_DX12 "Enable DirectX 12 backend (Windows only)" ON)
```

### Add after VIDEOOUT_LIB:
```cmake
# DirectX 12 Backend
if(WIN32 AND ENABLE_DX12)
    set(DX12_RENDERER
        src/video_core/renderer_dx12/d3d12_device.h
        src/video_core/renderer_dx12/d3d12_device.cpp
        src/video_core/renderer_dx12/d3d12_command_list.h
        src/video_core/renderer_dx12/d3d12_command_list.cpp
        src/video_core/renderer_dx12/d3d12_texture.h
        src/video_core/renderer_dx12/d3d12_texture.cpp
        src/video_core/renderer_dx12/d3d12_pipeline.h
        src/video_core/renderer_dx12/d3d12_pipeline.cpp
        src/video_core/renderer_dx12/d3d12_rasterizer.h
        src/video_core/renderer_dx12/d3d12_rasterizer.cpp
        src/video_core/renderer_dx12/liverpool_to_dx12.h
        src/video_core/renderer_dx12/liverpool_to_dx12.cpp
    )
    message(STATUS "DirectX 12 backend enabled")
else()
    set(DX12_RENDERER "")
endif()

# Renderer Interface
set(RENDERER_INTERFACE
    src/video_core/renderer_interface.h
    src/video_core/renderer_factory.h
    src/video_core/renderer_factory.cpp
)

# Advanced Graphics
set(GRAPHICS_CONFIG
    src/common/config_graphics.cpp
)

# Frame Limiter
set(FRAME_LIMITER
    src/video_core/renderer_vulkan/vk_frame_limiter.h
    src/video_core/renderer_vulkan/vk_frame_limiter.cpp
)
```

### Add to target_sources:
```cmake
${RENDERER_INTERFACE}
${GRAPHICS_CONFIG}
${FRAME_LIMITER}
${DX12_RENDERER}
```

### Add to target_link_libraries:
```cmake
if(WIN32 AND ENABLE_DX12)
    target_link_libraries(shadps4 PRIVATE
        d3d12.lib
        dxgi.lib
        dxguid.lib
    )
    target_compile_definitions(shadps4 PRIVATE ENABLE_DX12=1 NOMINMAX)
endif()
```

---

## ✅ Build Instructions

### 1. Configure CMake:
```bash
cd build
cmake .. -DENABLE_DX12=ON -DENABLE_QT_GUI=ON
```

### 2. Build:
```bash
cmake --build . --config Release
```

### 3. Verify:
```bash
# Check if DX12 is enabled
cmake -L | grep ENABLE_DX12

# Should output:
# ENABLE_DX12:BOOL=ON
```

---

## 🧪 Testing

### 1. Check Available Backends:
```cpp
auto backends = RendererFactory::GetAvailableBackends();
// Should return: ["Vulkan", "DirectX12"] on Windows
// Should return: ["Vulkan"] on Linux/Mac
```

### 2. Test Backend Selection:
```cpp
// Set to DirectX 12
Config::setRendererBackend("DirectX12");

// Create renderer
auto renderer = RendererFactory::CreateRenderer(window_handle, liverpool);

// Should create DX12::Rasterizer on Windows
// Should fallback to Vulkan::Rasterizer on error
```

### 3. Verify Compilation:
```bash
# Should see in build output:
# -- DirectX 12 backend enabled
# -- Linking DirectX 12 libraries: d3d12.lib, dxgi.lib, dxguid.lib
```

---

## ⚠️ Important Notes

### Platform Support:
- **Windows:** Both Vulkan and DirectX 12
- **Linux:** Vulkan only
- **macOS:** Vulkan only (via MoltenVK)

### Build Requirements:
- **Windows SDK 10.0.19041.0** or later (for DirectX 12)
- **Visual Studio 2022** or **Clang 15+**
- **CMake 3.24** or later

### Runtime Requirements:
- **Windows 10 version 1809** or later
- **DirectX 12 capable GPU**
- **Latest GPU drivers**

---

## 🎯 Summary

### Files to Create:
1. ✅ `src/video_core/renderer_factory.h`
2. ✅ `src/video_core/renderer_factory.cpp`
3. ✅ `src/video_core/renderer_dx12/*.{h,cpp}` (already created)

### Files to Modify:
1. ⏳ `CMakeLists.txt` - Add DX12 sources and libraries
2. ⏳ `src/common/config.h` - Add renderer backend selection
3. ⏳ `src/common/config.cpp` - Implement backend config
4. ⏳ `src/core/platform.cpp` - Use RendererFactory
5. ⏳ `src/qt_gui/settings_dialog.ui` - Add backend ComboBox
6. ⏳ `src/qt_gui/settings_dialog.cpp` - Connect UI to config

### Build System Changes:
- ✅ Add `ENABLE_DX12` option
- ✅ Add DX12 source files
- ✅ Link DirectX 12 libraries
- ✅ Add compile definitions
- ✅ Platform-specific handling

---

**Status: Ready for integration!** 🚀

**Next Steps:**
1. Apply CMakeLists.txt changes
2. Create renderer_factory files
3. Update config system
4. Modify platform initialization
5. Add Qt GUI selection
6. Build and test

**Estimated Time: 2-4 hours**
