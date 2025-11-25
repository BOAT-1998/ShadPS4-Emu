// ============================================================================
// Advanced Graphics Settings Implementation
// ============================================================================

// Resolution Scaling
int getResolutionScale() {
    return resolutionScale.get();
}

void setResolutionScale(int scale, bool is_game_specific) {
    // Clamp to valid values
    if (scale != 50 && scale != 75 && scale != 100 && scale != 150 && scale != 200 &&
        scale != 300 && scale != 400) {
        scale = 100; // Default to 100%
    }
    resolutionScale.set(scale, is_game_specific);
}

// Anisotropic Filtering
int getAnisotropicFiltering() {
    return anisotropicFiltering.get();
}

void setAnisotropicFiltering(int level, bool is_game_specific) {
    // Clamp to valid values: 1, 2, 4, 8, 16
    if (level != 1 && level != 2 && level != 4 && level != 8 && level != 16) {
        level = 16; // Default to 16x
    }
    anisotropicFiltering.set(level, is_game_specific);
}

// Anti-Aliasing
std::string getAntiAliasing() {
    return antiAliasing.get();
}

void setAntiAliasing(const std::string& mode, bool is_game_specific) {
    antiAliasing.set(mode, is_game_specific);
}

// Texture Quality
std::string getTextureQuality() {
    return textureQuality.get();
}

void setTextureQuality(const std::string& quality, bool is_game_specific) {
    textureQuality.set(quality, is_game_specific);
}

// Upscaler
std::string getUpscaler() {
    return upscaler.get();
}

void setUpscaler(const std::string& upscaler_mode, bool is_game_specific) {
    upscaler.set(upscaler_mode, is_game_specific);
}

int getUpscalerSharpness() {
    return upscalerSharpness.get();
}

void setUpscalerSharpness(int sharpness, bool is_game_specific) {
    // Clamp to 0-100
    if (sharpness < 0)
        sharpness = 0;
    if (sharpness > 100)
        sharpness = 100;
    upscalerSharpness.set(sharpness, is_game_specific);
}

// Frame Limiter
int getFrameLimit() {
    return frameLimit.get();
}

void setFrameLimit(int fps, bool is_game_specific) {
    frameLimit.set(fps, is_game_specific);
}

// VSync Mode
std::string getVSyncMode() {
    return vsyncMode.get();
}

void setVSyncMode(const std::string& mode, bool is_game_specific) {
    vsyncMode.set(mode, is_game_specific);
}

// HDR
bool getHDREnabled() {
    return hdrEnabled.get();
}

void setHDREnabled(bool enable, bool is_game_specific) {
    hdrEnabled.set(enable, is_game_specific);
}

// Vulkan Advanced Options
bool getVkPipelineCache() {
    return vkPipelineCache.get();
}

void setVkPipelineCache(bool enable, bool is_game_specific) {
    vkPipelineCache.set(enable, is_game_specific);
}

bool getVkPipelinePrefetch() {
    return vkPipelinePrefetch.get();
}

void setVkPipelinePrefetch(bool enable, bool is_game_specific) {
    vkPipelinePrefetch.set(enable, is_game_specific);
}

bool getVkDescriptorIndexing() {
    return vkDescriptorIndexing.get();
}

void setVkDescriptorIndexing(bool enable, bool is_game_specific) {
    vkDescriptorIndexing.set(enable, is_game_specific);
}

bool getVkDynamicRendering() {
    return vkDynamicRendering.get();
}

void setVkDynamicRendering(bool enable, bool is_game_specific) {
    vkDynamicRendering.set(enable, is_game_specific);
}

bool getVkGPUTimestamps() {
    return vkGPUTimestamps.get();
}

void setVkGPUTimestamps(bool enable, bool is_game_specific) {
    vkGPUTimestamps.set(enable, is_game_specific);
}

bool getVkMultithreadedCmdRecording() {
    return vkMultithreadedCmd.get();
}

void setVkMultithreadedCmdRecording(bool enable, bool is_game_specific) {
    vkMultithreadedCmd.set(enable, is_game_specific);
}

// Shader Compilation
bool getShaderPreloadCache() {
    return shaderPreloadCache.get();
}

void setShaderPreloadCache(bool enable, bool is_game_specific) {
    shaderPreloadCache.set(enable, is_game_specific);
}

bool getAsyncShaderCompile() {
    return asyncShaderCompile.get();
}

void setAsyncShaderCompile(bool enable, bool is_game_specific) {
    asyncShaderCompile.set(enable, is_game_specific);
}

bool getSkipSlowBarriers() {
    return skipSlowBarriers.get();
}

void setSkipSlowBarriers(bool enable, bool is_game_specific) {
    skipSlowBarriers.set(enable, is_game_specific);
}

bool getShaderDebugMode() {
    return shaderDebugMode.get();
}

void setShaderDebugMode(bool enable, bool is_game_specific) {
    shaderDebugMode.set(enable, is_game_specific);
}

// CPU/GPU Timing
std::string getTimingMode() {
    return timingMode.get();
}

void setTimingMode(const std::string& mode, bool is_game_specific) {
    timingMode.set(mode, is_game_specific);
}
