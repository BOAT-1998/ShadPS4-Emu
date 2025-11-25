#// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include "video_core/renderer_interface.h"


using Microsoft::WRL::ComPtr;

namespace DX12 {

class Device;

/// DirectX 12 texture implementation
class Texture : public VideoCore::ITexture {
public:
    Texture(Device* device, u32 width, u32 height, u32 depth, u32 mip_levels, u32 array_layers,
            VideoCore::PixelFormat format, D3D12_RESOURCE_FLAGS flags);
    ~Texture() override;

    // ITexture interface
    u32 GetWidth() const override {
        return width;
    }
    u32 GetHeight() const override {
        return height;
    }
    u32 GetDepth() const override {
        return depth;
    }
    u32 GetMipLevels() const override {
        return mip_levels;
    }
    u32 GetArrayLayers() const override {
        return array_layers;
    }
    VideoCore::PixelFormat GetFormat() const override {
        return format;
    }
    void* GetNativeHandle() const override {
        return resource.Get();
    }

    // DirectX 12 specific methods
    ID3D12Resource* GetResource() const {
        return resource.Get();
    }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle() const {
        return rtv_handle;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const {
        return dsv_handle;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandle() const {
        return srv_handle;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE GetUavHandle() const {
        return uav_handle;
    }

    /// Transition resource state
    void TransitionTo(ID3D12GraphicsCommandList* cmd_list, D3D12_RESOURCE_STATES new_state);

    /// Get current resource state
    D3D12_RESOURCE_STATES GetCurrentState() const {
        return current_state;
    }

private:
    void CreateResource(D3D12_RESOURCE_FLAGS flags);
    void CreateViews();
    DXGI_FORMAT GetDxgiFormat() const;

    Device* device;
    ComPtr<ID3D12Resource> resource;

    u32 width;
    u32 height;
    u32 depth;
    u32 mip_levels;
    u32 array_layers;
    VideoCore::PixelFormat format;

    D3D12_RESOURCE_STATES current_state = D3D12_RESOURCE_STATE_COMMON;

    // Descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE srv_handle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE uav_handle = {};

    u32 rtv_index = UINT32_MAX;
    u32 dsv_index = UINT32_MAX;
    u32 srv_index = UINT32_MAX;
    u32 uav_index = UINT32_MAX;
};

} // namespace DX12


