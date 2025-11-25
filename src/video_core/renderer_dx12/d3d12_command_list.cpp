#// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#include <pix3.h> // For PIX markers
#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_dx12/d3d12_command_list.h"
#include "video_core/renderer_dx12/d3d12_device.h"
#include "video_core/renderer_dx12/d3d12_pipeline.h"
#include "video_core/renderer_dx12/d3d12_texture.h"


namespace DX12 {

namespace {

void ThrowIfFailed(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        LOG_ERROR(Render, "{}: HRESULT = 0x{:08X}", message, static_cast<u32>(hr));
        throw std::runtime_error(message);
    }
}

D3D12_RESOURCE_BARRIER CreateTransitionBarrier(ID3D12Resource* resource,
                                               D3D12_RESOURCE_STATES before,
                                               D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

} // Anonymous namespace

CommandList::CommandList(Device* device) : device(device) {
    CreateCommandAllocator();
    CreateCommandList();
}

CommandList::~CommandList() = default;

void CommandList::CreateCommandAllocator() {
    ThrowIfFailed(device->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                              IID_PPV_ARGS(&command_allocator)),
                  "Failed to create command allocator");
}

void CommandList::CreateCommandList() {
    ThrowIfFailed(device->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                         command_allocator.Get(), nullptr,
                                                         IID_PPV_ARGS(&command_list)),
                  "Failed to create command list");

    // Command lists are created in the recording state, close it for now
    command_list->Close();
    is_recording = false;
}

void CommandList::Begin() {
    ASSERT_MSG(!is_recording, "Command list is already recording");

    ThrowIfFailed(command_allocator->Reset(), "Failed to reset command allocator");
    ThrowIfFailed(command_list->Reset(command_allocator.Get(), nullptr),
                  "Failed to reset command list");

    // Set descriptor heaps
    ID3D12DescriptorHeap* heaps[] = {device->GetCbvSrvUavHeap()->GetHeap(),
                                     device->GetSamplerHeap()->GetHeap()};
    command_list->SetDescriptorHeaps(_countof(heaps), heaps);

    is_recording = true;
}

void CommandList::End() {
    ASSERT_MSG(is_recording, "Command list is not recording");

    ThrowIfFailed(command_list->Close(), "Failed to close command list");
    is_recording = false;
}

void CommandList::Reset() {
    if (is_recording) {
        End();
    }
    Begin();
}

void CommandList::SetPipeline(VideoCore::IPipelineState* pipeline) {
    auto* dx12_pipeline = static_cast<PipelineState*>(pipeline);

    if (dx12_pipeline->IsCompute()) {
        command_list->SetPipelineState(dx12_pipeline->GetPipelineState());
        command_list->SetComputeRootSignature(dx12_pipeline->GetRootSignature());
    } else {
        command_list->SetPipelineState(dx12_pipeline->GetPipelineState());
        command_list->SetGraphicsRootSignature(dx12_pipeline->GetRootSignature());
    }
}

void CommandList::BindVertexBuffers(u32 first_binding, std::span<const VAddr> addresses,
                                    std::span<const u32> strides) {
    ASSERT(addresses.size() == strides.size());

    std::vector<D3D12_VERTEX_BUFFER_VIEW> views;
    views.reserve(addresses.size());

    for (size_t i = 0; i < addresses.size(); ++i) {
        // TODO: Get actual buffer from buffer cache
        D3D12_VERTEX_BUFFER_VIEW view = {};
        view.BufferLocation = addresses[i]; // This needs to be GPU virtual address
        view.SizeInBytes = 0;               // TODO: Get size from buffer cache
        view.StrideInBytes = strides[i];
        views.push_back(view);
    }

    command_list->IASetVertexBuffers(first_binding, static_cast<UINT>(views.size()), views.data());
}

void CommandList::BindIndexBuffer(VAddr address, u32 size, bool is_32bit) {
    D3D12_INDEX_BUFFER_VIEW view = {};
    view.BufferLocation = address; // This needs to be GPU virtual address
    view.SizeInBytes = size;
    view.Format = is_32bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

    command_list->IASetIndexBuffer(&view);
}

void CommandList::SetViewport(float x, float y, float width, float height, float min_depth,
                              float max_depth) {
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = x;
    viewport.TopLeftY = y;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = min_depth;
    viewport.MaxDepth = max_depth;

    command_list->RSSetViewports(1, &viewport);
}

void CommandList::SetScissor(s32 x, s32 y, u32 width, u32 height) {
    D3D12_RECT scissor = {};
    scissor.left = x;
    scissor.top = y;
    scissor.right = x + width;
    scissor.bottom = y + height;

    command_list->RSSetScissorRects(1, &scissor);
}

void CommandList::Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance) {
    command_list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

void CommandList::DrawIndexed(u32 index_count, u32 instance_count, u32 first_index,
                              s32 vertex_offset, u32 first_instance) {
    command_list->DrawIndexedInstanced(index_count, instance_count, first_index, vertex_offset,
                                       first_instance);
}

void CommandList::DrawIndirect(VAddr args_address, u32 draw_count, u32 stride) {
    // TODO: Implement indirect draw
    // Need to get ID3D12Resource* from buffer cache
    LOG_WARNING(Render, "DrawIndirect not fully implemented");
}

void CommandList::Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) {
    command_list->Dispatch(group_count_x, group_count_y, group_count_z);
}

void CommandList::DispatchIndirect(VAddr args_address) {
    // TODO: Implement indirect dispatch
    LOG_WARNING(Render, "DispatchIndirect not fully implemented");
}

void CommandList::CopyBuffer(VAddr dst, VAddr src, u64 size) {
    // TODO: Get ID3D12Resource* from buffer cache
    LOG_WARNING(Render, "CopyBuffer not fully implemented");
}

void CommandList::CopyBufferToTexture(VAddr src_buffer, VideoCore::ITexture* dst_texture,
                                      u32 mip_level, u32 array_layer) {
    auto* dx12_texture = static_cast<Texture*>(dst_texture);

    // TODO: Implement buffer to texture copy
    LOG_WARNING(Render, "CopyBufferToTexture not fully implemented");
}

void CommandList::CopyTextureToBuffer(VideoCore::ITexture* src_texture, VAddr dst_buffer,
                                      u32 mip_level, u32 array_layer) {
    auto* dx12_texture = static_cast<Texture*>(src_texture);

    // TODO: Implement texture to buffer copy
    LOG_WARNING(Render, "CopyTextureToBuffer not fully implemented");
}

void CommandList::BeginRenderPass(std::span<VideoCore::ITexture*> color_attachments,
                                  VideoCore::ITexture* depth_attachment) {
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtv_handles;
    rtv_handles.reserve(color_attachments.size());

    for (auto* attachment : color_attachments) {
        auto* dx12_texture = static_cast<Texture*>(attachment);
        rtv_handles.push_back(dx12_texture->GetRtvHandle());
    }

    D3D12_CPU_DESCRIPTOR_HANDLE* dsv_handle = nullptr;
    if (depth_attachment) {
        auto* dx12_depth = static_cast<Texture*>(depth_attachment);
        dsv_handle = &dx12_depth->GetDsvHandle();
    }

    command_list->OMSetRenderTargets(static_cast<UINT>(rtv_handles.size()), rtv_handles.data(),
                                     FALSE, dsv_handle);
}

void CommandList::EndRenderPass() {
    // In D3D12, there's no explicit "end render pass" like Vulkan
    // Just clear the render targets
    command_list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
}

void CommandList::Barrier(VideoCore::PipelineStage src_stage, VideoCore::PipelineStage dst_stage,
                          VideoCore::AccessFlags src_access, VideoCore::AccessFlags dst_access) {
    // TODO: Implement proper resource barriers based on access flags
    // For now, just insert a UAV barrier
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = nullptr; // Applies to all resources

    command_list->ResourceBarrier(1, &barrier);
}

void CommandList::BeginDebugMarker(std::string_view label, const float color[4]) {
    if (color) {
        PIXBeginEvent(command_list.Get(),
                      PIX_COLOR(static_cast<BYTE>(color[0] * 255),
                                static_cast<BYTE>(color[1] * 255),
                                static_cast<BYTE>(color[2] * 255)),
                      label.data());
    } else {
        PIXBeginEvent(command_list.Get(), 0, label.data());
    }
}

void CommandList::EndDebugMarker() {
    PIXEndEvent(command_list.Get());
}

void CommandList::InsertDebugMarker(std::string_view label, const float color[4]) {
    if (color) {
        PIXSetMarker(command_list.Get(),
                     PIX_COLOR(static_cast<BYTE>(color[0] * 255), static_cast<BYTE>(color[1] * 255),
                               static_cast<BYTE>(color[2] * 255)),
                     label.data());
    } else {
        PIXSetMarker(command_list.Get(), 0, label.data());
    }
}

} // namespace DX12


