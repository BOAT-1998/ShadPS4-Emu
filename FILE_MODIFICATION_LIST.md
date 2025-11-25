# Complete File Modification List for Advanced Graphics Settings

## ✅ Files to Modify (Existing)

### Configuration System
1. `src/common/config.h` - ✅ DONE (declarations added)
2. `src/common/config.cpp` - ✅ DONE (variables added)
3. `src/common/config_graphics.cpp` - ✅ DONE (implementations added)

### Qt GUI
4. `src/qt_gui/settings_dialog.ui` - ⏳ TODO (add Advanced Graphics tab)
5. `src/qt_gui/settings_dialog.h` - ⏳ TODO (add slot declarations)
6. `src/qt_gui/settings_dialog.cpp` - ⏳ TODO (connect UI to config)

### Vulkan Renderer Core
7. `src/video_core/renderer_vulkan/vk_instance.h` - ⏳ TODO (add settings cache)
8. `src/video_core/renderer_vulkan/vk_instance.cpp` - ⏳ TODO (read config on init)
9. `src/video_core/renderer_vulkan/vk_swapchain.h` - ⏳ TODO (add HDR/VSync support)
10. `src/video_core/renderer_vulkan/vk_swapchain.cpp` - ⏳ TODO (implement HDR/VSync)
11. `src/video_core/renderer_vulkan/vk_presenter.h` - ⏳ TODO (add resolution scaling)
12. `src/video_core/renderer_vulkan/vk_presenter.cpp` - ⏳ TODO (implement scaling/upscaling)
13. `src/video_core/renderer_vulkan/vk_rasterizer.h` - ⏳ TODO (add frame limiter)
14. `src/video_core/renderer_vulkan/vk_rasterizer.cpp` - ⏳ TODO (implement frame limiter)
15. `src/video_core/renderer_vulkan/vk_scheduler.h` - ⏳ TODO (add async shader support)
16. `src/video_core/renderer_vulkan/vk_scheduler.cpp` - ⏳ TODO (implement async compile)

### Texture & Sampling
17. `src/video_core/texture_cache/texture_cache.h` - ⏳ TODO (add quality settings)
18. `src/video_core/texture_cache/texture_cache.cpp` - ⏳ TODO (implement aniso filtering)
19. `src/video_core/texture_cache/sampler_cache.h` - ⏳ TODO (add aniso cache)
20. `src/video_core/texture_cache/sampler_cache.cpp` - ⏳ TODO (implement aniso cache)

### Pipeline & Shaders
21. `src/video_core/renderer_vulkan/vk_pipeline_cache.h` - ⏳ TODO (add prefetch)
22. `src/video_core/renderer_vulkan/vk_pipeline_cache.cpp` - ⏳ TODO (implement prefetch)

## 📁 Files to Create (New)

### Post-Processing Shaders
23. `src/video_core/host_shaders/fxaa.frag` - ⏳ TODO (FXAA shader)
24. `src/video_core/host_shaders/smaa_edge.frag` - ⏳ TODO (SMAA edge detection)
25. `src/video_core/host_shaders/smaa_blend.frag` - ⏳ TODO (SMAA blending)
26. `src/video_core/host_shaders/taa.frag` - ⏳ TODO (TAA shader)

### Upscaling Shaders
27. `src/video_core/host_shaders/fsr1_easu.comp` - ⏳ TODO (FSR1 EASU)
28. `src/video_core/host_shaders/fsr1_rcas.comp` - ⏳ TODO (FSR1 RCAS)
29. `src/video_core/host_shaders/fsr2.comp` - ⏳ TODO (FSR2 compute)
30. `src/video_core/host_shaders/nis.comp` - ⏳ TODO (NIS upscaler)

### Post-Processing System
31. `src/video_core/renderer_vulkan/vk_post_process.h` - ⏳ TODO (post-process manager)
32. `src/video_core/renderer_vulkan/vk_post_process.cpp` - ⏳ TODO (implement AA/upscaling)

### Frame Limiter
33. `src/video_core/renderer_vulkan/vk_frame_limiter.h` - ⏳ TODO (frame limiter)
34. `src/video_core/renderer_vulkan/vk_frame_limiter.cpp` - ⏳ TODO (implement limiter)

## 🔧 Build System
35. `CMakeLists.txt` - ⏳ TODO (add new files to build)

## 📊 Total Files
- **Modify:** 22 files
- **Create:** 13 files
- **Total:** 35 files

## 🎯 Implementation Priority

### Priority 1 (Core Functionality) - Week 1
1. Resolution Scaling (vk_presenter)
2. VSync Mode (vk_swapchain)
3. Anisotropic Filtering (texture_cache)
4. Frame Limiter (vk_frame_limiter)
5. Qt GUI (settings_dialog)

### Priority 2 (Quality Improvements) - Week 2
6. FXAA (vk_post_process + shader)
7. FSR1 (vk_post_process + shader)
8. HDR (vk_swapchain)
9. Texture Quality (texture_cache)

### Priority 3 (Advanced Features) - Week 3
10. SMAA (post-process + shaders)
11. TAA (post-process + shader)
12. FSR2 (post-process + shader)
13. NIS (post-process + shader)
14. Async Shader Compile (vk_scheduler)
15. Pipeline Prefetch (vk_pipeline_cache)

### Priority 4 (Polish) - Week 4
16. Vulkan Advanced Options
17. Shader Debug Mode
18. Timing Modes
19. Testing & Optimization
20. Documentation

## 📝 Notes
- All shader files need to be compiled to SPIR-V
- CMakeLists.txt needs custom commands for shader compilation
- Qt .ui file needs to be regenerated after modification
- Config save/load needs to be tested thoroughly
