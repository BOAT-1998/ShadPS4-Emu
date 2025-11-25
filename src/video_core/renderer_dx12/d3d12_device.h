#// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/renderer_interface.h"

#include <memory>
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>


using Microsoft::WRL::ComPtr;

namespace VideoCore {
class BufferCache;
class TextureCache;
class PageManager;
} // namespace VideoCore

namespace DX12 {

class CommandList;
class DescriptorHeap;
class RootSignature;

/// DirectX 12 GPU device implementation
class Device : public VideoCore::IGpuDevice {
public:
    explicit Device(void* window_handle);
    ~Device() override;

    // IGpuDevice interface
    VideoCore::GraphicsAPI GetAPI() const override {
        return VideoCore::GraphicsAPI::DirectX12;
    }
    void WaitIdle() override;
    VideoCore::ICommandList* CreateCommandList() override;
    void Submit(std::span<VideoCore::ICommandList*> command_lists) override;
    void Present() override;
    VideoCore::BufferCache& GetBufferCache() override;
    VideoCore::TextureCache& GetTextureCache() override;

    // DirectX 12 specific methods
    ID3D12Device* GetDevice() const {
        return device.Get();
    }
    ID3D12CommandQueue* GetGraphicsQueue() const {
        return graphics_queue.Get();
    }
    ID3D12CommandQueue* GetComputeQueue() const {
        return compute_queue.Get();
    }
    ID3D12CommandQueue* GetCopyQueue() const {
        return copy_queue.Get();
    }

    DescriptorHeap* GetRtvHeap() const {
        return rtv_heap.get();
    }
    DescriptorHeap* GetDsvHeap() const {
        return dsv_heap.get();
    }
    DescriptorHeap* GetCbvSrvUavHeap() const {
        return cbv_srv_uav_heap.get();
    }
    DescriptorHeap* GetSamplerHeap() const {
        return sampler_heap.get();
    }

    /// Get current frame index
    u32 GetCurrentFrameIndex() const {
        return current_frame_index;
    }

    /// Get number of frames in flight
    static constexpr u32 GetNumFrames() {
        return NUM_FRAMES;
    }

    /// Wait for a specific fence value
    void WaitForFence(u64 fence_value);

    /// Signal fence from GPU
    u64 SignalFence();

private:
    void CreateDevice();
    void CreateCommandQueues();
    void CreateSwapchain(void* window_handle);
    void CreateDescriptorHeaps();
    void CreateFenceAndEvent();
    void CreateRenderTargets();

    static constexpr u32 NUM_FRAMES = 3;
    static constexpr u32 SWAPCHAIN_WIDTH = 1920;
    static constexpr u32 SWAPCHAIN_HEIGHT = 1080;

    // Core D3D12 objects
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> graphics_queue;
    ComPtr<ID3D12CommandQueue> compute_queue;
    ComPtr<ID3D12CommandQueue> copy_queue;

    // Swapchain
    ComPtr<IDXGISwapChain3> swapchain;
    ComPtr<ID3D12Resource> render_targets[NUM_FRAMES];

    // Descriptor heaps
    std::unique_ptr<DescriptorHeap> rtv_heap;
    std::unique_ptr<DescriptorHeap> dsv_heap;
    std::unique_ptr<DescriptorHeap> cbv_srv_uav_heap;
    std::unique_ptr<DescriptorHeap> sampler_heap;

    // Synchronization
    ComPtr<ID3D12Fence> fence;
    HANDLE fence_event;
    u64 fence_value = 0;
    u64 frame_fence_values[NUM_FRAMES] = {};
    u32 current_frame_index = 0;

    // Command lists pool
    std::vector<std::unique_ptr<CommandList>> command_lists;

    // Caches (will be implemented separately)
    std::unique_ptr<VideoCore::BufferCache> buffer_cache;
    std::unique_ptr<VideoCore::TextureCache> texture_cache;
    std::unique_ptr<VideoCore::PageManager> page_manager;
};

/// Descriptor heap wrapper
class DescriptorHeap {
public:
    DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_descriptors,
                   bool shader_visible = false);
    ~DescriptorHeap();

    /// Get CPU descriptor handle at index
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(u32 index) const;

    /// Get GPU descriptor handle at index (only for shader-visible heaps)
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(u32 index) const;

    /// Allocate a descriptor and return its index
    u32 Allocate();

    /// Free a descriptor
    void Free(u32 index);

    /// Get the heap
    ID3D12DescriptorHeap* GetHeap() const {
        return heap.Get();
    }

private:
    ComPtr<ID3D12DescriptorHeap> heap;
    u32 descriptor_size = 0;
    u32 num_descriptors = 0;
    u32 next_free_index = 0;
    std::vector<bool> allocated;
};

} // namespace DX12


