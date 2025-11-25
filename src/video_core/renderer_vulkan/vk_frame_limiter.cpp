// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <numeric>
#include <thread>
#include "vk_frame_limiter.h"


namespace Vulkan {

FrameLimiter::FrameLimiter() {
    Reset();
}

void FrameLimiter::BeginFrame() {
    frame_start_time = Clock::now();
}

void FrameLimiter::EndFrame(int target_fps) {
    const auto frame_end_time = Clock::now();
    const auto frame_duration =
        std::chrono::duration_cast<Duration>(frame_end_time - frame_start_time);

    // Update statistics
    const float frame_time_ms = frame_duration.count() / 1000.0f;
    UpdateStatistics(frame_time_ms);

    // Apply frame limiting if target FPS is set
    if (target_fps > 0) {
        const auto target_frame_time = CalculateTargetFrameTime(target_fps);

        if (frame_duration < target_frame_time) {
            const auto sleep_duration = target_frame_time - frame_duration;
            PreciseSleep(sleep_duration);
        }
    }

    last_frame_time = Clock::now();
}

void FrameLimiter::Reset() {
    frame_start_time = Clock::now();
    last_frame_time = frame_start_time;
    current_fps = 0.0f;
    average_frame_time = 0.0f;
    frame_time_history.fill(0.0f);
    history_index = 0;
}

FrameLimiter::Duration FrameLimiter::CalculateTargetFrameTime(int fps) const {
    if (fps <= 0) {
        return Duration(0);
    }

    // Convert FPS to microseconds per frame
    const auto microseconds_per_frame = 1'000'000 / fps;
    return Duration(microseconds_per_frame);
}

void FrameLimiter::PreciseSleep(Duration duration) {
    if (duration.count() <= 0) {
        return;
    }

    // For durations > 2ms, use thread sleep
    // For last 2ms, use spin-wait for precision
    constexpr auto SPIN_THRESHOLD = Duration(2000); // 2ms

    if (duration > SPIN_THRESHOLD) {
        const auto sleep_duration = duration - SPIN_THRESHOLD;
        std::this_thread::sleep_for(sleep_duration);
    }

    // Spin-wait for remaining time (high precision)
    const auto target_time = Clock::now() + duration;
    while (Clock::now() < target_time) {
        // Yield to other threads to avoid 100% CPU usage
        std::this_thread::yield();
    }
}

void FrameLimiter::UpdateStatistics(float frame_time) {
    // Add to history
    frame_time_history[history_index] = frame_time;
    history_index = (history_index + 1) % HISTORY_SIZE;

    // Calculate average frame time
    const float sum = std::accumulate(frame_time_history.begin(), frame_time_history.end(), 0.0f);
    average_frame_time = sum / HISTORY_SIZE;

    // Calculate current FPS
    if (average_frame_time > 0.0f) {
        current_fps = 1000.0f / average_frame_time;
    } else {
        current_fps = 0.0f;
    }
}

} // namespace Vulkan


