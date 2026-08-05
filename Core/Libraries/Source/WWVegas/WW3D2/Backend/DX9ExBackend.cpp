/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// This file must never see D3D8 headers: DX9Ex and DX8 declare colliding
// type names (D3DLIGHTTYPE, D3DPRESENT_PARAMETERS, ...), so it opts out of
// the shared PCH (see CMakeLists.txt SKIP_PRECOMPILE_HEADERS) instead of
// coexisting with dx8wrapper.h/d3d8.h in the same translation unit.
#include "Utility/CppMacros.h" // Must be first (see the PCH's own ordering)
#include "DX9ExBackend.h"

#include <d3d9.h>

#include "WW3D2/light.h"
#include "WW3D2/lightenvironment.h"
#include "WW3D2/surfaceclass.h"
#include "WWMath/matrix4.h"
#include "WWMath/matrix3d.h"
#include "WWDebug/wwdebug.h"

namespace
{
    // D3DTRANSFORMSTATETYPE values are identical between the D3D8 and D3D9
    // headers -- RB_TRANSFORM_* already match them directly (see
    // IRenderBackend.h), so no separate DX9 mapping table is needed.
    D3DTRANSFORMSTATETYPE To_D3D_Transform(TransformKind transform)
    {
        switch (transform)
        {
        case RB_TRANSFORM_VIEW:       return D3DTS_VIEW;
        case RB_TRANSFORM_PROJECTION: return D3DTS_PROJECTION;
        case RB_TRANSFORM_WORLD:
        default:                      return D3DTS_WORLD;
        }
    }
}

IDirect3DDevice9Ex * DX9ExBackend::s_currentDevice = nullptr;

DX9ExBackend::DX9ExBackend()
    : m_d3d9(nullptr)
    , m_device(nullptr)
    , m_isWindowed(true)
    , m_hasStencil(false)
    , m_deviceLost(false)
    , m_worldIsIdentity(true)
    , m_viewIsIdentity(true)
    , m_ambientColor(0.0f, 0.0f, 0.0f)
    , m_fogEnable(false)
    , m_lightEnvironment(nullptr)
{
}

DX9ExBackend::~DX9ExBackend()
{
    Release_Device();
}

void DX9ExBackend::Release_Device()
{
    if (m_device != nullptr)
    {
        if (s_currentDevice == m_device)
        {
            s_currentDevice = nullptr;
        }
        m_device->Release();
        m_device = nullptr;
    }
    if (m_d3d9 != nullptr)
    {
        m_d3d9->Release();
        m_d3d9 = nullptr;
    }
}

// TheSuperHackers @feature DX9ExBackend forces Direct3DCreate9Ex -- there is
// intentionally no fallback to plain Direct3DCreate9. Systems without the
// D3D9Ex/WDDM update (Windows XP without the optional update, effectively)
// cannot run this backend; they should select DX8 via -dx8/GraphicsBackend
// instead. This mirrors DX8Backend/DX8Wrapper's device management shape but
// is entirely self-contained -- it does not touch DX8Wrapper's device or
// state at all.
void DX9ExBackend::Initialize(void * window, int width, int height)
{
    if (m_device != nullptr)
    {
        return;
    }

    HWND hwnd = static_cast<HWND>(window);

    typedef HRESULT (WINAPI *Direct3DCreate9ExType)(UINT SDKVersion, IDirect3D9Ex **ppD3D);
    HMODULE d3d9Lib = ::LoadLibraryA("d3d9.dll");
    if (d3d9Lib == nullptr)
    {
        WWDEBUG_SAY(("DX9ExBackend: failed to load d3d9.dll"));
        return;
    }

    Direct3DCreate9ExType Direct3DCreate9ExPtr =
        (Direct3DCreate9ExType)::GetProcAddress(d3d9Lib, "Direct3DCreate9Ex");
    if (Direct3DCreate9ExPtr == nullptr)
    {
        WWDEBUG_SAY(("DX9ExBackend: Direct3DCreate9Ex not found -- OS lacks D3D9Ex support"));
        return;
    }

    if (FAILED(Direct3DCreate9ExPtr(D3D_SDK_VERSION, &m_d3d9)) || m_d3d9 == nullptr)
    {
        WWDEBUG_SAY(("DX9ExBackend: Direct3DCreate9Ex failed"));
        return;
    }

    D3DDISPLAYMODEEX desktopMode;
    ::ZeroMemory(&desktopMode, sizeof(desktopMode));
    desktopMode.Size = sizeof(D3DDISPLAYMODEEX);
    m_d3d9->GetAdapterDisplayModeEx(D3DADAPTER_DEFAULT, &desktopMode, nullptr);

    m_isWindowed = true;

    D3DPRESENT_PARAMETERS params;
    ::ZeroMemory(&params, sizeof(params));
    params.BackBufferWidth            = static_cast<UINT>(width);
    params.BackBufferHeight           = static_cast<UINT>(height);
    params.BackBufferFormat           = desktopMode.Format;
    params.BackBufferCount            = 1;
    params.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow               = hwnd;
    params.Windowed                   = TRUE;
    params.EnableAutoDepthStencil     = TRUE;
    params.AutoDepthStencilFormat     = D3DFMT_D24S8;
    params.Flags                      = 0;
    params.FullScreen_RefreshRateInHz = 0;
    params.PresentationInterval       = D3DPRESENT_INTERVAL_DEFAULT;

    // D24S8 is nearly universal on hardware that supports D3D9Ex at all;
    // fall back to a depth-only format rather than failing device creation
    // if the driver genuinely can't do it.
    if (FAILED(m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            desktopMode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8)))
    {
        params.AutoDepthStencilFormat = D3DFMT_D16;
        m_hasStencil = false;
    }
    else
    {
        m_hasStencil = true;
    }

    HRESULT hr = m_d3d9->CreateDeviceEx(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &params,
        nullptr,
        &m_device);

    if (FAILED(hr) || m_device == nullptr)
    {
        WWDEBUG_SAY(("DX9ExBackend: CreateDeviceEx failed (hr=0x%08lX)", static_cast<unsigned long>(hr)));
        if (m_d3d9 != nullptr)
        {
            m_d3d9->Release();
            m_d3d9 = nullptr;
        }
        return;
    }

    s_currentDevice = m_device;

    WWDEBUG_SAY(("DX9ExBackend: IDirect3DDevice9Ex created (%dx%d, windowed)", width, height));
}

void DX9ExBackend::Shutdown()
{
    Release_Device();
}

bool DX9ExBackend::Is_Device_Lost() const
{
    return m_deviceLost;
}

bool DX9ExBackend::Has_Stencil()
{
    return m_hasStencil;
}

WW3DFormat DX9ExBackend::Get_Back_Buffer_Format()
{
    // TODO(dx9ex-resources): WW3DFormat conversion needs a D3DFORMAT->WW3DFormat
    // table shared with the DX8 path (see formconv.h) once texture/surface
    // resources are wired up. Unused until Set_Texture/Create_Render_Target
    // are implemented.
    return WW3D_FORMAT_UNKNOWN;
}

SurfaceClass * DX9ExBackend::Get_Back_Buffer(unsigned int num)
{
    // TODO(dx9ex-resources): SurfaceClass is hard-typed to IDirect3DSurface8
    // today (see BACKEND_AGNOSTIC_RESOURCES_PLAN.md, Phase 2). Cannot wrap
    // an IDirect3DSurface9 in it yet.
    return nullptr;
}

void DX9ExBackend::Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit)
{
    if (m_device == nullptr)
    {
        return;
    }
    D3DGAMMARAMP ramp;
    for (int i = 0; i < 256; ++i)
    {
        float val = static_cast<float>(i) / 255.0f;
        val = powf(val, 1.0f / gamma) * contrast + bright;
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;
        WORD component = static_cast<WORD>(val * 65535.0f);
        ramp.red[i] = ramp.green[i] = ramp.blue[i] = component;
    }
    m_device->SetGammaRamp(0, calibrate ? D3DSGR_CALIBRATE : D3DSGR_NO_CALIBRATION, &ramp);
}

void DX9ExBackend::Begin_Scene()
{
    if (m_device != nullptr)
    {
        m_device->BeginScene();
    }
}

void DX9ExBackend::End_Scene(bool flip_frame)
{
    if (m_device == nullptr)
    {
        return;
    }
    m_device->EndScene();

    if (flip_frame)
    {
        HRESULT hr = m_device->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
        m_deviceLost = (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICEHUNG);
    }
}

void DX9ExBackend::Flip_To_Primary()
{
    // Windowed-only for now (see Initialize) -- no fullscreen flip-chain
    // management needed.
}

void DX9ExBackend::Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil)
{
    if (m_device == nullptr)
    {
        return;
    }

    DWORD flags = 0;
    if (clear_color)    flags |= D3DCLEAR_TARGET;
    if (clear_z_stencil)
    {
        flags |= D3DCLEAR_ZBUFFER;
        if (m_hasStencil) flags |= D3DCLEAR_STENCIL;
    }
    if (flags == 0)
    {
        return;
    }

    D3DCOLOR d3dColor = D3DCOLOR_COLORVALUE(color.X, color.Y, color.Z, dest_alpha);
    m_device->Clear(0, nullptr, flags, d3dColor, z, stencil);
}

void DX9ExBackend::Set_Viewport(const RenderBackendViewport & viewport)
{
    if (m_device == nullptr)
    {
        return;
    }
    D3DVIEWPORT9 vp;
    vp.X      = viewport.x;
    vp.Y      = viewport.y;
    vp.Width  = viewport.width;
    vp.Height = viewport.height;
    vp.MinZ   = viewport.min_z;
    vp.MaxZ   = viewport.max_z;
    m_device->SetViewport(&vp);
}

// -- Resource-bound methods ---------------------------------------------
// Vertex/index buffers, textures, shaders, and draw calls all require D3D9
// resources (IDirect3DVertexBuffer9/IDirect3DTexture9/etc.), but
// VertexBufferClass/IndexBufferClass/TextureBaseClass/SurfaceClass are still
// hard-typed to D3D8 COM interfaces. Left as stubs, matching BgfxBackend's
// precedent, until BACKEND_AGNOSTIC_RESOURCES_PLAN.md's phases land.

void DX9ExBackend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream)
{
}

void DX9ExBackend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
}

void DX9ExBackend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
}

void DX9ExBackend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
}

void DX9ExBackend::Set_Index_Buffer_Index_Offset(unsigned int offset)
{
}

void DX9ExBackend::Set_Shader(const ShaderClass & shader)
{
}

void DX9ExBackend::Get_Shader(ShaderClass & shader)
{
}

void DX9ExBackend::Set_Material(const VertexMaterialClass * material)
{
}

void DX9ExBackend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
}

void DX9ExBackend::Apply_Render_State_Changes()
{
}

void DX9ExBackend::Apply_Default_State()
{
    if (m_device == nullptr)
    {
        return;
    }
    m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
}

void DX9ExBackend::Invalidate_Cached_Render_States()
{
}

void DX9ExBackend::Set_Transform(TransformKind transform, const Matrix4x4 & m)
{
    if (m_device == nullptr)
    {
        return;
    }
    if (transform == RB_TRANSFORM_WORLD)  m_worldIsIdentity = false;
    if (transform == RB_TRANSFORM_VIEW)   m_viewIsIdentity  = false;
    D3DMATRIX dxm = To_D3DMATRIX(m);
    m_device->SetTransform(To_D3D_Transform(transform), &dxm);
}

void DX9ExBackend::Set_Transform(TransformKind transform, const Matrix3D & m)
{
    if (m_device == nullptr)
    {
        return;
    }
    if (transform == RB_TRANSFORM_WORLD)  m_worldIsIdentity = false;
    if (transform == RB_TRANSFORM_VIEW)   m_viewIsIdentity  = false;
    D3DMATRIX dxm = To_D3DMATRIX(m);
    m_device->SetTransform(To_D3D_Transform(transform), &dxm);
}

void DX9ExBackend::Get_Transform(TransformKind transform, Matrix4x4 & m)
{
    if (m_device == nullptr)
    {
        m.Make_Identity();
        return;
    }
    D3DMATRIX dxm;
    m_device->GetTransform(To_D3D_Transform(transform), &dxm);
    m = To_Matrix4x4(dxm);
}

void DX9ExBackend::Set_World_Identity()
{
    Matrix4x4 identity(true);
    Set_Transform(RB_TRANSFORM_WORLD, identity);
    m_worldIsIdentity = true;
}

void DX9ExBackend::Set_View_Identity()
{
    Matrix4x4 identity(true);
    Set_Transform(RB_TRANSFORM_VIEW, identity);
    m_viewIsIdentity = true;
}

bool DX9ExBackend::Is_World_Identity()
{
    return m_worldIsIdentity;
}

bool DX9ExBackend::Is_View_Identity()
{
    return m_viewIsIdentity;
}

void DX9ExBackend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar)
{
    // DX8Wrapper folds a Z-bias-derived depth-range tweak into the
    // projection matrix here (see dx8wrapper.h); DX9ExBackend does not draw
    // anything real yet, so just set the matrix directly.
    Set_Transform(RB_TRANSFORM_PROJECTION, matrix);
}

void DX9ExBackend::Set_Light(unsigned int index, const LightClass & light)
{
    if (m_device == nullptr)
    {
        return;
    }

    D3DLIGHT9 dlight;
    ::ZeroMemory(&dlight, sizeof(dlight));

    switch (light.Get_Type())
    {
    case LightClass::POINT:       dlight.Type = D3DLIGHT_POINT;       break;
    case LightClass::DIRECTIONAL: dlight.Type = D3DLIGHT_DIRECTIONAL; break;
    case LightClass::SPOT:        dlight.Type = D3DLIGHT_SPOT;        break;
    default:                      dlight.Type = D3DLIGHT_DIRECTIONAL; break;
    }

    Vector3 temp;
    light.Get_Diffuse(&temp);
    temp *= light.Get_Intensity();
    dlight.Diffuse.r = temp.X; dlight.Diffuse.g = temp.Y; dlight.Diffuse.b = temp.Z; dlight.Diffuse.a = 1.0f;

    light.Get_Specular(&temp);
    temp *= light.Get_Intensity();
    dlight.Specular.r = temp.X; dlight.Specular.g = temp.Y; dlight.Specular.b = temp.Z; dlight.Specular.a = 1.0f;

    light.Get_Ambient(&temp);
    temp *= light.Get_Intensity();
    dlight.Ambient.r = temp.X; dlight.Ambient.g = temp.Y; dlight.Ambient.b = temp.Z; dlight.Ambient.a = 1.0f;

    temp = light.Get_Position();
    dlight.Position = *(D3DVECTOR*)&temp;

    light.Get_Spot_Direction(temp);
    dlight.Direction = *(D3DVECTOR*)&temp;

    dlight.Range   = light.Get_Attenuation_Range();
    dlight.Falloff = light.Get_Spot_Exponent();
    dlight.Theta   = light.Get_Spot_Angle();
    dlight.Phi     = light.Get_Spot_Angle();

    double a, b;
    light.Get_Far_Attenuation_Range(a, b);
    dlight.Attenuation0 = 1.0f;
    dlight.Attenuation1 = (fabs(a - b) < 1e-5) ? 0.0f : static_cast<float>(1.0 / a);
    dlight.Attenuation2 = 0.0f;

    m_device->SetLight(index, &dlight);
    m_device->LightEnable(index, TRUE);
}

void DX9ExBackend::Set_Ambient(const Vector3 & color)
{
    m_ambientColor = color;
    if (m_device != nullptr)
    {
        D3DCOLOR d3dColor = D3DCOLOR_COLORVALUE(color.X, color.Y, color.Z, 1.0f);
        m_device->SetRenderState(D3DRS_AMBIENT, d3dColor);
    }
}

const Vector3 & DX9ExBackend::Get_Ambient() const
{
    return m_ambientColor;
}

void DX9ExBackend::Set_Fog(bool enable, const Vector3 & color, float start, float end)
{
    m_fogEnable = enable;
    if (m_device == nullptr)
    {
        return;
    }
    m_device->SetRenderState(D3DRS_FOGENABLE, enable ? TRUE : FALSE);
    if (enable)
    {
        D3DCOLOR d3dColor = D3DCOLOR_COLORVALUE(color.X, color.Y, color.Z, 1.0f);
        m_device->SetRenderState(D3DRS_FOGCOLOR, d3dColor);
        m_device->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
        m_device->SetRenderState(D3DRS_FOGSTART, *reinterpret_cast<DWORD*>(&start));
        m_device->SetRenderState(D3DRS_FOGEND, *reinterpret_cast<DWORD*>(&end));
    }
}

bool DX9ExBackend::Get_Fog_Enable() const
{
    return m_fogEnable;
}

void DX9ExBackend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    m_lightEnvironment = light_env;
}

LightEnvironmentClass * DX9ExBackend::Get_Light_Environment() const
{
    return m_lightEnvironment;
}

void DX9ExBackend::Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
}

void DX9ExBackend::Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
}

void DX9ExBackend::Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count)
{
}

void DX9ExBackend::Set_Vertex_Shader(unsigned long vertex_shader)
{
}

void DX9ExBackend::Set_Pixel_Shader(unsigned long pixel_shader)
{
}

void DX9ExBackend::Set_Vertex_Shader_Constant(int reg, const void * data, int count)
{
    if (m_device != nullptr)
    {
        m_device->SetVertexShaderConstantF(reg, static_cast<const float*>(data), count);
    }
}

void DX9ExBackend::Set_Pixel_Shader_Constant(int reg, const void * data, int count)
{
    if (m_device != nullptr)
    {
        m_device->SetPixelShaderConstantF(reg, static_cast<const float*>(data), count);
    }
}

TextureClass * DX9ExBackend::Create_Render_Target(int width, int height, WW3DFormat format)
{
    return nullptr;
}

void DX9ExBackend::Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture)
{
}

bool DX9ExBackend::Is_Render_To_Texture()
{
    return false;
}

void DX9ExBackend::Set_Shadow_Map(int idx, ZTextureClass * ztex)
{
}

ZTextureClass * DX9ExBackend::Get_Shadow_Map(int idx)
{
    return nullptr;
}
