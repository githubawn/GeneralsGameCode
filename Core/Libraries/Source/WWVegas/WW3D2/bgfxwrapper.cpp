#include "bgfxwrapper.h"
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

bool BGFXWrapper::IsInitted = false;
void* BGFXWrapper::Hwnd = nullptr;

// TheSuperHackers @build JohnsterID 13/05/2026 Implement BGFX wrapper
bool BGFXWrapper::Init(void* hwnd, bool lite)
{
    if (IsInitted) return true;
    Hwnd = hwnd;

    if (lite) {
        IsInitted = true;
        return true;
    }

    bgfx::Init init;
    init.type = bgfx::RendererType::Count; // Auto detect
    init.vendorId = BGFX_PCI_ID_NONE;
    init.platformData.nwh = hwnd;
    init.resolution.width = 800; // Placeholder, should be updated by Set_Render_Device
    init.resolution.height = 600;
    init.resolution.reset = BGFX_RESET_VSYNC;

    if (!bgfx::init(init)) {
        return false;
    }

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, uint16_t(800), uint16_t(600));

    IsInitted = true;
    return true;
}

void BGFXWrapper::Shutdown()
{
    if (IsInitted) {
        bgfx::shutdown();
        IsInitted = false;
    }
}

void BGFXWrapper::Begin_Scene()
{
    bgfx::touch(0);
}

void BGFXWrapper::End_Scene(bool flip_frame)
{
    bgfx::frame();
}

void BGFXWrapper::Clear(bool clear_color, bool clear_z_stencil, const Vector3& color, float dest_alpha, float z, unsigned int stencil)
{
    uint16_t flags = 0;
    if (clear_color) flags |= BGFX_CLEAR_COLOR;
    if (clear_z_stencil) flags |= BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL;

    uint32_t r = (uint32_t)(color.X * 255.0f);
    uint32_t g = (uint32_t)(color.Y * 255.0f);
    uint32_t b = (uint32_t)(color.Z * 255.0f);
    uint32_t a = (uint32_t)(dest_alpha * 255.0f);
    uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | a;

    bgfx::setViewClear(0, flags, rgba, z, (uint8_t)stencil);
}