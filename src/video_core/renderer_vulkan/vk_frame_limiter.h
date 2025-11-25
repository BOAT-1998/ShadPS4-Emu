// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <array>
#include "common/types.h"

namespace Vulkan {

/**
 * Frame Limiter - Controls frame pacing to match target FPS
 *
 * Performance Impact:
 * - Minimal CPU overhead (< 0.1%)
 * - Reduces GPU power consumption when limiting
 * - Provides smoother frame times
 *
 * Usage:
 * - Set target FPS via Config::setFrameLimit()
 * - Call BeginFrame() at start of frame
 * - Call EndFrame() after present
 */
class FrameLimiter {
public:
    FrameLimiter();
    ~FrameLimiter() = default;

    /**
     * Call at the beginning of each frame
     * Records frame start time
     */
    void BeginFrame();

    /**
     * Call at the end of each frame (after present)
     * Sleeps if necessary to match target frame time
     *
     * @param target_fps Target frames per second (0 = unlimited)
     */
    void EndFrame(int target_fps);

    /**
     * Get current FPS
     * @return Current frames per second
     */
    float GetCurrentFPS() const {
        return current_fps;
    }

    /**
     * Get average frame time in milliseconds
     * @return Average frame time
     */
    float GetAverageFrameTime() const {
        return average_frame_time;
    }

    /**
     * Reset statistics
     */
    void Reset();

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::microseconds;

    TimePoint frame_start_time;
    TimePoint last_frame_time;

    // Statistics
    float current_fps = 0.0f;
    float average_frame_time = 0.0f;

    // Frame time history for smoothing
    static constexpr size_t HISTORY_SIZE = 60;
    std::array<float, HISTORY_SIZE> frame_time_history{};
    size_t history_index = 0;

    /**
     * Calculate target frame time from FPS
     * @param fps Target frames per second
     * @return Target frame time in microseconds
     */
    Duration CalculateTargetFrameTime(int fps) const;

    /**
     * Sleep for specified duration with high precision
     * Uses spin-wait for last microseconds for accuracy
     *
     * @param duration Duration to sleep
     */
    void PreciseSleep(Duration duration);

    /**
     * Update FPS statistics
     * @param frame_time Current frame time
     */
    void UpdateStatistics(float frame_time);
};

} // namespace Vulkan


