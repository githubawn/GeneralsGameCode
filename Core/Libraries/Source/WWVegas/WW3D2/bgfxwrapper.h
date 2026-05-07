#pragma once

#include "always.h"
#include "wwstring.h"
#include "vector3.h"

// TheSuperHackers @build JohnsterID 13/05/2026 Add BGFX wrapper
class BGFXWrapper
{
public:
    static bool Init(void* hwnd, bool lite = false);
    static void Shutdown();
    static void Begin_Scene();
    static void End_Scene(bool flip_frame = true);
    static void Clear(bool clear_color, bool clear_z_stencil, const Vector3& color, float dest_alpha = 0.0f, float z = 1.0f, unsigned int stencil = 0);

    static bool Is_Initted() { return IsInitted; }

private:
    static bool IsInitted;
    static void* Hwnd;
};