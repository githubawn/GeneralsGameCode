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

// TheSuperHackers @feature DX9ExBackend owns a real IDirect3DDevice9Ex,
// created via Direct3DCreate9Ex/CreateDeviceEx -- forced, no fallback to
// plain Direct3DCreate9. Device lifecycle and the state/scene calls that
// don't depend on WW3D2's resource classes (VertexBufferClass/IndexBufferClass/
// TextureBaseClass/SurfaceClass, still hard-typed to D3D8 COM interfaces) are
// implemented against this device directly. The resource-bound methods
// (vertex/index buffers, textures, shaders, draw calls, render targets) are
// stubs for now -- see BACKEND_AGNOSTIC_RESOURCES_PLAN.md for what's needed
// before those can bind real D3D9 resources.

#pragma once

#include "WW3D2/IRenderBackend.h"
#include "WWMath/vector3.h"

struct IDirect3D9Ex;
struct IDirect3DDevice9Ex;

class DX9ExBackend : public IRenderBackend
{
public:
    DX9ExBackend();
    virtual ~DX9ExBackend() override;

    virtual void Initialize(void * window, int width, int height) override;
    virtual void Shutdown() override;

    // TheSuperHackers @feature Resource classes (DX9VertexBufferClass,
    // DX9IndexBufferClass, ...) need the live device to create/lock GPU
    // resources, but live outside Backend/ and must stay D3D9-header-free
    // at their own header level. This mirrors DX8Wrapper::_Get_D3D_Device8()'s
    // role for the DX8 resource classes.
    static IDirect3DDevice9Ex * Get_Device() { return s_currentDevice; }

    virtual bool Is_Device_Lost() const override;
    virtual bool Has_Stencil() override;
    virtual WW3DFormat Get_Back_Buffer_Format() override;
    virtual SurfaceClass * Get_Back_Buffer(unsigned int num) override;
    virtual void Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit) override;

    virtual void Begin_Scene() override;
    virtual void End_Scene(bool flip_frame) override;
    virtual void Flip_To_Primary() override;
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil) override;
    virtual void Set_Viewport(const RenderBackendViewport & viewport) override;

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream) override;
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) override;
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) override;
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) override;
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset) override;

    virtual void Set_Shader(const ShaderClass & shader) override;
    virtual void Get_Shader(ShaderClass & shader) override;
    virtual void Set_Material(const VertexMaterialClass * material) override;
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture) override;
    virtual void Apply_Render_State_Changes() override;
    virtual void Apply_Default_State() override;
    virtual void Invalidate_Cached_Render_States() override;

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m) override;
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m) override;
    virtual void Get_Transform(TransformKind transform, Matrix4x4 & m) override;
    virtual void Set_World_Identity() override;
    virtual void Set_View_Identity() override;
    virtual bool Is_World_Identity() override;
    virtual bool Is_View_Identity() override;
    virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar) override;

    virtual void Set_Light(unsigned int index, const LightClass & light) override;
    virtual void Set_Ambient(const Vector3 & color) override;
    virtual const Vector3 & Get_Ambient() const override;
    virtual void Set_Fog(bool enable, const Vector3 & color, float start, float end) override;
    virtual bool Get_Fog_Enable() const override;
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env) override;
    virtual LightEnvironmentClass * Get_Light_Environment() const override;

    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) override;
    virtual void Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) override;
    virtual void Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count) override;

    // The shader id is treated as an opaque unsigned long.
    virtual void Set_Vertex_Shader(unsigned long vertex_shader) override;
    virtual void Set_Pixel_Shader(unsigned long pixel_shader) override;
    virtual void Set_Vertex_Shader_Constant(int reg, const void * data, int count) override;
    virtual void Set_Pixel_Shader_Constant(int reg, const void * data, int count) override;

    virtual TextureClass * Create_Render_Target(int width, int height, WW3DFormat format) override;
    virtual void Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture) override;
    virtual bool Is_Render_To_Texture() override;
    virtual void Set_Shadow_Map(int idx, ZTextureClass * ztex) override;
    virtual ZTextureClass * Get_Shadow_Map(int idx) override;

private:
    void Release_Device();

    static IDirect3DDevice9Ex * s_currentDevice;

    IDirect3D9Ex *        m_d3d9;
    IDirect3DDevice9Ex *  m_device;
    bool                  m_isWindowed;
    bool                  m_hasStencil;
    bool                  m_deviceLost;

    // Cached scene state -- DX9ExBackend owns its own copy rather than
    // reaching into DX8Wrapper's private statics (see Phase 0 in
    // BACKEND_AGNOSTIC_RESOURCES_PLAN.md).
    bool      m_worldIsIdentity;
    bool      m_viewIsIdentity;
    Vector3   m_ambientColor;
    bool      m_fogEnable;
    LightEnvironmentClass * m_lightEnvironment;
};
