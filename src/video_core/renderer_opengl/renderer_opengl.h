// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/math_util.h"

#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"

namespace Core {
class System;
class TelemetrySession;
} // namespace Core

namespace Core::Frontend {
class EmuWindow;
}

namespace Tegra {
class GPU;
}

namespace OpenGL {

class BlitScreen;

class RendererOpenGL final : public VideoCore::RendererBase {
public:
    explicit RendererOpenGL(Core::TelemetrySession& telemetry_session_,
                            Core::Frontend::EmuWindow& emu_window_,
                            Tegra::MaxwellDeviceMemoryManager& device_memory_, Tegra::GPU& gpu_,
                            std::unique_ptr<Core::Frontend::GraphicsContext> context_);
    ~RendererOpenGL() override;

    void Composite(std::span<const Tegra::FramebufferConfig> framebuffers) override;

    VideoCore::RasterizerInterface* ReadRasterizer() override {
        return &rasterizer;
    }

    [[nodiscard]] std::string GetDeviceVendor() const override {
        return device.GetVendorName();
    }

private:
    void AddTelemetryFields();
    void RenderScreenshot(std::span<const Tegra::FramebufferConfig> framebuffers);

    Core::TelemetrySession& telemetry_session;
    Core::Frontend::EmuWindow& emu_window;
    Tegra::MaxwellDeviceMemoryManager& device_memory;
    Tegra::GPU& gpu;

    Device device;
    StateTracker state_tracker;
    ProgramManager program_manager;
    RasterizerOpenGL rasterizer;
    OGLFramebuffer screenshot_framebuffer;

    std::unique_ptr<BlitScreen> blit_screen;
};

} // namespace OpenGL
