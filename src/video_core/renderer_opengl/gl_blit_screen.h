// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <memory>
#include <span>

#include "core/hle/service/nvnflinger/pixel_format.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Layout {
struct FramebufferLayout;
}

namespace Tegra {
struct FramebufferConfig;
}

namespace Settings {
enum class ScalingFilter : u32;
}

namespace OpenGL {

class Device;
class Layer;
class ProgramManager;
class RasterizerOpenGL;
class StateTracker;
class WindowAdaptPass;

/// Structure used for storing information about the display target for the Switch screen
struct FramebufferTextureInfo {
    GLuint display_texture{};
    u32 width;
    u32 height;
    u32 scaled_width;
    u32 scaled_height;
};

class BlitScreen {
public:
    explicit BlitScreen(RasterizerOpenGL& rasterizer,
                        Tegra::MaxwellDeviceMemoryManager& device_memory,
                        StateTracker& state_tracker, ProgramManager& program_manager,
                        Device& device);
    ~BlitScreen();

    /// Draws the emulated screens to the emulator window.
    void DrawScreen(std::span<const Tegra::FramebufferConfig> framebuffers,
                    const Layout::FramebufferLayout& layout);

private:
    void CreateWindowAdapt();

    RasterizerOpenGL& rasterizer;
    Tegra::MaxwellDeviceMemoryManager& device_memory;
    StateTracker& state_tracker;
    ProgramManager& program_manager;
    Device& device;

    Settings::ScalingFilter current_window_adapt{};
    std::unique_ptr<WindowAdaptPass> window_adapt;

    std::list<Layer> layers;
};

} // namespace OpenGL
