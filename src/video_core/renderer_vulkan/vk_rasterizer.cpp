// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "video_core/control/channel_state.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/blit_image.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/shader_cache.h"
#include "video_core/texture_cache/texture_cache_base.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using VideoCommon::ImageViewId;
using VideoCommon::ImageViewType;

MICROPROFILE_DEFINE(Vulkan_WaitForWorker, "Vulkan", "Wait for worker", MP_RGB(255, 192, 192));
MICROPROFILE_DEFINE(Vulkan_Drawing, "Vulkan", "Record drawing", MP_RGB(192, 128, 128));
MICROPROFILE_DEFINE(Vulkan_Compute, "Vulkan", "Record compute", MP_RGB(192, 128, 128));
MICROPROFILE_DEFINE(Vulkan_Clearing, "Vulkan", "Record clearing", MP_RGB(192, 128, 128));
MICROPROFILE_DEFINE(Vulkan_PipelineCache, "Vulkan", "Pipeline cache", MP_RGB(192, 128, 128));

namespace {
struct DrawParams {
    u32 base_instance;
    u32 num_instances;
    u32 base_vertex;
    u32 num_vertices;
    u32 first_index;
    bool is_indexed;
};

VkViewport GetViewportState(const Device& device, const Maxwell& regs, size_t index, float scale) {
    const auto& src = regs.viewport_transform[index];
    const auto conv = [scale](float value) {
        float new_value = value * scale;
        if (scale < 1.0f) {
            const bool sign = std::signbit(value);
            new_value = std::round(std::abs(new_value));
            new_value = sign ? -new_value : new_value;
        }
        return new_value;
    };
    const float x = conv(src.translate_x - src.scale_x);
    const float width = conv(src.scale_x * 2.0f);
    float y = conv(src.translate_y - src.scale_y);
    float height = conv(src.scale_y * 2.0f);
    if (regs.screen_y_control.y_negate) {
        y += height;
        height = -height;
    }
    const float reduce_z = regs.depth_mode == Maxwell::DepthMode::MinusOneToOne ? 1.0f : 0.0f;
    VkViewport viewport{
        .x = x,
        .y = y,
        .width = width != 0.0f ? width : 1.0f,
        .height = height != 0.0f ? height : 1.0f,
        .minDepth = src.translate_z - src.scale_z * reduce_z,
        .maxDepth = src.translate_z + src.scale_z,
    };
    if (!device.IsExtDepthRangeUnrestrictedSupported()) {
        viewport.minDepth = std::clamp(viewport.minDepth, 0.0f, 1.0f);
        viewport.maxDepth = std::clamp(viewport.maxDepth, 0.0f, 1.0f);
    }
    return viewport;
}

VkRect2D GetScissorState(const Maxwell& regs, size_t index, u32 up_scale = 1, u32 down_shift = 0) {
    const auto& src = regs.scissor_test[index];
    VkRect2D scissor;
    const auto scale_up = [&](s32 value) -> s32 {
        if (value == 0) {
            return 0U;
        }
        const s32 upset = value * up_scale;
        s32 acumm = 0;
        if ((up_scale >> down_shift) == 0) {
            acumm = upset % 2;
        }
        const s32 converted_value = (value * up_scale) >> down_shift;
        return value < 0 ? std::min<s32>(converted_value - acumm, -1)
                         : std::max<s32>(converted_value + acumm, 1);
    };
    if (src.enable) {
        scissor.offset.x = scale_up(static_cast<s32>(src.min_x));
        scissor.offset.y = scale_up(static_cast<s32>(src.min_y));
        scissor.extent.width = scale_up(src.max_x - src.min_x);
        scissor.extent.height = scale_up(src.max_y - src.min_y);
    } else {
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = std::numeric_limits<s32>::max();
        scissor.extent.height = std::numeric_limits<s32>::max();
    }
    return scissor;
}

DrawParams MakeDrawParams(const Maxwell& regs, u32 num_instances, bool is_instanced,
                          bool is_indexed) {
    DrawParams params{
        .base_instance = regs.vb_base_instance,
        .num_instances = is_instanced ? num_instances : 1,
        .base_vertex = is_indexed ? regs.vb_element_base : regs.vertex_buffer.first,
        .num_vertices = is_indexed ? regs.index_array.count : regs.vertex_buffer.count,
        .first_index = is_indexed ? regs.index_array.first : 0,
        .is_indexed = is_indexed,
    };
    if (regs.draw.topology == Maxwell::PrimitiveTopology::Quads) {
        // 6 triangle vertices per quad, base vertex is part of the index
        // See BindQuadArrayIndexBuffer for more details
        params.num_vertices = (params.num_vertices / 4) * 6;
        params.base_vertex = 0;
        params.is_indexed = true;
    }
    return params;
}
} // Anonymous namespace

RasterizerVulkan::RasterizerVulkan(Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu_,
                                   Core::Memory::Memory& cpu_memory_, ScreenInfo& screen_info_,
                                   const Device& device_, MemoryAllocator& memory_allocator_,
                                   StateTracker& state_tracker_, Scheduler& scheduler_)
    : RasterizerAccelerated{cpu_memory_}, gpu{gpu_}, screen_info{screen_info_}, device{device_},
      memory_allocator{memory_allocator_}, state_tracker{state_tracker_}, scheduler{scheduler_},
      staging_pool(device, memory_allocator, scheduler), descriptor_pool(device, scheduler),
      update_descriptor_queue(device, scheduler),
      blit_image(device, scheduler, state_tracker, descriptor_pool),
      astc_decoder_pass(device, scheduler, descriptor_pool, staging_pool, update_descriptor_queue,
                        memory_allocator),
      render_pass_cache(device), texture_cache_runtime{device,           scheduler,
                                                       memory_allocator, staging_pool,
                                                       blit_image,       astc_decoder_pass,
                                                       render_pass_cache},
      texture_cache(texture_cache_runtime, *this),
      buffer_cache_runtime(device, memory_allocator, scheduler, staging_pool,
                           update_descriptor_queue, descriptor_pool),
      buffer_cache(*this, cpu_memory_, buffer_cache_runtime),
      pipeline_cache(*this, device, scheduler, descriptor_pool, update_descriptor_queue,
                     render_pass_cache, buffer_cache, texture_cache, gpu.ShaderNotify()),
      query_cache{*this, device, scheduler}, accelerate_dma{buffer_cache},
      fence_manager(*this, gpu, texture_cache, buffer_cache, query_cache, device, scheduler),
      wfi_event(device.GetLogical().CreateEvent()) {
    scheduler.SetQueryCache(query_cache);
}

RasterizerVulkan::~RasterizerVulkan() = default;

void RasterizerVulkan::Draw(bool is_indexed, bool is_instanced) {
    MICROPROFILE_SCOPE(Vulkan_Drawing);

    SCOPE_EXIT({ gpu.TickWork(); });
    FlushWork();

    query_cache.UpdateCounters();

    GraphicsPipeline* const pipeline{pipeline_cache.CurrentGraphicsPipeline()};
    if (!pipeline) {
        return;
    }
    std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
    // update engine as channel may be different.
    pipeline->SetEngine(maxwell3d, gpu_memory);
    pipeline->Configure(is_indexed);

    BeginTransformFeedback();

    UpdateDynamicStates();

    const auto& regs{maxwell3d->regs};
    const u32 num_instances{maxwell3d->mme_draw.instance_count};
    const DrawParams draw_params{MakeDrawParams(regs, num_instances, is_instanced, is_indexed)};
    scheduler.Record([draw_params](vk::CommandBuffer cmdbuf) {
        if (draw_params.is_indexed) {
            cmdbuf.DrawIndexed(draw_params.num_vertices, draw_params.num_instances,
                               draw_params.first_index, draw_params.base_vertex,
                               draw_params.base_instance);
        } else {
            cmdbuf.Draw(draw_params.num_vertices, draw_params.num_instances,
                        draw_params.base_vertex, draw_params.base_instance);
        }
    });
    EndTransformFeedback();
}

void RasterizerVulkan::Clear() {
    MICROPROFILE_SCOPE(Vulkan_Clearing);

    if (!maxwell3d->ShouldExecute()) {
        return;
    }
    FlushWork();

    query_cache.UpdateCounters();

    auto& regs = maxwell3d->regs;
    const bool use_color = regs.clear_buffers.R || regs.clear_buffers.G || regs.clear_buffers.B ||
                           regs.clear_buffers.A;
    const bool use_depth = regs.clear_buffers.Z;
    const bool use_stencil = regs.clear_buffers.S;
    if (!use_color && !use_depth && !use_stencil) {
        return;
    }

    std::scoped_lock lock{texture_cache.mutex};
    texture_cache.UpdateRenderTargets(true);
    const Framebuffer* const framebuffer = texture_cache.GetFramebuffer();
    const VkExtent2D render_area = framebuffer->RenderArea();
    scheduler.RequestRenderpass(framebuffer);

    u32 up_scale = 1;
    u32 down_shift = 0;
    if (texture_cache.IsRescaling()) {
        up_scale = Settings::values.resolution_info.up_scale;
        down_shift = Settings::values.resolution_info.down_shift;
    }
    UpdateViewportsState(regs);

    VkRect2D default_scissor;
    default_scissor.offset.x = 0;
    default_scissor.offset.y = 0;
    default_scissor.extent.width = std::numeric_limits<s32>::max();
    default_scissor.extent.height = std::numeric_limits<s32>::max();

    VkClearRect clear_rect{
        .rect = regs.clear_flags.scissor ? GetScissorState(regs, 0, up_scale, down_shift)
                                         : default_scissor,
        .baseArrayLayer = regs.clear_buffers.layer,
        .layerCount = 1,
    };
    if (clear_rect.rect.extent.width == 0 || clear_rect.rect.extent.height == 0) {
        return;
    }
    clear_rect.rect.extent = VkExtent2D{
        .width = std::min(clear_rect.rect.extent.width, render_area.width),
        .height = std::min(clear_rect.rect.extent.height, render_area.height),
    };

    const u32 color_attachment = regs.clear_buffers.RT;
    if (use_color && framebuffer->HasAspectColorBit(color_attachment)) {
        VkClearValue clear_value;
        bool is_integer = false;
        bool is_signed = false;
        size_t int_size = 8;
        for (std::size_t i = 0; i < Tegra::Engines::Maxwell3D::Regs::NumRenderTargets; ++i) {
            const auto& this_rt = regs.rt[i];
            if (this_rt.Address() == 0) {
                continue;
            }
            if (this_rt.format == Tegra::RenderTargetFormat::NONE) {
                continue;
            }
            const auto format =
                VideoCore::Surface::PixelFormatFromRenderTargetFormat(this_rt.format);
            is_integer = IsPixelFormatInteger(format);
            is_signed = IsPixelFormatSignedInteger(format);
            int_size = PixelComponentSizeBitsInteger(format);
            break;
        }
        if (!is_integer) {
            std::memcpy(clear_value.color.float32, regs.clear_color, sizeof(regs.clear_color));
        } else if (!is_signed) {
            for (size_t i = 0; i < 4; i++) {
                clear_value.color.uint32[i] = static_cast<u32>(
                    static_cast<f32>(static_cast<u64>(int_size) << 1U) * regs.clear_color[i]);
            }
        } else {
            for (size_t i = 0; i < 4; i++) {
                clear_value.color.int32[i] =
                    static_cast<s32>(static_cast<f32>(static_cast<s64>(int_size - 1) << 1) *
                                     (regs.clear_color[i] - 0.5f));
            }
        }

        scheduler.Record([color_attachment, clear_value, clear_rect](vk::CommandBuffer cmdbuf) {
            const VkClearAttachment attachment{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = color_attachment,
                .clearValue = clear_value,
            };
            cmdbuf.ClearAttachments(attachment, clear_rect);
        });
    }

    if (!use_depth && !use_stencil) {
        return;
    }
    VkImageAspectFlags aspect_flags = 0;
    if (use_depth && framebuffer->HasAspectDepthBit()) {
        aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (use_stencil && framebuffer->HasAspectStencilBit()) {
        aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    if (aspect_flags == 0) {
        return;
    }
    scheduler.Record([clear_depth = regs.clear_depth, clear_stencil = regs.clear_stencil,
                      clear_rect, aspect_flags](vk::CommandBuffer cmdbuf) {
        VkClearAttachment attachment;
        attachment.aspectMask = aspect_flags;
        attachment.colorAttachment = 0;
        attachment.clearValue.depthStencil.depth = clear_depth;
        attachment.clearValue.depthStencil.stencil = clear_stencil;
        cmdbuf.ClearAttachments(attachment, clear_rect);
    });
}

void RasterizerVulkan::DispatchCompute() {
    FlushWork();

    ComputePipeline* const pipeline{pipeline_cache.CurrentComputePipeline()};
    if (!pipeline) {
        return;
    }
    std::scoped_lock lock{texture_cache.mutex, buffer_cache.mutex};
    pipeline->Configure(*kepler_compute, *gpu_memory, scheduler, buffer_cache, texture_cache);

    const auto& qmd{kepler_compute->launch_description};
    const std::array<u32, 3> dim{qmd.grid_dim_x, qmd.grid_dim_y, qmd.grid_dim_z};
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([dim](vk::CommandBuffer cmdbuf) { cmdbuf.Dispatch(dim[0], dim[1], dim[2]); });
}

void RasterizerVulkan::ResetCounter(VideoCore::QueryType type) {
    query_cache.ResetCounter(type);
}

void RasterizerVulkan::Query(GPUVAddr gpu_addr, VideoCore::QueryType type,
                             std::optional<u64> timestamp) {
    query_cache.Query(gpu_addr, type, timestamp);
}

void RasterizerVulkan::BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr,
                                                 u32 size) {
    buffer_cache.BindGraphicsUniformBuffer(stage, index, gpu_addr, size);
}

void Vulkan::RasterizerVulkan::DisableGraphicsUniformBuffer(size_t stage, u32 index) {
    buffer_cache.DisableGraphicsUniformBuffer(stage, index);
}

void RasterizerVulkan::FlushAll() {}

void RasterizerVulkan::FlushRegion(VAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.DownloadMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.DownloadMemory(addr, size);
    }
    query_cache.FlushRegion(addr, size);
}

bool RasterizerVulkan::MustFlushRegion(VAddr addr, u64 size) {
    std::scoped_lock lock{texture_cache.mutex, buffer_cache.mutex};
    if (!Settings::IsGPULevelHigh()) {
        return buffer_cache.IsRegionGpuModified(addr, size);
    }
    return texture_cache.IsRegionGpuModified(addr, size) ||
           buffer_cache.IsRegionGpuModified(addr, size);
}

void RasterizerVulkan::InvalidateRegion(VAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.WriteMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    pipeline_cache.InvalidateRegion(addr, size);
    query_cache.InvalidateRegion(addr, size);
}

void RasterizerVulkan::OnCPUWrite(VAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }
    pipeline_cache.OnCPUWrite(addr, size);
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.WriteMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.CachedWriteMemory(addr, size);
    }
}

void RasterizerVulkan::InvalidateGPUCache() {
    pipeline_cache.SyncGuestHost();
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.FlushCachedWrites();
    }
}

void RasterizerVulkan::UnmapMemory(VAddr addr, u64 size) {
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.UnmapMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    pipeline_cache.OnCPUWrite(addr, size);
}

void RasterizerVulkan::ModifyGPUMemory(size_t as_id, GPUVAddr addr, u64 size) {
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.UnmapGPUMemory(as_id, addr, size);
    }
}

void RasterizerVulkan::SignalFence(std::function<void()>&& func) {
    fence_manager.SignalFence(std::move(func));
}

void RasterizerVulkan::SyncOperation(std::function<void()>&& func) {
    fence_manager.SyncOperation(std::move(func));
}

void RasterizerVulkan::SignalSyncPoint(u32 value) {
    fence_manager.SignalSyncPoint(value);
}

void RasterizerVulkan::SignalReference() {
    fence_manager.SignalOrdering();
}

void RasterizerVulkan::ReleaseFences() {
    fence_manager.WaitPendingFences();
}

void RasterizerVulkan::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    if (Settings::IsGPULevelExtreme()) {
        FlushRegion(addr, size);
    }
    InvalidateRegion(addr, size);
}

void RasterizerVulkan::WaitForIdle() {
    // Everything but wait pixel operations. This intentionally includes FRAGMENT_SHADER_BIT because
    // fragment shaders can still write storage buffers.
    VkPipelineStageFlags flags =
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
        VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
        VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (device.IsExtTransformFeedbackSupported()) {
        flags |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;
    }

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([event = *wfi_event, flags](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetEvent(event, flags);
        cmdbuf.WaitEvents(event, flags, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, {}, {}, {});
    });
    SignalReference();
}

void RasterizerVulkan::FragmentBarrier() {
    // We already put barriers when a render pass finishes
    scheduler.RequestOutsideRenderPassOperationContext();
}

void RasterizerVulkan::TiledCacheBarrier() {
    // TODO: Implementing tiled barriers requires rewriting a good chunk of the Vulkan backend
}

void RasterizerVulkan::FlushCommands() {
    if (draw_counter == 0) {
        return;
    }
    draw_counter = 0;
    scheduler.Flush();
}

void RasterizerVulkan::TickFrame() {
    draw_counter = 0;
    update_descriptor_queue.TickFrame();
    fence_manager.TickFrame();
    staging_pool.TickFrame();
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.TickFrame();
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.TickFrame();
    }
}

bool RasterizerVulkan::AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Surface& src,
                                             const Tegra::Engines::Fermi2D::Surface& dst,
                                             const Tegra::Engines::Fermi2D::Config& copy_config) {
    std::scoped_lock lock{texture_cache.mutex};
    texture_cache.BlitImage(dst, src, copy_config);
    return true;
}

Tegra::Engines::AccelerateDMAInterface& RasterizerVulkan::AccessAccelerateDMA() {
    return accelerate_dma;
}

void RasterizerVulkan::AccelerateInlineToMemory(GPUVAddr address, size_t copy_size,
                                                std::span<const u8> memory) {
    auto cpu_addr = gpu_memory->GpuToCpuAddress(address);
    if (!cpu_addr) [[unlikely]] {
        gpu_memory->WriteBlock(address, memory.data(), copy_size);
        return;
    }
    gpu_memory->WriteBlockUnsafe(address, memory.data(), copy_size);
    {
        std::unique_lock<std::mutex> lock{buffer_cache.mutex};
        if (!buffer_cache.InlineMemory(*cpu_addr, copy_size, memory)) {
            buffer_cache.WriteMemory(*cpu_addr, copy_size);
        }
    }
    {
        std::scoped_lock lock_texture{texture_cache.mutex};
        texture_cache.WriteMemory(*cpu_addr, copy_size);
    }
    pipeline_cache.InvalidateRegion(*cpu_addr, copy_size);
    query_cache.InvalidateRegion(*cpu_addr, copy_size);
}

bool RasterizerVulkan::AccelerateDisplay(const Tegra::FramebufferConfig& config,
                                         VAddr framebuffer_addr, u32 pixel_stride) {
    if (!framebuffer_addr) {
        return false;
    }
    std::scoped_lock lock{texture_cache.mutex};
    ImageView* const image_view = texture_cache.TryFindFramebufferImageView(framebuffer_addr);
    if (!image_view) {
        return false;
    }
    screen_info.image_view = image_view->Handle(Shader::TextureType::Color2D);
    screen_info.width = image_view->size.width;
    screen_info.height = image_view->size.height;
    screen_info.is_srgb = VideoCore::Surface::IsPixelFormatSRGB(image_view->format);
    return true;
}

void RasterizerVulkan::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                         const VideoCore::DiskResourceLoadCallback& callback) {
    pipeline_cache.LoadDiskResources(title_id, stop_loading, callback);
}

void RasterizerVulkan::FlushWork() {
    static constexpr u32 DRAWS_TO_DISPATCH = 4096;

    // Only check multiples of 8 draws
    static_assert(DRAWS_TO_DISPATCH % 8 == 0);
    if ((++draw_counter & 7) != 7) {
        return;
    }
    if (draw_counter < DRAWS_TO_DISPATCH) {
        // Send recorded tasks to the worker thread
        scheduler.DispatchWork();
        return;
    }
    // Otherwise (every certain number of draws) flush execution.
    // This submits commands to the Vulkan driver.
    scheduler.Flush();
    draw_counter = 0;
}

AccelerateDMA::AccelerateDMA(BufferCache& buffer_cache_) : buffer_cache{buffer_cache_} {}

bool AccelerateDMA::BufferClear(GPUVAddr src_address, u64 amount, u32 value) {
    std::scoped_lock lock{buffer_cache.mutex};
    return buffer_cache.DMAClear(src_address, amount, value);
}

bool AccelerateDMA::BufferCopy(GPUVAddr src_address, GPUVAddr dest_address, u64 amount) {
    std::scoped_lock lock{buffer_cache.mutex};
    return buffer_cache.DMACopy(src_address, dest_address, amount);
}

void RasterizerVulkan::UpdateDynamicStates() {
    auto& regs = maxwell3d->regs;
    UpdateViewportsState(regs);
    UpdateScissorsState(regs);
    UpdateDepthBias(regs);
    UpdateBlendConstants(regs);
    UpdateDepthBounds(regs);
    UpdateStencilFaces(regs);
    UpdateLineWidth(regs);
    if (device.IsExtExtendedDynamicStateSupported()) {
        UpdateCullMode(regs);
        UpdateDepthBoundsTestEnable(regs);
        UpdateDepthTestEnable(regs);
        UpdateDepthWriteEnable(regs);
        UpdateDepthCompareOp(regs);
        UpdateFrontFace(regs);
        UpdateStencilOp(regs);
        UpdateStencilTestEnable(regs);
        if (device.IsExtVertexInputDynamicStateSupported()) {
            UpdateVertexInput(regs);
        }
    }
}

void RasterizerVulkan::BeginTransformFeedback() {
    const auto& regs = maxwell3d->regs;
    if (regs.tfb_enabled == 0) {
        return;
    }
    if (!device.IsExtTransformFeedbackSupported()) {
        LOG_ERROR(Render_Vulkan, "Transform feedbacks used but not supported");
        return;
    }
    UNIMPLEMENTED_IF(regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::TesselationControl) ||
                     regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::TesselationEval) ||
                     regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::Geometry));
    scheduler.Record(
        [](vk::CommandBuffer cmdbuf) { cmdbuf.BeginTransformFeedbackEXT(0, 0, nullptr, nullptr); });
}

void RasterizerVulkan::EndTransformFeedback() {
    const auto& regs = maxwell3d->regs;
    if (regs.tfb_enabled == 0) {
        return;
    }
    if (!device.IsExtTransformFeedbackSupported()) {
        return;
    }
    scheduler.Record(
        [](vk::CommandBuffer cmdbuf) { cmdbuf.EndTransformFeedbackEXT(0, 0, nullptr, nullptr); });
}

void RasterizerVulkan::UpdateViewportsState(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchViewports()) {
        return;
    }
    const bool is_rescaling{texture_cache.IsRescaling()};
    const float scale = is_rescaling ? Settings::values.resolution_info.up_factor : 1.0f;
    const std::array viewports{
        GetViewportState(device, regs, 0, scale),  GetViewportState(device, regs, 1, scale),
        GetViewportState(device, regs, 2, scale),  GetViewportState(device, regs, 3, scale),
        GetViewportState(device, regs, 4, scale),  GetViewportState(device, regs, 5, scale),
        GetViewportState(device, regs, 6, scale),  GetViewportState(device, regs, 7, scale),
        GetViewportState(device, regs, 8, scale),  GetViewportState(device, regs, 9, scale),
        GetViewportState(device, regs, 10, scale), GetViewportState(device, regs, 11, scale),
        GetViewportState(device, regs, 12, scale), GetViewportState(device, regs, 13, scale),
        GetViewportState(device, regs, 14, scale), GetViewportState(device, regs, 15, scale),
    };
    scheduler.Record([viewports](vk::CommandBuffer cmdbuf) { cmdbuf.SetViewport(0, viewports); });
}

void RasterizerVulkan::UpdateScissorsState(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchScissors()) {
        return;
    }
    u32 up_scale = 1;
    u32 down_shift = 0;
    if (texture_cache.IsRescaling()) {
        up_scale = Settings::values.resolution_info.up_scale;
        down_shift = Settings::values.resolution_info.down_shift;
    }
    const std::array scissors{
        GetScissorState(regs, 0, up_scale, down_shift),
        GetScissorState(regs, 1, up_scale, down_shift),
        GetScissorState(regs, 2, up_scale, down_shift),
        GetScissorState(regs, 3, up_scale, down_shift),
        GetScissorState(regs, 4, up_scale, down_shift),
        GetScissorState(regs, 5, up_scale, down_shift),
        GetScissorState(regs, 6, up_scale, down_shift),
        GetScissorState(regs, 7, up_scale, down_shift),
        GetScissorState(regs, 8, up_scale, down_shift),
        GetScissorState(regs, 9, up_scale, down_shift),
        GetScissorState(regs, 10, up_scale, down_shift),
        GetScissorState(regs, 11, up_scale, down_shift),
        GetScissorState(regs, 12, up_scale, down_shift),
        GetScissorState(regs, 13, up_scale, down_shift),
        GetScissorState(regs, 14, up_scale, down_shift),
        GetScissorState(regs, 15, up_scale, down_shift),
    };
    scheduler.Record([scissors](vk::CommandBuffer cmdbuf) { cmdbuf.SetScissor(0, scissors); });
}

void RasterizerVulkan::UpdateDepthBias(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthBias()) {
        return;
    }
    float units = regs.polygon_offset_units / 2.0f;
    const bool is_d24 = regs.zeta.format == Tegra::DepthFormat::S8_UINT_Z24_UNORM ||
                        regs.zeta.format == Tegra::DepthFormat::D24X8_UNORM ||
                        regs.zeta.format == Tegra::DepthFormat::D24S8_UNORM ||
                        regs.zeta.format == Tegra::DepthFormat::D24C8_UNORM;
    if (is_d24 && !device.SupportsD24DepthBuffer()) {
        // the base formulas can be obtained from here:
        //   https://docs.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage-depth-bias
        const double rescale_factor =
            static_cast<double>(1ULL << (32 - 24)) / (static_cast<double>(0x1.ep+127));
        units = static_cast<float>(static_cast<double>(units) * rescale_factor);
    }
    scheduler.Record([constant = units, clamp = regs.polygon_offset_clamp,
                      factor = regs.polygon_offset_factor](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthBias(constant, clamp, factor);
    });
}

void RasterizerVulkan::UpdateBlendConstants(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchBlendConstants()) {
        return;
    }
    const std::array blend_color = {regs.blend_color.r, regs.blend_color.g, regs.blend_color.b,
                                    regs.blend_color.a};
    scheduler.Record(
        [blend_color](vk::CommandBuffer cmdbuf) { cmdbuf.SetBlendConstants(blend_color.data()); });
}

void RasterizerVulkan::UpdateDepthBounds(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthBounds()) {
        return;
    }
    scheduler.Record([min = regs.depth_bounds[0], max = regs.depth_bounds[1]](
                         vk::CommandBuffer cmdbuf) { cmdbuf.SetDepthBounds(min, max); });
}

void RasterizerVulkan::UpdateStencilFaces(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchStencilProperties()) {
        return;
    }
    if (regs.stencil_two_side_enable) {
        // Separate values per face
        scheduler.Record(
            [front_ref = regs.stencil_front_func_ref, front_write_mask = regs.stencil_front_mask,
             front_test_mask = regs.stencil_front_func_mask, back_ref = regs.stencil_back_func_ref,
             back_write_mask = regs.stencil_back_mask,
             back_test_mask = regs.stencil_back_func_mask](vk::CommandBuffer cmdbuf) {
                // Front face
                cmdbuf.SetStencilReference(VK_STENCIL_FACE_FRONT_BIT, front_ref);
                cmdbuf.SetStencilWriteMask(VK_STENCIL_FACE_FRONT_BIT, front_write_mask);
                cmdbuf.SetStencilCompareMask(VK_STENCIL_FACE_FRONT_BIT, front_test_mask);

                // Back face
                cmdbuf.SetStencilReference(VK_STENCIL_FACE_BACK_BIT, back_ref);
                cmdbuf.SetStencilWriteMask(VK_STENCIL_FACE_BACK_BIT, back_write_mask);
                cmdbuf.SetStencilCompareMask(VK_STENCIL_FACE_BACK_BIT, back_test_mask);
            });
    } else {
        // Front face defines both faces
        scheduler.Record([ref = regs.stencil_front_func_ref, write_mask = regs.stencil_front_mask,
                          test_mask = regs.stencil_front_func_mask](vk::CommandBuffer cmdbuf) {
            cmdbuf.SetStencilReference(VK_STENCIL_FACE_FRONT_AND_BACK, ref);
            cmdbuf.SetStencilWriteMask(VK_STENCIL_FACE_FRONT_AND_BACK, write_mask);
            cmdbuf.SetStencilCompareMask(VK_STENCIL_FACE_FRONT_AND_BACK, test_mask);
        });
    }
}

void RasterizerVulkan::UpdateLineWidth(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchLineWidth()) {
        return;
    }
    const float width = regs.line_smooth_enable ? regs.line_width_smooth : regs.line_width_aliased;
    scheduler.Record([width](vk::CommandBuffer cmdbuf) { cmdbuf.SetLineWidth(width); });
}

void RasterizerVulkan::UpdateCullMode(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchCullMode()) {
        return;
    }
    scheduler.Record(
        [enabled = regs.cull_test_enabled, cull_face = regs.cull_face](vk::CommandBuffer cmdbuf) {
            cmdbuf.SetCullModeEXT(enabled ? MaxwellToVK::CullFace(cull_face) : VK_CULL_MODE_NONE);
        });
}

void RasterizerVulkan::UpdateDepthBoundsTestEnable(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthBoundsTestEnable()) {
        return;
    }
    bool enabled = regs.depth_bounds_enable;
    if (enabled && !device.IsDepthBoundsSupported()) {
        LOG_WARNING(Render_Vulkan, "Depth bounds is enabled but not supported");
        enabled = false;
    }
    scheduler.Record([enable = regs.depth_bounds_enable](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthBoundsTestEnableEXT(enable);
    });
}

void RasterizerVulkan::UpdateDepthTestEnable(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthTestEnable()) {
        return;
    }
    scheduler.Record([enable = regs.depth_test_enable](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthTestEnableEXT(enable);
    });
}

void RasterizerVulkan::UpdateDepthWriteEnable(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthWriteEnable()) {
        return;
    }
    scheduler.Record([enable = regs.depth_write_enabled](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthWriteEnableEXT(enable);
    });
}

void RasterizerVulkan::UpdateDepthCompareOp(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthCompareOp()) {
        return;
    }
    scheduler.Record([func = regs.depth_test_func](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthCompareOpEXT(MaxwellToVK::ComparisonOp(func));
    });
}

void RasterizerVulkan::UpdateFrontFace(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchFrontFace()) {
        return;
    }

    VkFrontFace front_face = MaxwellToVK::FrontFace(regs.front_face);
    if (regs.screen_y_control.triangle_rast_flip != 0) {
        front_face = front_face == VK_FRONT_FACE_CLOCKWISE ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                                           : VK_FRONT_FACE_CLOCKWISE;
    }
    scheduler.Record(
        [front_face](vk::CommandBuffer cmdbuf) { cmdbuf.SetFrontFaceEXT(front_face); });
}

void RasterizerVulkan::UpdateStencilOp(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchStencilOp()) {
        return;
    }
    const Maxwell::StencilOp fail = regs.stencil_front_op_fail;
    const Maxwell::StencilOp zfail = regs.stencil_front_op_zfail;
    const Maxwell::StencilOp zpass = regs.stencil_front_op_zpass;
    const Maxwell::ComparisonOp compare = regs.stencil_front_func_func;
    if (regs.stencil_two_side_enable) {
        // Separate stencil op per face
        const Maxwell::StencilOp back_fail = regs.stencil_back_op_fail;
        const Maxwell::StencilOp back_zfail = regs.stencil_back_op_zfail;
        const Maxwell::StencilOp back_zpass = regs.stencil_back_op_zpass;
        const Maxwell::ComparisonOp back_compare = regs.stencil_back_func_func;
        scheduler.Record([fail, zfail, zpass, compare, back_fail, back_zfail, back_zpass,
                          back_compare](vk::CommandBuffer cmdbuf) {
            cmdbuf.SetStencilOpEXT(VK_STENCIL_FACE_FRONT_BIT, MaxwellToVK::StencilOp(fail),
                                   MaxwellToVK::StencilOp(zpass), MaxwellToVK::StencilOp(zfail),
                                   MaxwellToVK::ComparisonOp(compare));
            cmdbuf.SetStencilOpEXT(VK_STENCIL_FACE_BACK_BIT, MaxwellToVK::StencilOp(back_fail),
                                   MaxwellToVK::StencilOp(back_zpass),
                                   MaxwellToVK::StencilOp(back_zfail),
                                   MaxwellToVK::ComparisonOp(back_compare));
        });
    } else {
        // Front face defines the stencil op of both faces
        scheduler.Record([fail, zfail, zpass, compare](vk::CommandBuffer cmdbuf) {
            cmdbuf.SetStencilOpEXT(VK_STENCIL_FACE_FRONT_AND_BACK, MaxwellToVK::StencilOp(fail),
                                   MaxwellToVK::StencilOp(zpass), MaxwellToVK::StencilOp(zfail),
                                   MaxwellToVK::ComparisonOp(compare));
        });
    }
}

void RasterizerVulkan::UpdateStencilTestEnable(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchStencilTestEnable()) {
        return;
    }
    scheduler.Record([enable = regs.stencil_enable](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetStencilTestEnableEXT(enable);
    });
}

void RasterizerVulkan::UpdateVertexInput(Tegra::Engines::Maxwell3D::Regs& regs) {
    auto& dirty{maxwell3d->dirty.flags};
    if (!dirty[Dirty::VertexInput]) {
        return;
    }
    dirty[Dirty::VertexInput] = false;

    boost::container::static_vector<VkVertexInputBindingDescription2EXT, 32> bindings;
    boost::container::static_vector<VkVertexInputAttributeDescription2EXT, 32> attributes;

    // There seems to be a bug on Nvidia's driver where updating only higher attributes ends up
    // generating dirty state. Track the highest dirty attribute and update all attributes until
    // that one.
    size_t highest_dirty_attr{};
    for (size_t index = 0; index < Maxwell::NumVertexAttributes; ++index) {
        if (dirty[Dirty::VertexAttribute0 + index]) {
            highest_dirty_attr = index;
        }
    }
    for (size_t index = 0; index < highest_dirty_attr; ++index) {
        const Maxwell::VertexAttribute attribute{regs.vertex_attrib_format[index]};
        const u32 binding{attribute.buffer};
        dirty[Dirty::VertexAttribute0 + index] = false;
        dirty[Dirty::VertexBinding0 + static_cast<size_t>(binding)] = true;
        if (!attribute.constant) {
            attributes.push_back({
                .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                .pNext = nullptr,
                .location = static_cast<u32>(index),
                .binding = binding,
                .format = MaxwellToVK::VertexFormat(device, attribute.type, attribute.size),
                .offset = attribute.offset,
            });
        }
    }
    for (size_t index = 0; index < Maxwell::NumVertexAttributes; ++index) {
        if (!dirty[Dirty::VertexBinding0 + index]) {
            continue;
        }
        dirty[Dirty::VertexBinding0 + index] = false;

        const u32 binding{static_cast<u32>(index)};
        const auto& input_binding{regs.vertex_array[binding]};
        const bool is_instanced{regs.instanced_arrays.IsInstancingEnabled(binding)};
        bindings.push_back({
            .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
            .pNext = nullptr,
            .binding = binding,
            .stride = input_binding.stride,
            .inputRate = is_instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
            .divisor = is_instanced ? input_binding.divisor : 1,
        });
    }
    scheduler.Record([bindings, attributes](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetVertexInputEXT(bindings, attributes);
    });
}

void RasterizerVulkan::InitializeChannel(Tegra::Control::ChannelState& channel) {
    CreateChannel(channel);
    {
        std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
        texture_cache.CreateChannel(channel);
        buffer_cache.CreateChannel(channel);
    }
    pipeline_cache.CreateChannel(channel);
    query_cache.CreateChannel(channel);
    state_tracker.SetupTables(channel);
}

void RasterizerVulkan::BindChannel(Tegra::Control::ChannelState& channel) {
    const s32 channel_id = channel.bind_id;
    BindToChannel(channel_id);
    {
        std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
        texture_cache.BindToChannel(channel_id);
        buffer_cache.BindToChannel(channel_id);
    }
    pipeline_cache.BindToChannel(channel_id);
    query_cache.BindToChannel(channel_id);
    state_tracker.ChangeChannel(channel);
    state_tracker.InvalidateState();
}

void RasterizerVulkan::ReleaseChannel(s32 channel_id) {
    EraseChannel(channel_id);
    {
        std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
        texture_cache.EraseChannel(channel_id);
        buffer_cache.EraseChannel(channel_id);
    }
    pipeline_cache.EraseChannel(channel_id);
    query_cache.EraseChannel(channel_id);
}

} // namespace Vulkan
