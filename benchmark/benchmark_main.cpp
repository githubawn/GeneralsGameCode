/*
 * SAGE Engine MSAA CopyRects Benchmark (v4 - Full Engine Path Simulation)
 *
 * This benchmark replicates the exact code paths from:
 *   - W3DSmudge::copyRect()          -- sub-rect extraction from render target
 *   - W3DSmudge::testHardwareSupport() -- draw->copy->compare round-trip test
 *   - W3DShaderManager::init()       -- RTT texture creation vs MSAA
 *   - W3DShaderManager::startRenderToTexture() -- SetRenderTarget with MSAA depth buffer
 *
 * Tests are run both WITHOUT MSAA (as a baseline) and WITH 4x MSAA.
 * Each sub-test prints PASS/FAIL with the exact HRESULT.
 */

#include <windows.h>
#include <d3d8.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "d3d8.lib")

// Test counters
static int g_passed = 0;
static int g_failed = 0;

void LogResult(const char* testName, bool pass, HRESULT hr)
{
    if (pass)
    {
        printf("  [PASS] %s (hr=0x%08X)\n", testName, hr);
        g_passed++;
    }
    else
    {
        printf("  [FAIL] %s (hr=0x%08X)\n", testName, hr);
        g_failed++;
    }
}

HWND CreateTestWindow()
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "DX8BenchmarkWnd";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("DX8BenchmarkWnd", "DX8 MSAA Benchmark",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, NULL, NULL, NULL, NULL);
    ShowWindow(hwnd, SW_SHOW);
    return hwnd;
}

IDirect3DDevice8* CreateDevice(IDirect3D8* d3d, HWND hwnd, D3DMULTISAMPLE_TYPE msaaType)
{
    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.MultiSampleType = msaaType;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.hDeviceWindow = hwnd;

    IDirect3DDevice8* device = NULL;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &device);
    if (FAILED(hr))
    {
        printf("  CreateDevice FAILED: 0x%08X\n", hr);
        return NULL;
    }
    return device;
}

// ---------------------------------------------------------------------------
// W3DShaderManager::init() simulation
// Q: Can we create a D3DUSAGE_RENDERTARGET texture when MSAA is active?
// Engine fix: Skip this if backbuffer is multisampled to avoid depth mismatch.
// ---------------------------------------------------------------------------
void TestShaderManagerInit(IDirect3DDevice8* device, const char* label)
{
    printf("\n[W3DShaderManager::init] %s\n", label);

    IDirect3DSurface8* oldRenderTarget = NULL;
    HRESULT hr = device->GetRenderTarget(&oldRenderTarget);
    LogResult("GetRenderTarget", SUCCEEDED(hr), hr);
    if (FAILED(hr)) return;

    D3DSURFACE_DESC desc;
    oldRenderTarget->GetDesc(&desc);
    printf("  BackBuffer: %ux%u MultisampleType=%u\n", desc.Width, desc.Height, desc.MultiSampleType);

    // Test 1: Can we create a render-target texture at the same size/format?
    IDirect3DTexture8* renderTexture = NULL;
    hr = device->CreateTexture(desc.Width, desc.Height, 1,
        D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &renderTexture);
    LogResult("CreateTexture(RENDERTARGET, same format)", SUCCEEDED(hr), hr);

    if (SUCCEEDED(hr))
    {
        IDirect3DSurface8* newRenderSurface = NULL;
        hr = renderTexture->GetSurfaceLevel(0, &newRenderSurface);
        LogResult("GetSurfaceLevel(0)", SUCCEEDED(hr), hr);

        if (SUCCEEDED(hr))
        {
            // Test 2 (critical): SetRenderTarget with MSAA depth buffer.
            // This is the direct cause of the blackout.
            // DX8 spec: SetRenderTarget target and depth buffer must match MultiSampleType.
            IDirect3DSurface8* depthStencil = NULL;
            hr = device->GetDepthStencilSurface(&depthStencil);
            LogResult("GetDepthStencilSurface", SUCCEEDED(hr), hr);

            D3DSURFACE_DESC depthDesc;
            depthStencil->GetDesc(&depthDesc);
            printf("  DepthStencil MultisampleType=%u\n", depthDesc.MultiSampleType);

            // This SHOULD FAIL if backbuffer is MSAA but renderTexture is not.
            hr = device->SetRenderTarget(newRenderSurface, depthStencil);
            bool isMsaa = (desc.MultiSampleType != D3DMULTISAMPLE_NONE);
            bool expectFail = isMsaa; // should fail on MSAA, succeed on non-MSAA
            LogResult("SetRenderTarget(non-MSAA-texture, MSAA-depthbuffer) [expect FAIL on MSAA]",
                (FAILED(hr) == expectFail), hr);

            if (SUCCEEDED(hr))
            {
                // Restore render target
                device->SetRenderTarget(oldRenderTarget, depthStencil);
            }

            if (depthStencil) depthStencil->Release();
            if (newRenderSurface) newRenderSurface->Release();
        }
        if (renderTexture) renderTexture->Release();
    }

    if (oldRenderTarget) oldRenderTarget->Release();
}

// ---------------------------------------------------------------------------
// W3DSmudge::copyRect() simulation
// Q: Can we do whole-surface resolve + sub-rect extraction?
// ---------------------------------------------------------------------------
void TestCopyRect(IDirect3DDevice8* device, const char* label)
{
    printf("\n[W3DSmudge::copyRect] %s\n", label);

    // Grab render target (the MSAA or non-MSAA backbuffer)
    IDirect3DSurface8* surface = NULL;
    HRESULT hr = device->GetRenderTarget(&surface);
    LogResult("GetRenderTarget", SUCCEEDED(hr), hr);
    if (FAILED(hr)) return;

    D3DSURFACE_DESC desc;
    surface->GetDesc(&desc);
    printf("  Surface: %ux%u MultisampleType=%u\n", desc.Width, desc.Height, desc.MultiSampleType);

    // Parameters for extraction (mimicking copyRect: oX=0, oY=0, BLOCK_SIZE=8)
    const int oX = 0, oY = 0, width = 8, height = 8;
    RECT srcRect = { oX, oY, oX + width, oY + height };
    POINT dstPoint = { 0, 0 };

    // Create temp destination surface (8x8 system memory, like copyRect does)
    IDirect3DSurface8* tempSurface = NULL;
    hr = device->CreateImageSurface(width, height, desc.Format, &tempSurface);
    LogResult("CreateImageSurface (8x8 dest, POOL_SYSTEMMEM)", SUCCEEDED(hr), hr);
    if (FAILED(hr)) { surface->Release(); return; }

    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        printf("  Path: MSAA -> full resolve to intermediate, then sub-rect copy\n");

        // Step A: Whole-surface resolve to intermediate (legal on MSAA)
        IDirect3DSurface8* resolvedSurface = NULL;
        hr = device->CreateImageSurface(desc.Width, desc.Height, desc.Format, &resolvedSurface);
        LogResult("CreateImageSurface (full intermediate, POOL_SYSTEMMEM)", SUCCEEDED(hr), hr);

        if (SUCCEEDED(hr))
        {
            hr = device->CopyRects(surface, NULL, 0, resolvedSurface, NULL);
            LogResult("CopyRects(MSAA->intermediate, NULL rects) [should PASS]", SUCCEEDED(hr), hr);

            if (SUCCEEDED(hr))
            {
                // Step B: Sub-rect from intermediate (non-MSAA, so legal)
                hr = device->CopyRects(resolvedSurface, &srcRect, 1, tempSurface, &dstPoint);
                LogResult("CopyRects(intermediate->8x8, sub-rect) [should PASS]", SUCCEEDED(hr), hr);
            }

            if (resolvedSurface) resolvedSurface->Release();
        }
    }
    else
    {
        printf("  Path: No MSAA -> direct sub-rect copy (original code path)\n");

        // Direct sub-rect from non-MSAA surface (always legal)
        hr = device->CopyRects(surface, &srcRect, 1, tempSurface, &dstPoint);
        LogResult("CopyRects(non-MSAA->8x8, sub-rect) [should PASS]", SUCCEEDED(hr), hr);
    }

    // Also test the illegal path for documentation:
    printf("  Illegal sub-rect path result for reference:\n");
    hr = device->CopyRects(surface, &srcRect, 1, tempSurface, &dstPoint);
    bool isMsaa = (desc.MultiSampleType != D3DMULTISAMPLE_NONE);
    LogResult("CopyRects(surface->8x8, sub-rect directly) [expect FAIL on MSAA]",
        (FAILED(hr) == isMsaa), hr);

    if (tempSurface) tempSurface->Release();
    if (surface) surface->Release();
}

// ---------------------------------------------------------------------------
// W3DSmudge::testHardwareSupport() -- background capture round-trip
// Draw unique color, copy region out, compare with expected.
// ---------------------------------------------------------------------------
void TestHardwareSupportCapture(IDirect3DDevice8* device, const char* label)
{
    printf("\n[W3DSmudge::testHardwareSupport (capture round-trip)] %s\n", label);

    // Key DX8 observation: CopyRects from a backbuffer reads GPU-submitted data.
    // On MSAA back buffers, the data is not directly readable via CopyRects even during
    // a scene (GPU command buffer is async). The only reliable way to read pixels that
    // were drawn is: Draw -> EndScene -> Present -> GetFrontBuffer (which is the resolved copy).
    const DWORD UNIQUE_COLOR = 0xFF345678;
    const DWORD UNIQUE_COLOR_RGB = UNIQUE_COLOR & 0x00FFFFFF; // 0x345678
    const int BLOCK_SIZE = 8;

    // Setup render states before scene
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);

    // Draw a unique color to top-left 8x8 pixels
    device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF888888, 1.0f, 0);
    device->BeginScene();

    struct Vertex { float x, y, z, rhw; DWORD color; };
    Vertex verts[4] = {
        { float(BLOCK_SIZE) - 0.5f, float(BLOCK_SIZE) - 0.5f, 0.0f, 1.0f, UNIQUE_COLOR },
        { float(BLOCK_SIZE) - 0.5f,                    -0.5f, 0.0f, 1.0f, UNIQUE_COLOR },
        {                    -0.5f, float(BLOCK_SIZE) - 0.5f, 0.0f, 1.0f, UNIQUE_COLOR },
        {                    -0.5f,                    -0.5f, 0.0f, 1.0f, UNIQUE_COLOR },
    };
    device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    HRESULT hrDraw = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(Vertex));
    device->EndScene();
    printf("  DrawPrimitiveUP hr=0x%08X\n", hrDraw);

    // Present forces GPU to flush MSAA resolve to the front buffer
    HRESULT hrPresent = device->Present(NULL, NULL, NULL, NULL);
    printf("  Present hr=0x%08X\n", hrPresent);

    // Test A: Use GetFrontBuffer (post-Present resolved surface) for pixel verification.
    // In DX8, GetFrontBuffer copies the front buffer INTO a pre-allocated SYSTEMMEM surface.
    IDirect3DSurface8* frontSurface = NULL;
    // GetFrontBuffer always returns D3DFMT_A8R8G8B8 (32bpp)
    HRESULT hr = device->CreateImageSurface(640, 480, D3DFMT_A8R8G8B8, &frontSurface);
    LogResult("CreateImageSurface for GetFrontBuffer", SUCCEEDED(hr), hr);

    if (SUCCEEDED(hr))
    {
        hr = device->GetFrontBuffer(frontSurface);
        LogResult("GetFrontBuffer (post-Present, resolved)", SUCCEEDED(hr), hr);

        if (SUCCEEDED(hr))
        {
            D3DLOCKED_RECT lrect;
            hr = frontSurface->LockRect(&lrect, NULL, D3DLOCK_READONLY);
            if (SUCCEEDED(hr))
            {
                DWORD* pixels = (DWORD*)lrect.pBits;
                DWORD got = pixels[0] & 0x00FFFFFF;
                printf("  FrontBuffer Pixel[0,0]: got=0x%06X expected=0x%06X\n", got, UNIQUE_COLOR_RGB);
                LogResult("GetFrontBuffer pixel matches drawn color",
                    (got == UNIQUE_COLOR_RGB), S_OK);
                frontSurface->UnlockRect();
            }
        }
        frontSurface->Release();
    }

    // Test B: Also simulate the engine's CopyRects path to verify it runs without errors.
    // The engine uses this for the smudge background capture during a live frame.
    IDirect3DSurface8* surface = NULL;
    device->GetRenderTarget(&surface);
    D3DSURFACE_DESC desc;
    surface->GetDesc(&desc);
    printf("  RenderTarget: %ux%u MultisampleType=%u\n",
        desc.Width, desc.Height, desc.MultiSampleType);

    const int W = BLOCK_SIZE, H = BLOCK_SIZE;
    RECT srcRect = { 0, 0, W, H };
    POINT dstPoint = { 0, 0 };
    IDirect3DSurface8* tempSurface = NULL;
    hr = device->CreateImageSurface(W, H, desc.Format, &tempSurface);
    LogResult("CreateImageSurface (8x8 temp dest)", SUCCEEDED(hr), hr);

    bool copyOk = false;
    if (SUCCEEDED(hr))
    {
        if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
        {
            // Engine path: resolve to intermediate first
            IDirect3DSurface8* resolvedSurface = NULL;
            hr = device->CreateImageSurface(desc.Width, desc.Height, desc.Format, &resolvedSurface);
            if (SUCCEEDED(hr))
            {
                hr = device->CopyRects(surface, NULL, 0, resolvedSurface, NULL);
                LogResult("CopyRects(MSAA->intermediate)", SUCCEEDED(hr), hr);
                if (SUCCEEDED(hr))
                {
                    hr = device->CopyRects(resolvedSurface, &srcRect, 1, tempSurface, &dstPoint);
                    LogResult("CopyRects(intermediate->8x8 sub-rect)", SUCCEEDED(hr), hr);
                    copyOk = SUCCEEDED(hr);
                }
                resolvedSurface->Release();
            }
        }
        else
        {
            hr = device->CopyRects(surface, &srcRect, 1, tempSurface, &dstPoint);
            LogResult("CopyRects(non-MSAA->8x8 sub-rect)", SUCCEEDED(hr), hr);
            copyOk = SUCCEEDED(hr);
        }

        if (copyOk)
        {
            D3DLOCKED_RECT lrect;
            if (SUCCEEDED(tempSurface->LockRect(&lrect, NULL, D3DLOCK_READONLY)))
            {
                DWORD* px = (DWORD*)lrect.pBits;
                printf("  CopyRects Pixel[0,0]: got=0x%06X\n", px[0] & 0x00FFFFFF);
                tempSurface->UnlockRect();
            }
        }
        tempSurface->Release();
    }

    if (surface) surface->Release();
}

// ---------------------------------------------------------------------------
// W3DSmudgeManager::ReAcquireResources: create background texture
// Tests that a POOL_DEFAULT render-target texture can be created for the copy path.
// ---------------------------------------------------------------------------
void TestBackgroundTextureCreation(IDirect3DDevice8* device, const char* label)
{
    printf("\n[W3DSmudgeManager::ReAcquireResources] %s\n", label);

    IDirect3DSurface8* backbuffer = NULL;
    device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    D3DSURFACE_DESC desc;
    backbuffer->GetDesc(&desc);
    backbuffer->Release();

    // Create background texture for copy path (POOL_DEFAULT + dynamic flag)
    // This is what TextureClass::POOL_DEFAULT resolves to in the engine.
    IDirect3DTexture8* bgTex = NULL;
    HRESULT hr = device->CreateTexture(
        640, 480, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &bgTex);
    LogResult("CreateTexture(640x480, RENDERTARGET, POOL_DEFAULT) [for m_backgroundTexture]",
        SUCCEEDED(hr), hr);

    if (bgTex) bgTex->Release();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main()
{
    printf("=============================================================\n");
    printf(" SAGE MSAA CopyRects Benchmark v4 - Engine Path Simulation\n");
    printf("=============================================================\n\n");

    IDirect3D8* d3d = Direct3DCreate8(D3D_SDK_VERSION);
    if (!d3d) { printf("FATAL: Direct3DCreate8 failed.\n"); return 1; }

    HWND hwnd = CreateTestWindow();

    // Check MSAA support
    HRESULT hrMsaa = d3d->CheckDeviceMultiSampleType(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_4_SAMPLES);
    printf("4x MSAA support: %s (0x%08X)\n\n", SUCCEEDED(hrMsaa) ? "YES" : "NO", hrMsaa);

    // === PASS 1: Non-MSAA device (baseline) ===
    printf("============================================================\n");
    printf(" PASS 1: Non-MSAA Device (baseline)\n");
    printf("============================================================\n");
    IDirect3DDevice8* devNoMsaa = CreateDevice(d3d, hwnd, D3DMULTISAMPLE_NONE);
    if (devNoMsaa)
    {
        TestShaderManagerInit(devNoMsaa, "No MSAA");
        TestCopyRect(devNoMsaa, "No MSAA");
        TestHardwareSupportCapture(devNoMsaa, "No MSAA");
        TestBackgroundTextureCreation(devNoMsaa, "No MSAA");
        devNoMsaa->Release();
    }

    // === PASS 2: 4x MSAA device ===
    printf("\n============================================================\n");
    printf(" PASS 2: 4x MSAA Device\n");
    printf("============================================================\n");
    if (SUCCEEDED(hrMsaa))
    {
        IDirect3DDevice8* devMsaa = CreateDevice(d3d, hwnd, D3DMULTISAMPLE_4_SAMPLES);
        if (devMsaa)
        {
            TestShaderManagerInit(devMsaa, "4x MSAA");
            TestCopyRect(devMsaa, "4x MSAA");
            TestHardwareSupportCapture(devMsaa, "4x MSAA");
            TestBackgroundTextureCreation(devMsaa, "4x MSAA");
            devMsaa->Release();
        }
    }
    else
    {
        printf("  Skipping PASS 2: 4x MSAA not supported by driver.\n");
    }

    printf("\n============================================================\n");
    printf(" Results: %d PASSED, %d FAILED\n", g_passed, g_failed);
    printf("============================================================\n");

    d3d->Release();
    DestroyWindow(hwnd);
    return (g_failed > 0) ? 1 : 0;
}
