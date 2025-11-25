# Advanced Graphics Settings Implementation Guide

## ✅ Phase 1: Configuration System (COMPLETED)

### Files Modified:
1. **src/common/config.h** - Added all declarations
2. **src/common/config.cpp** - Added ConfigEntry variables
3. **src/common/config_graphics.cpp** - Added getter/setter implementations

### Settings Added:

#### 1. Resolution Scaling
- **Values:** 50%, 75%, 100%, 150%, 200%, 300%, 400%
- **Config:** `resolutionScale` (int, default: 100)
- **Usage:** Multiply render target dimensions by scale/100

#### 2. Anisotropic Filtering
- **Values:** Off (1x), 2x, 4x, 8x, 16x
- **Config:** `anisotropicFiltering` (int, default: 16)
- **Usage:** Set `maxAnisotropy` in VkSamplerCreateInfo

#### 3. Anti-Aliasing
- **Values:** None, FXAA, SMAA, TAA
- **Config:** `antiAliasing` (string, default: "None")
- **Usage:** Apply post-processing shader pass

#### 4. Texture Quality
- **Values:** Low, Medium, High, Ultra
- **Config:** `textureQuality` (string, default: "High")
- **Usage:** Adjust mipmap levels and filtering

#### 5. Upscaler
- **Values:** None, FSR1, FSR2, NIS
- **Config:** `upscaler` (string, default: "None")
- **Sharpness:** 0-100 (default: 50)
- **Usage:** Apply upscaling shader before final present

#### 6. Frame Limiter
- **Values:** 0 (Unlimited), 30, 45, 60, 120, 144, 240 FPS
- **Config:** `frameLimit` (int, default: 0)
- **Usage:** Sleep/wait to match target frame time

#### 7. VSync Mode
- **Values:** FIFO, Mailbox, Immediate, FIFORelaxed
- **Config:** `vsyncMode` (string, default: "Mailbox")
- **Usage:** Set `presentMode` in VkSwapchainCreateInfoKHR

#### 8. HDR
- **Config:** `hdrEnabled` (bool, default: false)
- **Usage:** Use `VK_FORMAT_A2B10G10R10_UNORM_PACK32` swapchain format

#### 9. Vulkan Advanced Options
- **Pipeline Cache:** `vkPipelineCache` (bool, default: true)
- **Pipeline Prefetch:** `vkPipelinePrefetch` (bool, default: true)
- **Descriptor Indexing:** `vkDescriptorIndexing` (bool, default: true)
- **Dynamic Rendering:** `vkDynamicRendering` (bool, default: false)
- **GPU Timestamps:** `vkGPUTimestamps` (bool, default: false)
- **Multithreaded Cmd:** `vkMultithreadedCmd` (bool, default: true)

#### 10. Shader Compilation
- **Preload Cache:** `shaderPreloadCache` (bool, default: true)
- **Async Compile:** `asyncShaderCompile` (bool, default: true)
- **Skip Slow Barriers:** `skipSlowBarriers` (bool, default: false)
- **Debug Mode:** `shaderDebugMode` (bool, default: false)

#### 11. CPU/GPU Timing
- **Values:** Normal, HighPrecision, FastMode
- **Config:** `timingMode` (string, default: "Normal")

---

## ⏳ Phase 2: Vulkan Renderer Integration (TODO)

### Files to Modify:

#### 1. **src/video_core/renderer_vulkan/vk_instance.h**
Add member variables:
```cpp
class Instance {
    // ... existing members ...
    
    // Advanced graphics settings
    float resolution_scale = 1.0f;
    u32 max_anisotropy = 16;
    bool hdr_enabled = false;
    // ...
};
```

#### 2. **src/video_core/renderer_vulkan/vk_swapchain.cpp**
Modify swapchain creation:
```cpp
void Swapchain::Create(...) {
    // Get HDR setting
    const bool hdr = Config::getHDREnabled();
    
    // Select format
    const vk::Format format = hdr 
        ? vk::Format::eA2B10G10R10UnormPack32 
        : vk::Format::eB8G8R8A8Unorm;
    
    // Get VSync mode
    const auto vsync_mode = Config::getVSyncMode();
    vk::PresentModeKHR present_mode;
    if (vsync_mode == "FIFO") {
        present_mode = vk::PresentModeKHR::eFifo;
    } else if (vsync_mode == "Mailbox") {
        present_mode = vk::PresentModeKHR::eMailbox;
    } else if (vsync_mode == "Immediate") {
        present_mode = vk::PresentModeKHR::eImmediate;
    } else if (vsync_mode == "FIFORelaxed") {
        present_mode = vk::PresentModeKHR::eFifoRelaxed;
    }
    
    // Create swapchain with selected settings
    vk::SwapchainCreateInfoKHR create_info{
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = format,
        .presentMode = present_mode,
        // ...
    };
}
```

#### 3. **src/video_core/renderer_vulkan/vk_presenter.cpp**
Add resolution scaling:
```cpp
void Presenter::Present(...) {
    // Get resolution scale
    const int scale = Config::getResolutionScale();
    const float scale_factor = scale / 100.0f;
    
    // Calculate scaled dimensions
    const u32 scaled_width = static_cast<u32>(base_width * scale_factor);
    const u32 scaled_height = static_cast<u32>(base_height * scale_factor);
    
    // Create render targets with scaled size
    // ...
    
    // Apply upscaler if enabled
    const auto upscaler = Config::getUpscaler();
    if (upscaler == "FSR1") {
        ApplyFSR1(scaled_width, scaled_height, output_width, output_height);
    } else if (upscaler == "FSR2") {
        ApplyFSR2(scaled_width, scaled_height, output_width, output_height);
    } else if (upscaler == "NIS") {
        ApplyNIS(scaled_width, scaled_height, output_width, output_height);
    }
    
    // Apply anti-aliasing
    const auto aa = Config::getAntiAliasing();
    if (aa == "FXAA") {
        ApplyFXAA();
    } else if (aa == "SMAA") {
        ApplySMAA();
    } else if (aa == "TAA") {
        ApplyTAA();
    }
}
```

#### 4. **src/video_core/texture_cache/texture_cache.cpp**
Add anisotropic filtering:
```cpp
vk::Sampler TextureCache::GetSampler(...) {
    const u32 max_aniso = Config::getAnisotropicFiltering();
    
    vk::SamplerCreateInfo sampler_info{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .anisotropyEnable = max_aniso > 1,
        .maxAnisotropy = static_cast<float>(max_aniso),
        // ...
    };
    
    return device.createSampler(sampler_info);
}
```

#### 5. **src/video_core/renderer_vulkan/vk_scheduler.cpp**
Add frame limiting:
```cpp
void Scheduler::Present() {
    const int target_fps = Config::getFrameLimit();
    
    if (target_fps > 0) {
        const auto target_frame_time = std::chrono::microseconds(1000000 / target_fps);
        const auto current_time = std::chrono::high_resolution_clock::now();
        const auto elapsed = current_time - last_frame_time;
        
        if (elapsed < target_frame_time) {
            const auto sleep_time = target_frame_time - elapsed;
            std::this_thread::sleep_for(sleep_time);
        }
        
        last_frame_time = std::chrono::high_resolution_clock::now();
    }
    
    // Present frame
    // ...
}
```

#### 6. **src/video_core/host_shaders/** (New Files)
Create post-processing shaders:

**fxaa.frag** - FXAA implementation
**smaa.frag** - SMAA implementation  
**taa.frag** - TAA implementation
**fsr1.comp** - AMD FSR 1.0 compute shader
**fsr2.comp** - AMD FSR 2.0 compute shader
**nis.comp** - NVIDIA NIS compute shader

---

## ⏳ Phase 3: Qt GUI Integration (TODO)

### Files to Modify:

#### 1. **src/qt_gui/settings_dialog.ui**
Add new tab "Advanced Graphics":
```xml
<widget class="QTabWidget">
    <widget class="QWidget" name="advancedGraphicsTab">
        <layout class="QVBoxLayout">
            <!-- Resolution Scale -->
            <widget class="QGroupBox" name="resolutionGroup">
                <property name="title">
                    <string>Resolution Scaling</string>
                </property>
                <layout class="QFormLayout">
                    <item row="0" column="0">
                        <widget class="QLabel">
                            <property name="text">
                                <string>Internal Resolution:</string>
                            </property>
                        </widget>
                    </item>
                    <item row="0" column="1">
                        <widget class="QComboBox" name="resolutionScaleCombo">
                            <item><property name="text"><string>50%</string></property></item>
                            <item><property name="text"><string>75%</string></property></item>
                            <item><property name="text"><string>100% (Native)</string></property></item>
                            <item><property name="text"><string>150%</string></property></item>
                            <item><property name="text"><string>200%</string></property></item>
                            <item><property name="text"><string>300%</string></property></item>
                            <item><property name="text"><string>400%</string></property></item>
                        </widget>
                    </item>
                </layout>
            </widget>
            
            <!-- Anisotropic Filtering -->
            <widget class="QGroupBox" name="filteringGroup">
                <property name="title">
                    <string>Texture Filtering</string>
                </property>
                <layout class="QFormLayout">
                    <item row="0" column="0">
                        <widget class="QLabel">
                            <property name="text">
                                <string>Anisotropic Filtering:</string>
                            </property>
                        </widget>
                    </item>
                    <item row="0" column="1">
                        <widget class="QComboBox" name="anisotropicFilteringCombo">
                            <item><property name="text"><string>Off</string></property></item>
                            <item><property name="text"><string>2x</string></property></item>
                            <item><property name="text"><string>4x</string></property></item>
                            <item><property name="text"><string>8x</string></property></item>
                            <item><property name="text"><string>16x</string></property></item>
                        </widget>
                    </item>
                </layout>
            </widget>
            
            <!-- More groups... -->
        </layout>
    </widget>
</widget>
```

#### 2. **src/qt_gui/settings_dialog.cpp**
Connect UI to config:
```cpp
void SettingsDialog::LoadSettings() {
    // Resolution Scale
    const int scale = Config::getResolutionScale();
    int scale_index = 2; // Default to 100%
    switch (scale) {
        case 50: scale_index = 0; break;
        case 75: scale_index = 1; break;
        case 100: scale_index = 2; break;
        case 150: scale_index = 3; break;
        case 200: scale_index = 4; break;
        case 300: scale_index = 5; break;
        case 400: scale_index = 6; break;
    }
    ui->resolutionScaleCombo->setCurrentIndex(scale_index);
    
    // Anisotropic Filtering
    const int aniso = Config::getAnisotropicFiltering();
    int aniso_index = 4; // Default to 16x
    switch (aniso) {
        case 1: aniso_index = 0; break;
        case 2: aniso_index = 1; break;
        case 4: aniso_index = 2; break;
        case 8: aniso_index = 3; break;
        case 16: aniso_index = 4; break;
    }
    ui->anisotropicFilteringCombo->setCurrentIndex(aniso_index);
    
    // ... load other settings ...
}

void SettingsDialog::SaveSettings() {
    // Resolution Scale
    const int scale_values[] = {50, 75, 100, 150, 200, 300, 400};
    const int scale_index = ui->resolutionScaleCombo->currentIndex();
    Config::setResolutionScale(scale_values[scale_index]);
    
    // Anisotropic Filtering
    const int aniso_values[] = {1, 2, 4, 8, 16};
    const int aniso_index = ui->anisotropicFilteringCombo->currentIndex();
    Config::setAnisotropicFiltering(aniso_values[aniso_index]);
    
    // ... save other settings ...
}
```

---

## 📋 Implementation Checklist

### Phase 1: Configuration ✅
- [x] Add config declarations in config.h
- [x] Add config variables in config.cpp
- [x] Implement getter/setter functions
- [x] Add validation and clamping

### Phase 2: Vulkan Integration ⏳
- [ ] Modify vk_swapchain for HDR and VSync
- [ ] Add resolution scaling to vk_presenter
- [ ] Implement anisotropic filtering in texture_cache
- [ ] Add frame limiter to vk_scheduler
- [ ] Create post-processing shader passes
- [ ] Implement upscaling shaders (FSR1, FSR2, NIS)
- [ ] Add anti-aliasing passes (FXAA, SMAA, TAA)

### Phase 3: Qt GUI ⏳
- [ ] Design settings_dialog.ui layout
- [ ] Connect UI controls to config
- [ ] Add tooltips and descriptions
- [ ] Implement live preview (if possible)
- [ ] Add "Apply" and "Reset to Default" buttons

### Phase 4: Testing ⏳
- [ ] Test each setting individually
- [ ] Test setting combinations
- [ ] Verify config save/load
- [ ] Test performance impact
- [ ] Validate with different games

---

## 🎯 Next Steps

1. **Implement Vulkan Integration** (Highest Priority)
   - Start with resolution scaling
   - Add VSync mode selection
   - Implement anisotropic filtering

2. **Create Qt GUI**
   - Design clean, organized layout
   - Add helpful tooltips
   - Group related settings

3. **Add Post-Processing Shaders**
   - FXAA (simplest, start here)
   - SMAA (medium complexity)
   - TAA (most complex)

4. **Implement Upscaling**
   - FSR1 (easiest, no temporal data)
   - NIS (similar to FSR1)
   - FSR2 (complex, needs motion vectors)

5. **Testing & Optimization**
   - Profile performance impact
   - Test with various games
   - Optimize hot paths

---

## 📚 Reference Implementation

For reference, check these emulators:
- **RPCS3**: Excellent graphics settings UI
- **Yuzu/Ryujinx**: Good upscaling implementation
- **Dolphin**: Great post-processing system
- **PCSX2**: Comprehensive texture filtering

## 🔧 Build Integration

Add to CMakeLists.txt:
```cmake
# Graphics configuration
target_sources(shadps4 PRIVATE
    src/common/config_graphics.cpp
)

# Post-processing shaders
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/shaders/fxaa.spv
    COMMAND glslangValidator -V ${CMAKE_SOURCE_DIR}/src/video_core/host_shaders/fxaa.frag
            -o ${CMAKE_BINARY_DIR}/shaders/fxaa.spv
    DEPENDS src/video_core/host_shaders/fxaa.frag
)
```

---

**Status:** Configuration system complete, ready for Vulkan integration! 🚀
