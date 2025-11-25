#// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include <d3d12.h>
#include <wrl/client.h>
#include "video_core/renderer_interface.h"


using Microsoft::WRL::ComPtr;

namespace DX12 {

class Device;

/// DirectX 12 command list implementation
class CommandList : public VideoCore::ICommandList {
public:
    explicit CommandList(Device* device);
    ~CommandList() override;

    // ICommandList interface
    void Begin() override;
    void End() override;
    void Reset() override;
    void SetPipeline(VideoCore::IPipelineState* pipeline) override;
    void BindVertexBuffers(u32 first_binding, std::span<const VAddr> addresses,
                           std::span<const u32> strides) override;
    void BindIndexBuffer(VAddr address, u32 size, bool is_32bit) override;
    void SetViewport(float x, float y, float width, float height, float min_depth,
                     float max_depth) override;
    void SetScissor(s32 x, s32 y, u32 width, u32 height) override;
    void Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance) override;
    void DrawIndexed(u32 index_count, u32 instance_count, u32 first_index, s32 vertex_offset,
                     u32 first_instance) override;
    void DrawIndirect(VAddr args_address, u32 draw_count, u32 stride) override;
    void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) override;
    void DispatchIndirect(VAddr args_address) override;
    void CopyBuffer(VAddr dst, VAddr src, u64 size) override;
    void CopyBufferToTexture(VAddr src_buffer, VideoCore::ITexture* dst_texture, u32 mip_level,
                             u32 array_layer) override;
    void CopyTextureToBuffer(VideoCore::ITexture* src_texture, VAddr dst_buffer, u32 mip_level,
                             u32 array_layer) override;
    void BeginRenderPass(std::span<VideoCore::ITexture*> color_attachments,
                         VideoCore::ITexture* depth_attachment) override;
    void EndRenderPass() override;
    void Barrier(VideoCore::PipelineStage src_stage, VideoCore::PipelineStage dst_stage,
                 VideoCore::AccessFlags src_access, VideoCore::AccessFlags dst_access) override;
    void BeginDebugMarker(std::string_view label, const float color[4]) override;
    void EndDebugMarker() override;
    void InsertDebugMarker(std::string_view label, const float color[4]) override;

    // DirectX 12 specific methods
    ID3D12GraphicsCommandList* GetCommandList() const {
        return command_list.Get();
    }

private:
    void CreateCommandAllocator();
    void CreateCommandList();

    Device* device;
    ComPtr<ID3D12CommandAllocator> command_allocator;
    ComPtr<ID3D12GraphicsCommandList> command_list;
    bool is_recording = false;
};

} // namespace DX12
