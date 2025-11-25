# DirectX 12 Backend Integration Plan for shadPS4Plus

## สถานะปัจจุบัน ✅
เราได้สร้าง skeleton สำหรับ DirectX 12 backend แล้ว ประกอบด้วย:

1. **Abstract Interface Layer** (`renderer_interface.h`)
   - `IGpuDevice`, `ICommandList`, `ITexture`, `IBuffer`, `IPipelineState`
   - Interface ที่ทั้ง Vulkan และ DX12 สามารถ implement ได้

2. **DirectX 12 Core Files**
   - `d3d12_device.{h,cpp}` - Device, command queues, swapchain, descriptor heaps
   - `d3d12_command_list.{h,cpp}` - Command recording and submission
   - `d3d12_texture.h` - Texture resource management
   - `d3d12_pipeline.h` - Pipeline state objects

3. **Documentation**
   - `README.md` - ความแตกต่างระหว่าง Vulkan และ DX12, การ map PS4 GNM

## ขั้นตอนการ Integration (ต่อจากนี้)

### Phase 1: เพิ่ม Configuration System
**ไฟล์ที่ต้องแก้:**
- `src/common/config.h` - เพิ่ม function declarations
- `src/common/config.cpp` - เพิ่ม implementation

**เพิ่ม:**
```cpp
// ใน config.h
enum class RendererBackend {
    Vulkan,
    DirectX12,
    Auto  // เลือกอัตโนมัติตาม platform
};

RendererBackend getRendererBackend();
void setRendererBackend(RendererBackend backend, bool is_game_specific = false);

// ใน config.cpp
static ConfigEntry<string> rendererBackend("Vulkan");

RendererBackend getRendererBackend() {
    const auto& backend = rendererBackend.get();
    if (backend == "DirectX12" || backend == "DX12") {
        return RendererBackend::DirectX12;
    }
    return RendererBackend::Vulkan;
}

void setRendererBackend(RendererBackend backend, bool is_game_specific) {
    switch (backend) {
    case RendererBackend::Vulkan:
        rendererBackend.set("Vulkan", is_game_specific);
        break;
    case RendererBackend::DirectX12:
        rendererBackend.set("DirectX12", is_game_specific);
        break;
    case RendererBackend::Auto:
#ifdef _WIN32
        rendererBackend.set("DirectX12", is_game_specific);
#else
        rendererBackend.set("Vulkan", is_game_specific);
#endif
        break;
    }
}
```

### Phase 2: สร้าง Renderer Factory
**ไฟล์ใหม่:** `src/video_core/renderer_factory.h`

```cpp
#pragma once

#include "video_core/renderer_interface.h"
#include "common/config.h"
#include <memory>

namespace VideoCore {

class RendererFactory {
public:
    static std::unique_ptr<IRasterizer> CreateRasterizer(
        void* window_handle,
        AmdGpu::Liverpool* liverpool
    ) {
        const auto backend = Config::getRendererBackend();
        
        switch (backend) {
        case Config::RendererBackend::Vulkan:
            return CreateVulkanRasterizer(window_handle, liverpool);
            
        case Config::RendererBackend::DirectX12:
#ifdef _WIN32
            return CreateDX12Rasterizer(window_handle, liverpool);
#else
            LOG_ERROR(Render, "DirectX 12 is only available on Windows");
            return CreateVulkanRasterizer(window_handle, liverpool);
#endif
            
        default:
            return CreateVulkanRasterizer(window_handle, liverpool);
        }
    }

private:
    static std::unique_ptr<IRasterizer> CreateVulkanRasterizer(
        void* window_handle,
        AmdGpu::Liverpool* liverpool
    );
    
    static std::unique_ptr<IRasterizer> CreateDX12Rasterizer(
        void* window_handle,
        AmdGpu::Liverpool* liverpool
    );
};

} // namespace VideoCore
```

### Phase 3: Implement Missing Components

#### 3.1 DirectX 12 Rasterizer
**ไฟล์ใหม่:** `src/video_core/renderer_dx12/d3d12_rasterizer.{h,cpp}`

ต้อง implement:
- `class DX12Rasterizer : public VideoCore::IRasterizer`
- ทุก method ที่ abstract interface กำหนด
- ใช้ `vk_rasterizer.cpp` เป็น reference

#### 3.2 Liverpool to DirectX 12 Converter
**ไฟล์ใหม่:** `src/video_core/renderer_dx12/liverpool_to_dx12.{h,cpp}`

แปลง PS4 GNM state → DirectX 12 state:
```cpp
namespace LiverpoolToDX12 {
    DXGI_FORMAT DataFormat(AmdGpu::DataFormat format, AmdGpu::NumberFormat num_format);
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType(AmdGpu::PrimitiveType type);
    D3D12_BLEND BlendFactor(AmdGpu::BlendControl::BlendFactor factor);
    D3D12_COMPARISON_FUNC DepthCompare(AmdGpu::DepthControl::DepthCompare compare);
    // ... และอื่นๆ
}
```

#### 3.3 Shader Compiler Integration
**ต้องการ:** SPIR-V → DXIL compiler

**ตัวเลือก:**
1. **SPIRV-Cross** → HLSL → DXC (DirectX Shader Compiler)
2. **spirv-to-dxil** (Microsoft's direct converter)

**ไฟล์ใหม่:** `src/video_core/renderer_dx12/d3d12_shader_compiler.{h,cpp}`

```cpp
class ShaderCompiler {
public:
    // แปลง SPIR-V → DXIL
    ComPtr<ID3DBlob> CompileToDXIL(
        const std::vector<u32>& spirv,
        Shader::Stage stage
    );
    
private:
    // ใช้ DXC compiler
    ComPtr<IDxcCompiler3> dxc_compiler;
    ComPtr<IDxcUtils> dxc_utils;
};
```

### Phase 4: Integration Points

#### 4.1 แก้ไข Platform Initialization
**ไฟล์:** `src/core/platform.cpp`

```cpp
// เดิม
#include "video_core/renderer_vulkan/vk_instance.h"

// ใหม่
#include "video_core/renderer_factory.h"

// ใน Platform::Platform()
rasterizer = VideoCore::RendererFactory::CreateRasterizer(
    window_handle,
    liverpool.get()
);
```

#### 4.2 เพิ่ม UI Settings
**ไฟล์:** `src/qt_gui/settings_dialog.cpp`

เพิ่ม dropdown สำหรับเลือก renderer:
```cpp
QComboBox* rendererComboBox = new QComboBox();
rendererComboBox->addItem("Vulkan");
#ifdef _WIN32
rendererComboBox->addItem("DirectX 12");
#endif
```

### Phase 5: Testing & Validation

#### 5.1 Basic Tests
1. **Clear Screen Test** - ทดสอบ clear color
2. **Triangle Test** - render triangle เดียว
3. **Texture Test** - textured quad

#### 5.2 Game Tests
1. เริ่มจากเกม 2D ง่ายๆ
2. ทดสอบเกม 3D
3. เปรียบเทียบผลลัพธ์กับ Vulkan

#### 5.3 Performance Comparison
- Frame time
- CPU usage
- Memory usage
- Shader compilation time

## Implementation Files Summary

### ไฟล์ที่สร้างแล้ว ✅
```
src/video_core/
├── renderer_interface.h              ✅ Abstract interface
└── renderer_dx12/
    ├── README.md                      ✅ Documentation
    ├── d3d12_device.h                 ✅ Device header
    ├── d3d12_device.cpp               ✅ Device implementation
    ├── d3d12_command_list.h           ✅ Command list header
    ├── d3d12_command_list.cpp         ✅ Command list implementation
    ├── d3d12_texture.h                ✅ Texture header
    └── d3d12_pipeline.h               ✅ Pipeline header
```

### ไฟล์ที่ต้องสร้าง ⏳
```
src/video_core/
├── renderer_factory.h                 ⏳ Renderer factory
├── renderer_factory.cpp               ⏳
└── renderer_dx12/
    ├── d3d12_rasterizer.h             ⏳ Main rasterizer
    ├── d3d12_rasterizer.cpp           ⏳
    ├── d3d12_texture.cpp              ⏳ Texture implementation
    ├── d3d12_pipeline.cpp             ⏳ Pipeline implementation
    ├── d3d12_shader_compiler.h        ⏳ Shader compiler
    ├── d3d12_shader_compiler.cpp      ⏳
    ├── liverpool_to_dx12.h            ⏳ State converter
    └── liverpool_to_dx12.cpp          ⏳
```

### ไฟล์ที่ต้องแก้ไข 🔧
```
src/common/
├── config.h                           🔧 Add renderer backend config
└── config.cpp                         🔧

src/core/
└── platform.cpp                       🔧 Use renderer factory

src/qt_gui/
└── settings_dialog.cpp                🔧 Add renderer selection UI
```

## Build System Changes

### CMakeLists.txt
```cmake
# Add DirectX 12 option
option(ENABLE_DX12 "Enable DirectX 12 backend" ON)

if(ENABLE_DX12 AND WIN32)
    add_definitions(-DENABLE_DX12)
    
    # DirectX 12 sources
    set(DX12_SOURCES
        src/video_core/renderer_dx12/d3d12_device.cpp
        src/video_core/renderer_dx12/d3d12_command_list.cpp
        src/video_core/renderer_dx12/d3d12_texture.cpp
        src/video_core/renderer_dx12/d3d12_pipeline.cpp
        src/video_core/renderer_dx12/d3d12_rasterizer.cpp
        src/video_core/renderer_dx12/d3d12_shader_compiler.cpp
        src/video_core/renderer_dx12/liverpool_to_dx12.cpp
    )
    
    target_sources(shadps4 PRIVATE ${DX12_SOURCES})
    
    # Link DirectX 12 libraries
    target_link_libraries(shadps4 PRIVATE
        d3d12.lib
        dxgi.lib
        dxguid.lib
        dxcompiler.lib  # For shader compilation
    )
endif()
```

## Estimated Effort

| Task | Complexity | Time Estimate |
|------|------------|---------------|
| Config System | Low | 2-4 hours |
| Renderer Factory | Low | 2-3 hours |
| DX12 Rasterizer | High | 20-30 hours |
| Liverpool→DX12 Converter | Medium | 10-15 hours |
| Shader Compiler | High | 15-20 hours |
| Testing & Debug | High | 20-40 hours |
| **Total** | | **69-112 hours** |

## Next Steps (Priority Order)

1. ✅ **DONE:** Create abstract interface and DX12 skeleton
2. 🔄 **IN PROGRESS:** Fix black screen issue (predication)
3. ⏭️ **NEXT:** Add config system for renderer selection
4. ⏭️ Create renderer factory
5. ⏭️ Implement DX12 rasterizer
6. ⏭️ Add shader compilation pipeline
7. ⏭️ Test with simple games

## Notes

- DirectX 12 จะทำงานได้เฉพาะ Windows เท่านั้น
- ต้องการ Windows 10 version 1809 ขึ้นไป
- ต้องมี GPU ที่รองรับ DirectX 12
- Shader compilation จะช้ากว่า Vulkan ในครั้งแรก (ต้อง cache)
- Performance อาจแตกต่างกันขึ้นอยู่กับ driver

## Conclusion

DirectX 12 backend skeleton พร้อมใช้งานแล้ว แต่ต้องการการ implement เพิ่มเติมอีกหลายส่วนเพื่อให้ทำงานได้จริง การทำงานทั้งหมดประมาณ 70-112 ชั่วโมง ขึ้นอยู่กับความซับซ้อนของ shader compilation และ debugging
