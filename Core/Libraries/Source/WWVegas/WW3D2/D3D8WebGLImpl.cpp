/*
**	Command & Conquer Generals Zero Hour(tm)
**	D3D8 to WebGL Shim Implementation
*/

#include "WebShims/d3d8.h"
#include <iostream>
#include <vector>

class D3D8ResourceWebGL : public IDirect3DResource8 {
public:
    virtual HRESULT Release() override { delete this; return D3D_OK; }
    virtual HRESULT GetDevice(IDirect3DDevice8** ppDevice) override { return D3D_OK; }
};

class D3D8TextureWebGL : public IDirect3DTexture8 {
public:
    GLuint glTexture;
    D3D8TextureWebGL() { glGenTextures(1, &glTexture); }
    virtual ~D3D8TextureWebGL() { glDeleteTextures(1, &glTexture); }
    virtual HRESULT Release() override { delete this; return D3D_OK; }
    virtual HRESULT GetDevice(IDirect3DDevice8** ppDevice) override { return D3D_OK; }
    virtual void GenerateMipSubLevels() override {}
    virtual HRESULT LockRect(UINT Level, void* pLockedRect, const void* pRect, DWORD Flags) override { return D3D_OK; }
    virtual HRESULT UnlockRect(UINT Level) override { return D3D_OK; }
};

class D3D8VertexBufferWebGL : public IDirect3DVertexBuffer8 {
public:
    GLuint glBuffer;
    std::vector<uint8_t> data;
    D3D8VertexBufferWebGL(UINT size) : data(size) { glGenBuffers(1, &glBuffer); }
    virtual ~D3D8VertexBufferWebGL() { glDeleteBuffers(1, &glBuffer); }
    virtual HRESULT Release() override { delete this; return D3D_OK; }
    virtual HRESULT GetDevice(IDirect3DDevice8** ppDevice) override { return D3D_OK; }
    virtual HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags) override {
        *ppbData = data.data() + OffsetToLock;
        return D3D_OK;
    }
    virtual HRESULT Unlock() override {
        glBindBuffer(GL_ARRAY_BUFFER, glBuffer);
        glBufferData(GL_ARRAY_BUFFER, data.size(), data.data(), GL_STATIC_DRAW);
        return D3D_OK;
    }
};

class D3D8IndexBufferWebGL : public IDirect3DIndexBuffer8 {
public:
    GLuint glBuffer;
    std::vector<uint8_t> data;
    D3D8IndexBufferWebGL(UINT size) : data(size) { glGenBuffers(1, &glBuffer); }
    virtual ~D3D8IndexBufferWebGL() { glDeleteBuffers(1, &glBuffer); }
    virtual HRESULT Release() override { delete this; return D3D_OK; }
    virtual HRESULT GetDevice(IDirect3DDevice8** ppDevice) override { return D3D_OK; }
    virtual HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags) override {
        *ppbData = data.data() + OffsetToLock;
        return D3D_OK;
    }
    virtual HRESULT Unlock() override {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.size(), data.data(), GL_STATIC_DRAW);
        return D3D_OK;
    }
};

// GLSL FFP Emulator
const char* VS_SOURCE = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    vTexCoord = aTexCoord;
    vColor = aColor;
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
})";

const char* FS_SOURCE = R"(#version 300 es
precision mediump float;
in vec2 vTexCoord;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(uTexture, vTexCoord) * vColor;
})";

class D3D8DeviceWebGL : public IDirect3DDevice8 {
    GLuint m_program;
    GLint m_uModel, m_uView, m_uProj, m_uTexture;
    DWORD m_currentFVF;

    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        return shader;
    }

public:
    D3D8DeviceWebGL() : m_currentFVF(0) {
        GLuint vs = compileShader(GL_VERTEX_SHADER, VS_SOURCE);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, FS_SOURCE);
        m_program = glCreateProgram();
        glAttachShader(m_program, vs);
        glAttachShader(m_program, fs);
        glLinkProgram(m_program);
        glUseProgram(m_program);
        m_uModel = glGetUniformLocation(m_program, "uModel");
        m_uView = glGetUniformLocation(m_program, "uView");
        m_uProj = glGetUniformLocation(m_program, "uProj");
        m_uTexture = glGetUniformLocation(m_program, "uTexture");
    }

    virtual HRESULT Release() override {
        delete this;
        return D3D_OK;
    }

    virtual HRESULT BeginScene() override {
        // No-op for now
        return D3D_OK;
    }

    virtual HRESULT EndScene() override {
        // No-op for now
        return D3D_OK;
    }

    virtual HRESULT Clear(DWORD Count, const void* pRects, DWORD Flags, DWORD Color, float Z, DWORD Stencil) override {
        GLbitfield gl_flags = 0;
        if (Flags & 1) gl_flags |= GL_COLOR_BUFFER_BIT; // D3DCLEAR_TARGET
        if (Flags & 2) gl_flags |= GL_DEPTH_BUFFER_BIT; // D3DCLEAR_ZBUFFER
        if (Flags & 4) gl_flags |= GL_STENCIL_BUFFER_BIT; // D3DCLEAR_STENCIL

        float r = ((Color >> 16) & 0xFF) / 255.0f;
        float g = ((Color >> 8) & 0xFF) / 255.0f;
        float b = (Color & 0xFF) / 255.0f;
        float a = ((Color >> 24) & 0xFF) / 255.0f;

        glClearColor(r, g, b, a);
        glClearDepthf(Z);
        glClearStencil(Stencil);
        glClear(gl_flags);

        return D3D_OK;
    }

    virtual HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer) override {
        std::cout << "Creating WebGL Vertex Buffer..." << std::endl;
        *ppVertexBuffer = new D3D8VertexBufferWebGL(Length);
        return D3D_OK;
    }

    virtual HRESULT CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer) override {
        std::cout << "Creating WebGL Index Buffer..." << std::endl;
        *ppIndexBuffer = new D3D8IndexBufferWebGL(Length);
        return D3D_OK;
    }

    virtual HRESULT CreateVertexShader(const DWORD* pDeclaration, const DWORD* pFunction, DWORD* pHandle, DWORD Usage) override {
        std::cout << "Creating WebGL Vertex Shader..." << std::endl;
        *pHandle = 1; // Dummy handle
        return D3D_OK;
    }

    virtual HRESULT SetVertexShader(DWORD Handle) override {
        m_currentFVF = Handle;
        return D3D_OK;
    }

    virtual HRESULT CreatePixelShader(const DWORD* pFunction, DWORD* pHandle) override {
        std::cout << "Creating WebGL Pixel Shader..." << std::endl;
        *pHandle = 1; // Dummy handle
        return D3D_OK;
    }

    virtual HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override {
        switch (State) {
            case D3DRS_ZENABLE:
                if (Value) glEnable(GL_DEPTH_TEST);
                else glDisable(GL_DEPTH_TEST);
                break;
            case D3DRS_ZWRITEENABLE:
                glDepthMask(Value ? GL_TRUE : GL_FALSE);
                break;
            case D3DRS_ALPHABLENDENABLE:
                if (Value) glEnable(GL_BLEND);
                else glDisable(GL_BLEND);
                break;
            case D3DRS_SRCBLEND:
                // Map D3DBLEND to GL_BLEND_FACTOR
                break;
            case D3DRS_CULLMODE:
                if (Value == 1) glDisable(GL_CULL_FACE); // D3DCULL_NONE
                else {
                    glEnable(GL_CULL_FACE);
                    glCullFace(Value == 3 ? GL_BACK : GL_FRONT); // D3DCULL_CCW / CW
                }
                break;
        }
        return D3D_OK;
    }

    UINT m_currentStride;
    virtual HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8* pStreamData, UINT Stride) override {
        if (pStreamData) {
            glBindBuffer(GL_ARRAY_BUFFER, ((D3D8VertexBufferWebGL*)pStreamData)->glBuffer);
            m_currentStride = Stride;
        }
        return D3D_OK;
    }

    virtual HRESULT SetIndices(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex) override {
        if (pIndexData) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ((D3D8IndexBufferWebGL*)pIndexData)->glBuffer);
        }
        return D3D_OK;
    }

    virtual HRESULT DrawIndexedPrimitive(int Type, UINT MinIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount) override {
        GLenum mode = GL_TRIANGLES;
        if (Type == 4) mode = GL_TRIANGLES; // D3DPT_TRIANGLELIST
        else if (Type == 5) mode = GL_TRIANGLE_STRIP;
        
        // FVF attribute setup
        UINT offset = 0;
        if (m_currentFVF & D3DFVF_XYZ) {
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, m_currentStride, (void*)(uintptr_t)offset);
            offset += 12;
        } else if (m_currentFVF & D3DFVF_XYZRHW) {
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, m_currentStride, (void*)(uintptr_t)offset);
            offset += 16;
        }

        if (m_currentFVF & D3DFVF_NORMAL) {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, m_currentStride, (void*)(uintptr_t)offset);
            offset += 12;
        }

        if (m_currentFVF & D3DFVF_DIFFUSE) {
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, m_currentStride, (void*)(uintptr_t)offset);
            offset += 4;
        }

        if (m_currentFVF & D3DFVF_TEX1) {
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, m_currentStride, (void*)(uintptr_t)offset);
            offset += 8;
        }

        UINT indexCount = 0;
        if (mode == GL_TRIANGLES) indexCount = PrimitiveCount * 3;
        else if (mode == GL_TRIANGLE_STRIP) indexCount = PrimitiveCount + 2;

        glDrawElements(mode, indexCount, GL_UNSIGNED_SHORT, (void*)(uintptr_t)(StartIndex * 2));
        return D3D_OK;
    }

    virtual HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture8* pTexture) override {
        if (pTexture) {
            glActiveTexture(GL_TEXTURE0 + Stage);
            glBindTexture(GL_TEXTURE_2D, ((D3D8TextureWebGL*)pTexture)->glTexture);
        }
        return D3D_OK;
    }

    virtual HRESULT SetTransform(int State, const D3DMATRIX* pMatrix) override {
        if (State == 256) glUniformMatrix4fv(m_uModel, 1, GL_FALSE, (float*)pMatrix); // D3DTS_WORLD
        else if (State == 2) glUniformMatrix4fv(m_uView, 1, GL_FALSE, (float*)pMatrix); // D3DTS_VIEW
        else if (State == 3) glUniformMatrix4fv(m_uProj, 1, GL_FALSE, (float*)pMatrix); // D3DTS_PROJECTION
        return D3D_OK;
    }

    virtual HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8** ppTexture) override {
        *ppTexture = new D3D8TextureWebGL();
        return D3D_OK;
    }

    virtual HRESULT GetDeviceCaps(void* pCaps) override {
        return D3D_OK;
    }

    virtual HRESULT SetViewport(const void* pViewport) override {
        return D3D_OK;
    }

    virtual HRESULT SetMaterial(const void* pMaterial) override {
        // Map D3DMATERIAL8 to uniforms
        if (pMaterial) {
            float* m = (float*)pMaterial;
            // Diffuse is first 4 floats
            glUniform4fv(glGetUniformLocation(m_program, "uMaterialDiffuse"), 1, m);
        }
        return D3D_OK;
    }

    virtual HRESULT SetLight(DWORD Index, const void* pLight) override {
        return D3D_OK;
    }

    virtual HRESULT LightEnable(DWORD Index, BOOL Enable) override {
        return D3D_OK;
    }
};

class D3D8WebGL : public IDirect3D8 {
public:
    virtual HRESULT Release() override {
        delete this;
        return D3D_OK;
    }

    virtual HRESULT CreateDevice(UINT Adapter, int DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, void* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) override {
        std::cout << "Creating WebGL Device Shim..." << std::endl;
        *ppReturnedDeviceInterface = new D3D8DeviceWebGL();
        return D3D_OK;
    }
};

extern "C" {
    IDirect3D8* Direct3DCreate8(UINT SDKVersion) {
        return new D3D8WebGL();
    }
}
