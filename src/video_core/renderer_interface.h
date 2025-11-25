// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <span>
#include <string_view>
#include "common/types.h"
#include "video_core/amdgpu/liverpool.h"


namespace VideoCore {

// Forward declarations
class BufferCache;
class TextureCache;
class PageManager;

/// Graphics API backend type
enum class GraphicsAPI {
    Vulkan,
    DirectX12,
};

/// Resource format (maps to PS4 GNM formats)
enum class PixelFormat {
    Unknown,
    R8G8B8A8_UNORM,
    B8G8R8A8_UNORM,
    R32G32B32A32_FLOAT,
    D32_FLOAT,
    D24_UNORM_S8_UINT,
    // Add more formats as needed
};

/// Pipeline stage flags
enum class PipelineStage : u32 {
    VertexShader = 1 << 0,
    PixelShader = 1 << 1,
    ComputeShader = 1 << 2,
    Transfer = 1 << 3,
    AllGraphics = VertexShader | PixelShader,
    AllCommands = 0xFFFFFFFF,
};

/// Resource access flags
enum class AccessFlags : u32 {
    None = 0,
    ShaderRead = 1 << 0,
    ShaderWrite = 1 << 1,
    ColorAttachmentRead = 1 << 2,
    ColorAttachmentWrite = 1 << 3,
    DepthStencilRead = 1 << 4,
    DepthStencilWrite = 1 << 5,
    TransferRead = 1 << 6,
    TransferWrite = 1 << 7,
    MemoryRead = 1 << 8,
    MemoryWrite = 1 << 9,
};

/// Abstract GPU device interface
class IGpuDevice {
public:
    virtual ~IGpuDevice() = default;

    /// Get the graphics API type
    virtual GraphicsAPI GetAPI() const = 0;

    /// Wait for device to be idle
    virtual void WaitIdle() = 0;

    /// Create a command list
    virtual class ICommandList* CreateCommandList() = 0;

    /// Submit command lists for execution
    virtual void Submit(std::span<ICommandList*> command_lists) = 0;

    /// Present the swapchain
    virtual void Present() = 0;

    /// Get buffer cache
    virtual BufferCache& GetBufferCache() = 0;

    /// Get texture cache
    virtual TextureCache& GetTextureCache() = 0;
};

/// Abstract command list interface
class ICommandList {
public:
    virtual ~ICommandList() = default;

    /// Begin recording commands
    virtual void Begin() = 0;

    /// End recording commands
    virtual void End() = 0;

    /// Reset the command list for reuse
    virtual void Reset() = 0;

    /// Set pipeline state
    virtual void SetPipeline(class IPipelineState* pipeline) = 0;

    /// Bind vertex buffers
    virtual void BindVertexBuffers(u32 first_binding, std::span<const VAddr> addresses,
                                   std::span<const u32> strides) = 0;

    /// Bind index buffer
    virtual void BindIndexBuffer(VAddr address, u32 size, bool is_32bit) = 0;

    /// Set viewport
    virtual void SetViewport(float x, float y, float width, float height, float min_depth,
                             float max_depth) = 0;

    /// Set scissor rectangle
    virtual void SetScissor(s32 x, s32 y, u32 width, u32 height) = 0;

    /// Draw primitives
    virtual void Draw(u32 vertex_count, u32 instance_count, u32 first_vertex,
                      u32 first_instance) = 0;

    /// Draw indexed primitives
    virtual void DrawIndexed(u32 index_count, u32 instance_count, u32 first_index,
                             s32 vertex_offset, u32 first_instance) = 0;

    /// Draw indirect
    virtual void DrawIndirect(VAddr args_address, u32 draw_count, u32 stride) = 0;

    /// Dispatch compute shader
    virtual void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) = 0;

    /// Dispatch compute indirect
    virtual void DispatchIndirect(VAddr args_address) = 0;

    /// Copy buffer to buffer
    virtual void CopyBuffer(VAddr dst, VAddr src, u64 size) = 0;

    /// Copy buffer to texture
    virtual void CopyBufferToTexture(VAddr src_buffer, class ITexture* dst_texture, u32 mip_level,
                                     u32 array_layer) = 0;

    /// Copy texture to buffer
    virtual void CopyTextureToBuffer(class ITexture* src_texture, VAddr dst_buffer, u32 mip_level,
                                     u32 array_layer) = 0;

    /// Begin render pass
    virtual void BeginRenderPass(std::span<class ITexture*> color_attachments,
                                 ITexture* depth_attachment) = 0;

    /// End render pass
    virtual void EndRenderPass() = 0;

    /// Insert pipeline barrier
    virtual void Barrier(PipelineStage src_stage, PipelineStage dst_stage, AccessFlags src_access,
                         AccessFlags dst_access) = 0;

    /// Begin debug marker
    virtual void BeginDebugMarker(std::string_view label, const float color[4] = nullptr) = 0;

    /// End debug marker
    virtual void EndDebugMarker() = 0;

    /// Insert debug marker
    virtual void InsertDebugMarker(std::string_view label, const float color[4] = nullptr) = 0;
};

/// Abstract texture interface
class ITexture {
public:
    virtual ~ITexture() = default;

    /// Get texture width
    virtual u32 GetWidth() const = 0;

    /// Get texture height
    virtual u32 GetHeight() const = 0;

    /// Get texture depth
    virtual u32 GetDepth() const = 0;

    /// Get mip level count
    virtual u32 GetMipLevels() const = 0;

    /// Get array layer count
    virtual u32 GetArrayLayers() const = 0;

    /// Get pixel format
    virtual PixelFormat GetFormat() const = 0;

    /// Get native handle (VkImage or ID3D12Resource*)
    virtual void* GetNativeHandle() const = 0;
};

/// Abstract buffer interface
class IBuffer {
public:
    virtual ~IBuffer() = default;

    /// Get buffer size
    virtual u64 GetSize() const = 0;

    /// Get buffer address
    virtual VAddr GetAddress() const = 0;

    /// Map buffer for CPU access
    virtual void* Map() = 0;

    /// Unmap buffer
    virtual void Unmap() = 0;

    /// Get native handle (VkBuffer or ID3D12Resource*)
    virtual void* GetNativeHandle() const = 0;
};

/// Abstract pipeline state interface
class IPipelineState {
public:
    virtual ~IPipelineState() = default;

    /// Get pipeline type (graphics or compute)
    virtual bool IsCompute() const = 0;

    /// Get native handle (VkPipeline or ID3D12PipelineState*)
    virtual void* GetNativeHandle() const = 0;
};

/// Abstract shader module interface
class IShaderModule {
public:
    virtual ~IShaderModule() = default;

    /// Get shader bytecode
    virtual std::span<const u8> GetBytecode() const = 0;

    /// Get native handle (VkShaderModule or ID3DBlob*)
    virtual void* GetNativeHandle() const = 0;
};

/// Rasterizer interface (common for all backends)
class IRasterizer {
public:
    virtual ~IRasterizer() = default;

    /// Draw primitives
    virtual void Draw(bool is_indexed, u32 index_offset = 0) = 0;

    /// Draw indirect
    virtual void DrawIndirect(bool is_indexed, VAddr arg_address, u32 offset, u32 size,
                              u32 max_count, VAddr count_address) = 0;

    /// Dispatch compute shader
    virtual void DispatchDirect() = 0;

    /// Dispatch compute indirect
    virtual void DispatchIndirect(VAddr address, u32 offset, u32 size) = 0;

    /// Scope markers for debugging
    virtual void ScopeMarkerBegin(std::string_view str, bool from_guest = false) = 0;
    virtual void ScopeMarkerEnd(bool from_guest = false) = 0;
    virtual void ScopedMarkerInsert(std::string_view str, bool from_guest = false) = 0;
    virtual void ScopedMarkerInsertColor(std::string_view str, u32 color,
                                         bool from_guest = false) = 0;

    /// Set GPU predication state
    virtual void SetPredication(const AmdGpu::Liverpool::PredicationState& state) = 0;

    /// Inline data upload
    virtual void InlineData(VAddr address, const void* value, u32 num_bytes, bool is_gds) = 0;

    /// Copy buffer
    virtual void CopyBuffer(VAddr dst, VAddr src, u32 num_bytes, bool dst_gds, bool src_gds) = 0;

    /// Read data from GDS (Global Data Share)
    virtual u32 ReadDataFromGds(u32 gsd_offset) = 0;

    /// Memory management
    virtual bool InvalidateMemory(VAddr addr, u64 size) = 0;
    virtual bool ReadMemory(VAddr addr, u64 size) = 0;
    virtual bool IsMapped(VAddr addr, u64 size) = 0;
    virtual void MapMemory(VAddr addr, u64 size) = 0;
    virtual void UnmapMemory(VAddr addr, u64 size) = 0;

    /// Synchronization
    virtual void CpSync() = 0;
    virtual u64 Flush() = 0;
    virtual void Finish() = 0;
    virtual void OnSubmit() = 0;

    /// Get buffer cache
    virtual BufferCache& GetBufferCache() = 0;

    /// Get texture cache
    virtual TextureCache& GetTextureCache() = 0;
};

} // namespace VideoCore
