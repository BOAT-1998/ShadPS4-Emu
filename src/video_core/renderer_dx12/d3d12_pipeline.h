#// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include <d3d12.h>
#include <wrl/client.h>
#include "video_core/renderer_interface.h"


using Microsoft::WRL::ComPtr;

namespace DX12 {

class Device;

/// DirectX 12 pipeline state implementation
class PipelineState : public VideoCore::IPipelineState {
public:
    ~PipelineState() override;

    // IPipelineState interface
    bool IsCompute() const override {
        return is_compute;
    }
    void* GetNativeHandle() const override {
        return pipeline_state.Get();
    }

    // DirectX 12 specific methods
    ID3D12PipelineState* GetPipelineState() const {
        return pipeline_state.Get();
    }
    ID3D12RootSignature* GetRootSignature() const {
        return root_signature.Get();
    }

protected:
    PipelineState(Device* device, bool is_compute);

    Device* device;
    ComPtr<ID3D12PipelineState> pipeline_state;
    ComPtr<ID3D12RootSignature> root_signature;
    bool is_compute;
};

/// Graphics pipeline state
class GraphicsPipelineState : public PipelineState {
public:
    struct CreateInfo {
        // Vertex shader
        const void* vs_bytecode = nullptr;
        size_t vs_bytecode_size = 0;

        // Pixel shader
        const void* ps_bytecode = nullptr;
        size_t ps_bytecode_size = 0;

        // Geometry shader (optional)
        const void* gs_bytecode = nullptr;
        size_t gs_bytecode_size = 0;

        // Hull shader (optional)
        const void* hs_bytecode = nullptr;
        size_t hs_bytecode_size = 0;

        // Domain shader (optional)
        const void* ds_bytecode = nullptr;
        size_t ds_bytecode_size = 0;

        // Input layout
        std::vector<D3D12_INPUT_ELEMENT_DESC> input_elements;

        // Rasterizer state
        D3D12_RASTERIZER_DESC rasterizer_state = {};

        // Blend state
        D3D12_BLEND_DESC blend_state = {};

        // Depth stencil state
        D3D12_DEPTH_STENCIL_DESC depth_stencil_state = {};

        // Primitive topology
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // Render target formats
        std::vector<DXGI_FORMAT> rtv_formats;
        DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;

        // Sample desc
        DXGI_SAMPLE_DESC sample_desc = {1, 0};
    };

    GraphicsPipelineState(Device* device, const CreateInfo& info);

private:
    void CreateRootSignature();
    void CreatePipeline(const CreateInfo& info);
};

/// Compute pipeline state
class ComputePipelineState : public PipelineState {
public:
    struct CreateInfo {
        const void* cs_bytecode = nullptr;
        size_t cs_bytecode_size = 0;
    };

    ComputePipelineState(Device* device, const CreateInfo& info);

private:
    void CreateRootSignature();
    void CreatePipeline(const CreateInfo& info);
};

} // namespace DX12


