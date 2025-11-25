# DirectX 12 Backend Implementation Guide for shadPS4Plus

## Overview

This document explains the DirectX 12 backend implementation for shadPS4Plus, including key differences from Vulkan and implementation strategies.

## Architecture

```
┌─────────────────────────────────────────┐
│     PS4 GNM/GNMX Command Processor     │
│         (Liverpool/AMD GCN)             │
└──────────────┬──────────────────────────┘
               │
               ├──────────────┬────────────┐
               │              │            │
      ┌────────▼─────┐  ┌────▼─────┐  ┌──▼────┐
      │  Abstract    │  │ Abstract │  │ ...   │
      │  Renderer    │  │ Pipeline │  │       │
      │  Interface   │  │ Interface│  │       │
      └────┬────┬────┘  └──────────┘  └───────┘
           │    │
    ┌──────▼─┐  └──────▼──┐
    │ Vulkan │  │ DirectX  │
    │ Backend│  │ 12       │
    │        │  │ Backend  │
    └────────┘  └──────────┘
```

## Key Differences: Vulkan vs DirectX 12

### 1. Resource State Management

**Vulkan:**
- Uses explicit pipeline barriers with `vkCmdPipelineBarrier`
- Image layouts: UNDEFINED, GENERAL, COLOR_ATTACHMENT_OPTIMAL, etc.
- Memory barriers specify src/dst access masks and stage masks

**DirectX 12:**
- Uses resource barriers with `ResourceBarrier()`
- Resource states: COMMON, RENDER_TARGET, DEPTH_WRITE, SHADER_RESOURCE, etc.
- Transition barriers specify before/after states
- UAV barriers for unordered access synchronization

**Implementation Strategy:**
```cpp
// Vulkan barrier
vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

// DirectX 12 equivalent
D3D12_RESOURCE_BARRIER barrier = {};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barrier.Transition.pResource = resource;
barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
cmdList->ResourceBarrier(1, &barrier);
```

### 2. Descriptor Management

**Vulkan:**
- Descriptor sets and layouts
- Descriptor pools for allocation
- Update descriptor sets with `vkUpdateDescriptorSets`
- Bind descriptor sets with `vkCmdBindDescriptorSets`

**DirectX 12:**
- Descriptor heaps (CBV/SRV/UAV, Sampler, RTV, DSV)
- Descriptor tables in root signature
- Copy descriptors or create views directly
- Set descriptor heaps and root descriptor tables

**Implementation Strategy:**
```cpp
// Vulkan
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);

// DirectX 12
ID3D12DescriptorHeap* heaps[] = { cbvSrvUavHeap, samplerHeap };
cmdList->SetDescriptorHeaps(2, heaps);
cmdList->SetGraphicsRootDescriptorTable(0, gpuHandle);
```

### 3. Root Signature vs Pipeline Layout

**Vulkan:**
- Pipeline layout defines descriptor set layouts
- Push constants for small data
- Separate object from pipeline

**DirectX 12:**
- Root signature defines root parameters
- Root constants (32-bit values)
- Root descriptors (inline CBV/SRV/UAV)
- Descriptor tables (ranges in heaps)
- Must be compatible with pipeline state

**PS4 GNM Mapping:**
```cpp
// PS4 has user data registers (up to 16 per stage)
// Map to root signature:
// - Root constants for small data (user data)
// - Root CBVs for constant buffers
// - Descriptor tables for textures/samplers
```

### 4. Command Queue Types

**Vulkan:**
- Queue families (graphics, compute, transfer)
- Submit command buffers to queues
- Semaphores and fences for synchronization

**DirectX 12:**
- Command queue types: DIRECT, COMPUTE, COPY
- Execute command lists on queues
- Fences for synchronization
- No semaphores (use fences instead)

### 5. Memory Management

**Vulkan:**
- Explicit memory allocation with `vkAllocateMemory`
- Bind memory to resources
- Memory types and heaps

**DirectX 12:**
- Committed resources (implicit allocation)
- Placed resources (explicit heap management)
- Upload heap, default heap, readback heap
- Simpler than Vulkan for basic cases

**PS4 GNM Mapping:**
```cpp
// PS4 uses Onion (CPU-visible) and Garlic (GPU-only) memory
// Map to D3D12:
// - Onion → D3D12_HEAP_TYPE_UPLOAD
// - Garlic → D3D12_HEAP_TYPE_DEFAULT
```

### 6. Shader Compilation

**Vulkan:**
- SPIR-V bytecode
- Use SPIRV-Cross or similar for translation

**DirectX 12:**
- DXIL (DirectX Intermediate Language)
- Use DXC (DirectX Shader Compiler)
- Can compile HLSL or translate SPIR-V

**Implementation Strategy:**
```cpp
// For shadPS4Plus:
// 1. Recompile PS4 shaders → SPIR-V (existing)
// 2. SPIR-V → HLSL (using SPIRV-Cross)
// 3. HLSL → DXIL (using DXC)
// OR
// 2. SPIR-V → DXIL (using spirv-to-dxil)
```

## Implementation Checklist

### Phase 1: Core Infrastructure ✓
- [x] Abstract renderer interface
- [x] DirectX 12 device creation
- [x] Command queue management
- [x] Swapchain creation
- [x] Descriptor heap management
- [x] Command list implementation

### Phase 2: Resource Management
- [ ] Buffer cache integration
- [ ] Texture cache integration
- [ ] Memory allocator
- [ ] Resource state tracking
- [ ] Descriptor allocation

### Phase 3: Pipeline Creation
- [ ] Root signature generation from PS4 user data
- [ ] Graphics pipeline state creation
- [ ] Compute pipeline state creation
- [ ] Shader translation (SPIR-V → DXIL)
- [ ] Input layout mapping

### Phase 4: Rendering
- [ ] Draw call translation
- [ ] Render pass implementation
- [ ] Resource binding
- [ ] Dynamic state updates
- [ ] Indirect draw support

### Phase 5: Advanced Features
- [ ] Compute shader dispatch
- [ ] Predication support
- [ ] Query objects
- [ ] Debug markers
- [ ] Performance optimization

## PS4 GNM to DirectX 12 Mapping

### Command Buffer Translation

| PS4 GNM Command | DirectX 12 Equivalent |
|-----------------|----------------------|
| `drawIndex` | `DrawIndexedInstanced` |
| `dispatch` | `Dispatch` |
| `setRenderTarget` | `OMSetRenderTargets` |
| `setViewport` | `RSSetViewports` |
| `setScissor` | `RSSetScissorRects` |
| `setPrimitiveType` | `IASetPrimitiveTopology` |
| `setVertexBuffers` | `IASetVertexBuffers` |
| `setIndexBuffer` | `IASetIndexBuffer` |

### Register Mapping

```cpp
// PS4 Context Registers → D3D12 State
// CB_COLOR0_BASE → RTV descriptor
// DB_Z_INFO → DSV descriptor
// VGT_PRIMITIVE_TYPE → D3D12_PRIMITIVE_TOPOLOGY
// PA_SC_SCREEN_SCISSOR → D3D12_RECT
// PA_CL_VPORT_XSCALE/XOFFSET → D3D12_VIEWPORT
```

### Shader Resource Binding

```cpp
// PS4 has T# (texture), V# (buffer), S# (sampler) descriptors
// Map to D3D12:
// T# → SRV (Shader Resource View)
// V# → CBV (Constant Buffer View) or SRV
// S# → Sampler
// UAV# → UAV (Unordered Access View)
```

## Performance Considerations

### 1. Descriptor Heap Management
- Pre-allocate large descriptor heaps
- Use ring buffer for dynamic descriptors
- Batch descriptor updates

### 2. Command List Recycling
- Pool command allocators per frame
- Reuse command lists
- Reset allocators only when frame is complete

### 3. Resource State Tracking
- Track current state per resource
- Minimize unnecessary transitions
- Batch barriers when possible

### 4. Upload Buffer Strategy
```cpp
// Use ring buffer for dynamic uploads
// 3 frames in flight × upload buffer size
// Fence-based synchronization
```

### 5. Pipeline State Object (PSO) Caching
- Hash pipeline state
- Cache PSOs to disk
- Lazy compilation when possible

## Debug and Validation

### DirectX 12 Debug Layer
```cpp
#ifdef _DEBUG
ComPtr<ID3D12Debug> debugController;
D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
debugController->EnableDebugLayer();
#endif
```

### PIX Integration
```cpp
// Use PIX markers for debugging
PIXBeginEvent(cmdList, 0, L"Draw Scene");
// ... rendering commands ...
PIXEndEvent(cmdList);
```

### GPU Validation
```cpp
// Enable GPU-based validation
ComPtr<ID3D12Debug1> debugController1;
debugController->QueryInterface(IID_PPV_ARGS(&debugController1));
debugController1->SetEnableGPUBasedValidation(TRUE);
```

## Testing Strategy

1. **Basic Rendering**
   - Clear screen
   - Simple triangle
   - Textured quad

2. **PS4 Game Tests**
   - Start with simple 2D games
   - Progress to 3D games
   - Test compute shaders

3. **Compatibility**
   - Compare output with Vulkan backend
   - Verify frame timing
   - Check memory usage

## Future Enhancements

- [ ] DirectX 12 Ultimate features (Mesh Shaders, Ray Tracing)
- [ ] Variable Rate Shading
- [ ] Sampler Feedback
- [ ] DirectStorage integration
- [ ] Auto HDR support

## References

- [DirectX 12 Programming Guide](https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide)
- [AMD GCN Architecture](https://gpuopen.com/learn/gcn-execution-model/)
- [PS4 GPU Documentation](https://www.slideshare.net/DICEStudio/directx-11-rendering-in-battlefield-3)
- [Vulkan to DirectX 12 Porting Guide](https://github.com/microsoft/DirectX-Graphics-Samples)
