#// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#include <d3d12sdklayers.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_dx12/d3d12_command_list.h"
#include "video_core/renderer_dx12/d3d12_device.h"

namespace DX12 {

namespace {

void ThrowIfFailed(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        LOG_ERROR(Render, "{}: HRESULT = 0x{:08X}", message, static_cast<u32>(hr));
        throw std::runtime_error(message);
    }
}

} // Anonymous namespace

Device::Device(void* window_handle) {
    LOG_INFO(Render, "Initializing DirectX 12 device");

    CreateDevice();
    CreateCommandQueues();
    CreateSwapchain(window_handle);
    CreateDescriptorHeaps();
    CreateFenceAndEvent();
    CreateRenderTargets();

    LOG_INFO(Render, "DirectX 12 device initialized successfully");
}

Device::~Device() {
    WaitIdle();

    if (fence_event) {
        CloseHandle(fence_event);
    }
}

void Device::CreateDevice() {
    // Enable debug layer in debug builds
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debug_controller;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
        debug_controller->EnableDebugLayer();
        LOG_INFO(Render, "D3D12 debug layer enabled");
    }
#endif

    // Create DXGI factory
    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "Failed to create DXGI factory");

    // Enumerate adapters and find the best one
    ComPtr<IDXGIAdapter1> adapter;
    SIZE_T max_dedicated_memory = 0;

    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIAdapter1> current_adapter;
        if (factory->EnumAdapters1(i, &current_adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        DXGI_ADAPTER_DESC1 desc;
        current_adapter->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // Check if adapter supports D3D12
        if (SUCCEEDED(D3D12CreateDevice(current_adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                        __uuidof(ID3D12Device), nullptr))) {
            if (desc.DedicatedVideoMemory > max_dedicated_memory) {
                max_dedicated_memory = desc.DedicatedVideoMemory;
                adapter = current_adapter;
            }
        }
    }

    ASSERT_MSG(adapter, "No compatible D3D12 adapter found");

    // Create device
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)),
                  "Failed to create D3D12 device");

    // Log adapter info
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);
    LOG_INFO(Render, "Using adapter: {} (VRAM: {} MB)", Common::UTF16ToUTF8(desc.Description),
             desc.DedicatedVideoMemory / (1024 * 1024));
}

void Device::CreateCommandQueues() {
    // Graphics queue
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&graphics_queue)),
                  "Failed to create graphics command queue");
    graphics_queue->SetName(L"Graphics Queue");

    // Compute queue
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    ThrowIfFailed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&compute_queue)),
                  "Failed to create compute command queue");
    compute_queue->SetName(L"Compute Queue");

    // Copy queue
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    ThrowIfFailed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&copy_queue)),
                  "Failed to create copy command queue");
    copy_queue->SetName(L"Copy Queue");
}

void Device::CreateSwapchain(void* window_handle) {
    HWND hwnd = static_cast<HWND>(window_handle);

    // Create DXGI factory
    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)),
                  "Failed to create DXGI factory for swapchain");

    // Describe swapchain
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
    swapchain_desc.Width = SWAPCHAIN_WIDTH;
    swapchain_desc.Height = SWAPCHAIN_HEIGHT;
    swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.Stereo = FALSE;
    swapchain_desc.SampleDesc.Count = 1;
    swapchain_desc.SampleDesc.Quality = 0;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = NUM_FRAMES;
    swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchain_desc.Flags = 0;

    ComPtr<IDXGISwapChain1> swapchain1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(graphics_queue.Get(), hwnd, &swapchain_desc,
                                                  nullptr, nullptr, &swapchain1),
                  "Failed to create swapchain");

    // Upgrade to IDXGISwapChain3
    ThrowIfFailed(swapchain1.As(&swapchain), "Failed to upgrade swapchain");

    // Disable Alt+Enter fullscreen toggle
    ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER),
                  "Failed to disable Alt+Enter");

    current_frame_index = swapchain->GetCurrentBackBufferIndex();
}

void Device::CreateDescriptorHeaps() {
    // RTV heap (Render Target Views)
    rtv_heap =
        std::make_unique<DescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256, false);

    // DSV heap (Depth Stencil Views)
    dsv_heap =
        std::make_unique<DescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256, false);

    // CBV/SRV/UAV heap (Constant Buffer, Shader Resource, Unordered Access Views)
    cbv_srv_uav_heap = std::make_unique<DescriptorHeap>(
        device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 10000, true);

    // Sampler heap
    sampler_heap = std::make_unique<DescriptorHeap>(device.Get(),
                                                    D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048, true);
}

void Device::CreateFenceAndEvent() {
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
                  "Failed to create fence");

    fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ASSERT_MSG(fence_event, "Failed to create fence event");
}

void Device::CreateRenderTargets() {
    for (u32 i = 0; i < NUM_FRAMES; ++i) {
        ThrowIfFailed(swapchain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])),
                      "Failed to get swapchain buffer");

        // Create RTV for this render target
        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
        rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        u32 rtv_index = rtv_heap->Allocate();
        device->CreateRenderTargetView(render_targets[i].Get(), &rtv_desc,
                                       rtv_heap->GetCpuHandle(rtv_index));
    }
}

void Device::WaitIdle() {
    const u64 current_fence = SignalFence();
    WaitForFence(current_fence);
}

VideoCore::ICommandList* Device::CreateCommandList() {
    auto cmd_list = std::make_unique<CommandList>(this);
    auto* ptr = cmd_list.get();
    command_lists.push_back(std::move(cmd_list));
    return ptr;
}

void Device::Submit(std::span<VideoCore::ICommandList*> cmd_lists) {
    std::vector<ID3D12CommandList*> d3d12_cmd_lists;
    d3d12_cmd_lists.reserve(cmd_lists.size());

    for (auto* cmd_list : cmd_lists) {
        auto* dx12_cmd_list = static_cast<CommandList*>(cmd_list);
        d3d12_cmd_lists.push_back(dx12_cmd_list->GetCommandList());
    }

    graphics_queue->ExecuteCommandLists(static_cast<UINT>(d3d12_cmd_lists.size()),
                                        d3d12_cmd_lists.data());
}

void Device::Present() {
    ThrowIfFailed(swapchain->Present(1, 0), "Failed to present");

    // Move to next frame
    const u64 current_fence = SignalFence();
    frame_fence_values[current_frame_index] = current_fence;
    current_frame_index = swapchain->GetCurrentBackBufferIndex();

    // Wait for the next frame to be ready
    WaitForFence(frame_fence_values[current_frame_index]);
}

void Device::WaitForFence(u64 fence_value_to_wait) {
    if (fence->GetCompletedValue() < fence_value_to_wait) {
        ThrowIfFailed(fence->SetEventOnCompletion(fence_value_to_wait, fence_event),
                      "Failed to set fence event");
        WaitForSingleObject(fence_event, INFINITE);
    }
}

u64 Device::SignalFence() {
    const u64 value = ++fence_value;
    ThrowIfFailed(graphics_queue->Signal(fence.Get(), value), "Failed to signal fence");
    return value;
}

VideoCore::BufferCache& Device::GetBufferCache() {
    ASSERT_MSG(buffer_cache, "Buffer cache not initialized");
    return *buffer_cache;
}

VideoCore::TextureCache& Device::GetTextureCache() {
    ASSERT_MSG(texture_cache, "Texture cache not initialized");
    return *texture_cache;
}

// DescriptorHeap implementation

DescriptorHeap::DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                               u32 num_descriptors, bool shader_visible)
    : num_descriptors(num_descriptors), allocated(num_descriptors, false) {

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = num_descriptors;
    desc.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)),
                  "Failed to create descriptor heap");

    descriptor_size = device->GetDescriptorHandleIncrementSize(type);
}

DescriptorHeap::~DescriptorHeap() = default;

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpuHandle(u32 index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += index * descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpuHandle(u32 index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += index * descriptor_size;
    return handle;
}

u32 DescriptorHeap::Allocate() {
    for (u32 i = next_free_index; i < num_descriptors; ++i) {
        if (!allocated[i]) {
            allocated[i] = true;
            next_free_index = i + 1;
            return i;
        }
    }

    // Wrap around
    for (u32 i = 0; i < next_free_index; ++i) {
        if (!allocated[i]) {
            allocated[i] = true;
            next_free_index = i + 1;
            return i;
        }
    }

    UNREACHABLE_MSG("Descriptor heap exhausted");
}

void DescriptorHeap::Free(u32 index) {
    ASSERT(index < num_descriptors);
    allocated[index] = false;
    if (index < next_free_index) {
        next_free_index = index;
    }
}

} // namespace DX12


