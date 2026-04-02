#include "pch.h"
#include "sph_fluid.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Engine/Collision/map_collision.h"

#pragma comment(lib, "d3dcompiler.lib")

#ifndef PI
#define PI 3.14159265358979323846f
#endif

namespace Engine {

    // ============================================================
    SPHFluid::SPHFluid() {}
    SPHFluid::~SPHFluid() { Shutdown(); }

    // ============================================================
    // Initialize — matches original signature (device, maxParticles)
    // ============================================================
    bool SPHFluid::Initialize(ID3D11Device* device, int maxParticles) {
        m_device = device;
        m_maxParticles = (uint32_t)maxParticles;
        m_particles.reserve(m_maxParticles);

        // Get immediate context
        device->GetImmediateContext(&m_context);

        // Get screen size from device (via back buffer)
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory> factory;
        device->QueryInterface(__uuidof(IDXGIDevice), (void**)dxgiDevice.GetAddressOf());

        // Default screen size — will be updated on first draw if needed
        m_screenWidth = 1280;
        m_screenHeight = 720;

        // Try to get actual size from swap chain
        if (m_context) {
            ComPtr<ID3D11RenderTargetView> rtv;
            m_context->OMGetRenderTargets(1, rtv.GetAddressOf(), nullptr);
            if (rtv) {
                ComPtr<ID3D11Resource> res;
                rtv->GetResource(res.GetAddressOf());
                ComPtr<ID3D11Texture2D> tex;
                if (SUCCEEDED(res.As(&tex))) {
                    D3D11_TEXTURE2D_DESC desc;
                    tex->GetDesc(&desc);
                    m_screenWidth = desc.Width;
                    m_screenHeight = desc.Height;
                }
            }
        }
        m_aspect = (float)m_screenWidth / (float)m_screenHeight;

        // SPHパラメータのデフォルト値
        m_params.smoothingRadius = 1.0f;
        m_params.restDensity = 1000.0f;
        m_params.gasConstant = 2.0f;
        m_params.viscosity = 3.5f;
        m_params.gravity = { 0.0f, -9.81f, 0.0f };  // ★ 重力
        m_params.deltaTime = 1.0f / 60.0f;
        m_params.boundaryMin = m_boundaryMin;
        m_params.boundaryMax = m_boundaryMax;
        m_params.particleCount = 0;

        m_initialized = true;
        m_ssInitialized = false; // screen-space resources created lazily

        OutputDebugStringA("SPHFluid: Initialize success!\n");
        char buf[128];
        sprintf_s(buf, "SPHFluid: screen=%ux%u maxParticles=%u\n", m_screenWidth, m_screenHeight, m_maxParticles);
        OutputDebugStringA(buf);
        return true;
    }

    // ============================================================
    // InitializeScreenSpace — lazy init for GPU resources
    // ============================================================
    bool SPHFluid::InitializeScreenSpace() {
        if (m_ssInitialized) return true;
        if (!m_device || !m_context) return false;

        if (!CreateBillboardResources()) { OutputDebugStringA("SPHFluid: CreateBillboardResources FAILED\n"); return false; }
        if (!InitializeShaders()) { OutputDebugStringA("SPHFluid: InitializeShaders FAILED\n"); return false; }
        if (!CreateRenderTargets()) { OutputDebugStringA("SPHFluid: CreateRenderTargets FAILED\n"); return false; }
        if (!CreateStates()) { OutputDebugStringA("SPHFluid: CreateStates FAILED\n"); return false; }
        if (!CreateConstantBuffers()) { OutputDebugStringA("SPHFluid: CreateConstantBuffers FAILED\n"); return false; }
        if (!CreateSamplers()) { OutputDebugStringA("SPHFluid: CreateSamplers FAILED\n"); return false; }

        m_ssInitialized = true;
        OutputDebugStringA("SPHFluid: Screen-space resources created!\n");
        return true;
    }

    // ============================================================
    void SPHFluid::Finalize() { Shutdown(); }

    void SPHFluid::Shutdown() {
        delete m_depthRT;     m_depthRT = nullptr;
        delete m_blurRT1;     m_blurRT1 = nullptr;
        delete m_blurRT2;     m_blurRT2 = nullptr;
        delete m_thicknessRT; m_thicknessRT = nullptr;
        delete m_thickBlurRT1; m_thickBlurRT1 = nullptr;
        delete m_thickBlurRT2; m_thickBlurRT2 = nullptr;
        m_particles.clear();
        m_particleCount = 0;
        m_initialized = false;
        m_ssInitialized = false;
    }

// ============================================================
// Shader compilation helper — BOM自動スキップ版
// ============================================================
    static HRESULT CompileShaderFromFile(const wchar_t* filename,
        const char* entryPoint,
        const char* target,
        ID3DBlob** blob) {

        // カレントディレクトリを出力（初回のみ）
        static bool logged = false;
        if (!logged) {
            wchar_t cwd[512];
            GetCurrentDirectoryW(512, cwd);
            char buf[1024];
            sprintf_s(buf, "SPHFluid: Current directory: %ls\n", cwd);
            OutputDebugStringA(buf);
            logged = true;
        }

        FILE* f = nullptr;
        _wfopen_s(&f, filename, L"rb");
        if (!f) {
            char buf[256];
            sprintf_s(buf, "SPHFluid: Cannot open shader file: %ls\n", filename);
            OutputDebugStringA(buf);
            return E_FAIL;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<char> data(size);
        fread(data.data(), 1, size, f);
        fclose(f);

        // ★★★ ファイルサイズと先頭60文字を出力 ★★★
        {
            char buf[256];
            sprintf_s(buf, "SPHFluid: Loaded %ls (%ld bytes) entry=%s\n", filename, size, entryPoint);
            OutputDebugStringA(buf);
            // 先頭60文字を表示
            char preview[64] = {};
            int previewLen = (size < 60) ? (int)size : 60;
            memcpy(preview, data.data(), previewLen);
            // 改行を空白に置換
            for (int i = 0; i < previewLen; i++) {
                if (preview[i] == '\n' || preview[i] == '\r') preview[i] = ' ';
            }
            preview[previewLen] = 0;
            sprintf_s(buf, "SPHFluid: Preview: [%s]\n", preview);
            OutputDebugStringA(buf);
        }

        // BOMスキップ
        const char* src = data.data();
        SIZE_T srcSize = (SIZE_T)size;
        if (srcSize >= 3 &&
            (unsigned char)src[0] == 0xEF &&
            (unsigned char)src[1] == 0xBB &&
            (unsigned char)src[2] == 0xBF) {
            src += 3;
            srcSize -= 3;
            OutputDebugStringA("SPHFluid: BOM detected and skipped\n");
        }
        if (srcSize >= 2 &&
            (unsigned char)src[0] == 0xFF &&
            (unsigned char)src[1] == 0xFE) {
            OutputDebugStringA("SPHFluid: Shader file is UTF-16! Save as UTF-8!\n");
            return E_FAIL;
        }

        DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3DCompile(src, srcSize, nullptr, nullptr, nullptr,
            entryPoint, target, flags, 0, blob, errorBlob.GetAddressOf());
        if (FAILED(hr) && errorBlob) {
            char buf[2048];
            sprintf_s(buf, "SPHFluid shader error [%s]: %s\n", entryPoint,
                (char*)errorBlob->GetBufferPointer());
            OutputDebugStringA(buf);
        }
        return hr;
    }



    // ============================================================
    bool SPHFluid::CreateBillboardResources() {
        XMFLOAT2 quadVerts[4] = { {-1,-1}, {-1,1}, {1,1}, {1,-1} };

        D3D11_BUFFER_DESC vbd = {};
        vbd.ByteWidth = sizeof(quadVerts);
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.Usage = D3D11_USAGE_DEFAULT;
        D3D11_SUBRESOURCE_DATA sd = { quadVerts };
        if (FAILED(m_device->CreateBuffer(&vbd, &sd, &m_pBillboardVB))) return false;

        UINT idx[6] = { 0,1,2, 0,2,3 };
        D3D11_BUFFER_DESC ibd = {};
        ibd.ByteWidth = sizeof(idx);
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibd.Usage = D3D11_USAGE_DEFAULT;
        D3D11_SUBRESOURCE_DATA id = { idx };
        if (FAILED(m_device->CreateBuffer(&ibd, &id, &m_pBillboardIB))) return false;

        D3D11_BUFFER_DESC inst = {};
        inst.ByteWidth = sizeof(XMFLOAT3) * m_maxParticles;
        inst.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        inst.Usage = D3D11_USAGE_DYNAMIC;
        inst.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_device->CreateBuffer(&inst, nullptr, &m_pInstanceBuffer))) return false;

        return true;
    }

    // ============================================================
    bool SPHFluid::InitializeShaders() {
        ID3DBlob* blob = nullptr;
        HRESULT hr;

        // === fluif_depth.hlsl ===
        hr = CompileShaderFromFile(L"fluid_depth.hlsl", "VS_Depth", "vs_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile VS_Depth\n"); return false; }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "INST_POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
        hr = m_device->CreateInputLayout(layout, 2, blob->GetBufferPointer(), blob->GetBufferSize(), m_pBillboardLayout.GetAddressOf());
        m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pDepthVS.GetAddressOf());
        blob->Release(); blob = nullptr;
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to create input layout\n"); return false; }

        hr = CompileShaderFromFile(L"fluid_depth.hlsl", "PS_Depth", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_Depth\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pDepthPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        // === fluif_thickness.hlsl ===
        hr = CompileShaderFromFile(L"fluid_thickness.hlsl", "PS_Thickness", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_Thickness\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pThicknessPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        // === fluid_blur.hlsl ===
        hr = CompileShaderFromFile(L"fluid_blur.hlsl", "VS_Quad", "vs_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile VS_Quad\n"); return false; }
        m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pQuadVS.GetAddressOf());
        blob->Release(); blob = nullptr;

        hr = CompileShaderFromFile(L"fluid_blur.hlsl", "PS_BilateralBlur", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_BilateralBlur\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pBilateralBlurPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        hr = CompileShaderFromFile(L"fluid_blur.hlsl", "PS_GaussianBlur", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_GaussianBlur\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pGaussianBlurPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        // === fluid_final.hlsl ===
        hr = CompileShaderFromFile(L"fluid_final.hlsl", "PS_Final", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_Final\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pFinalPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        OutputDebugStringA("SPHFluid: All shaders compiled successfully!\n");
        return true;
    }
    bool SPHFluid::CreateRenderTargets() {
        // 既存のRTを削除
        if (m_depthRT) { delete m_depthRT;     m_depthRT = nullptr; }
        if (m_blurRT1) { delete m_blurRT1;     m_blurRT1 = nullptr; }
        if (m_blurRT2) { delete m_blurRT2;     m_blurRT2 = nullptr; }
        if (m_thicknessRT) { delete m_thicknessRT; m_thicknessRT = nullptr; }
        if (m_thickBlurRT1) { delete m_thickBlurRT1; m_thickBlurRT1 = nullptr; }
        if (m_thickBlurRT2) { delete m_thickBlurRT2; m_thickBlurRT2 = nullptr; }

        UINT w = m_screenWidth;
        UINT h = m_screenHeight;
        UINT halfW = w / 2;
        UINT halfH = h / 2;

        m_depthRT = new RenderTarget();
        if (!m_depthRT->Create(m_device, w, h, DXGI_FORMAT_R32_FLOAT, false)) {  // ← m_device
            OutputDebugStringA("SPHFluid: Failed to create depthRT\n");
            return false;
        }

        m_blurRT1 = new RenderTarget();
        m_blurRT2 = new RenderTarget();
        if (!m_blurRT1->Create(m_device, w, h, DXGI_FORMAT_R32_FLOAT, false) ||  // ← m_device
            !m_blurRT2->Create(m_device, w, h, DXGI_FORMAT_R32_FLOAT, false)) {
            OutputDebugStringA("SPHFluid: Failed to create blurRTs\n");
            return false;
        }

        m_thicknessRT = new RenderTarget();
        if (!m_thicknessRT->Create(m_device, halfW, halfH, DXGI_FORMAT_R16_FLOAT, false)) {  // ← m_device
            OutputDebugStringA("SPHFluid: Failed to create thicknessRT\n");
            return false;
        }

        m_thickBlurRT1 = new RenderTarget();
        m_thickBlurRT2 = new RenderTarget();
        if (!m_thickBlurRT1->Create(m_device, halfW, halfH, DXGI_FORMAT_R16_FLOAT, false) ||
            !m_thickBlurRT2->Create(m_device, halfW, halfH, DXGI_FORMAT_R16_FLOAT, false)) {
            OutputDebugStringA("SPHFluid: Failed to create thickBlurRTs\n");
            return false;
        }

        OutputDebugStringA("SPHFluid: All render targets created\n");
        return true;
    }

    // ============================================================
    bool SPHFluid::CreateStates() {
        HRESULT hr;
        // アルファブレンド（水の半透明描画用）
        {
            D3D11_BLEND_DESC d = {};
            d.AlphaToCoverageEnable = FALSE;
            d.IndependentBlendEnable = FALSE;
            d.RenderTarget[0].BlendEnable = TRUE;
            d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
            d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;  // 修正
            d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            hr = m_device->CreateBlendState(&d, &m_pAlphaBlendState);
            if (FAILED(hr)) return false;
        }
        {
            D3D11_BLEND_DESC d = {};
            d.RenderTarget[0].BlendEnable = TRUE;
            d.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            d.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
            d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
            d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            hr = m_device->CreateBlendState(&d, &m_pAdditiveBlendState);
            if (FAILED(hr)) return false;
        }
        {
            D3D11_DEPTH_STENCIL_DESC d = {};
            d.DepthEnable = FALSE;
            d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            hr = m_device->CreateDepthStencilState(&d, &m_pDepthDisabledState);
            if (FAILED(hr)) return false;
        }
        {
            D3D11_RASTERIZER_DESC d = {};
            d.FillMode = D3D11_FILL_SOLID;
            d.CullMode = D3D11_CULL_NONE;
            d.DepthClipEnable = TRUE;
            hr = m_device->CreateRasterizerState(&d, &m_pNoCullRS);
            if (FAILED(hr)) return false;
        }
        return true;
    }

    // ============================================================
    bool SPHFluid::CreateConstantBuffers() {
        auto makeCB = [&](UINT size, ComPtr<ID3D11Buffer>& buf) -> bool {
            D3D11_BUFFER_DESC d = {};
            d.ByteWidth = (size + 15) & ~15;
            d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            d.Usage = D3D11_USAGE_DYNAMIC;
            d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            return SUCCEEDED(m_device->CreateBuffer(&d, nullptr, buf.GetAddressOf()));
            };
        if (!makeCB(sizeof(CBCamera), m_pCameraCB)) return false;
        if (!makeCB(sizeof(CBBlur), m_pBlurCB))   return false;
        if (!makeCB(sizeof(CBFinal), m_pFinalCB))  return false;
        return true;
    }

    // ============================================================
    bool SPHFluid::CreateSamplers() {
        {
            D3D11_SAMPLER_DESC d = {};
            d.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            if (FAILED(m_device->CreateSamplerState(&d, &m_pPointSampler))) return false;
        }
        {
            D3D11_SAMPLER_DESC d = {};
            d.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            if (FAILED(m_device->CreateSamplerState(&d, &m_pLinearSampler))) return false;
        }
        return true;
    }

    // ============================================================
    void SPHFluid::UpdateInstanceBuffer(ID3D11DeviceContext* ctx) {
        if (m_particleCount == 0) return;
        D3D11_MAPPED_SUBRESOURCE mp;
        if (FAILED(ctx->Map(m_pInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) return;
        XMFLOAT3* dst = (XMFLOAT3*)mp.pData;
        for (uint32_t i = 0; i < m_particleCount; i++)
            dst[i] = m_particles[i].position;
        ctx->Unmap(m_pInstanceBuffer.Get(), 0);
    }

    // ============================================================
    void SPHFluid::SpawnParticles(const XMFLOAT3& c, uint32_t count, float radius) {
        uint32_t n = std::min(count, m_maxParticles - m_particleCount);
        char buf[128];
        sprintf_s(buf, "SPHFluid::SpawnParticles requested=%u actual=%u current=%u\n", count, n, m_particleCount);
        OutputDebugStringA(buf);
        for (uint32_t i = 0; i < n; ++i) {
            SPHParticle p;
            float th = (float)rand() / RAND_MAX * 2.0f * PI;
            float ph = (float)rand() / RAND_MAX * PI;
            float r = (float)rand() / RAND_MAX * radius;
            p.position = { c.x + r * sinf(ph) * cosf(th), c.y + r * sinf(ph) * sinf(th), c.z + r * cosf(ph) };
            p.velocity = { 0,0,0 }; p.acceleration = { 0,0,0 };
            p.density = m_params.restDensity; p.pressure = 0;
            p.mass = 1.0f;  // ← fixed: use literal, not m_params.particleMass
            p.lifetime = m_particleLifetime;
            m_particles.push_back(p); m_particleCount++;
        }
        m_params.particleCount = m_particleCount;
    }


    // ============================================================
    void SPHFluid::SpawnParticleWithVelocity(const XMFLOAT3& pos, const XMFLOAT3& vel) {
        if (m_particleCount >= m_maxParticles) {
            static int log = 0;
            if (log++ < 3) OutputDebugStringA("SPHFluid: SpawnParticle REJECTED (max reached)\n");
            return;
        }
        SPHParticle p;
        p.position = pos; p.velocity = vel; p.acceleration = { 0,0,0 };
        p.density = m_params.restDensity; p.pressure = 0;
        p.mass = 1.0f;
        p.lifetime = m_particleLifetime;
        m_particles.push_back(p); m_particleCount++;
        m_params.particleCount = m_particleCount;

        static int log2 = 0;
        if (log2++ < 5) {
            char buf[256];
            sprintf_s(buf, "SPHFluid: Spawned! cnt=%u pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f)\n",
                m_particleCount, pos.x, pos.y, pos.z, vel.x, vel.y, vel.z);
            OutputDebugStringA(buf);
        }
    }


    // ============================================================
    float SPHFluid::Poly6Kernel(float r, float h) {
        if (r >= h) return 0;
        float d = h * h - r * r;
        return 315.0f / (64.0f * PI * powf(h, 9)) * d * d * d;
    }

    XMFLOAT3 SPHFluid::SpikyGradient(const XMFLOAT3& diff, float r, float h) {
        if (r >= h || r < 1e-6f) return { 0,0,0 };
        float c = -45.0f / (PI * powf(h, 6)) * powf(h - r, 2) / r;
        return { c * diff.x, c * diff.y, c * diff.z };
    }

    float SPHFluid::ViscosityLaplacian(float r, float h) {
        if (r >= h) return 0;
        return 45.0f / (PI * powf(h, 6)) * (h - r);
    }

    // ============================================================
    void SPHFluid::EnforceBoundary(SPHParticle& p) {
        float b = 0.3f;
        if (p.position.x < m_boundaryMin.x) { p.position.x = m_boundaryMin.x; p.velocity.x *= -b; }
        if (p.position.x > m_boundaryMax.x) { p.position.x = m_boundaryMax.x; p.velocity.x *= -b; }
        if (p.position.y < m_boundaryMin.y) { p.position.y = m_boundaryMin.y; p.velocity.y *= -b; }
        if (p.position.y > m_boundaryMax.y) { p.position.y = m_boundaryMax.y; p.velocity.y *= -b; }
        if (p.position.z < m_boundaryMin.z) { p.position.z = m_boundaryMin.z; p.velocity.z *= -b; }
        if (p.position.z > m_boundaryMax.z) { p.position.z = m_boundaryMax.z; p.velocity.z *= -b; }
    }

    // ============================================================
// マップブロックとの衝突
// ============================================================
    void SPHFluid::CollideWithMap() {
        float bounce = 0.3f;
        float particleRadius = m_particleScale;

        for (uint32_t i = 0; i < m_particleCount; i++) {
            XMFLOAT3& pos = m_particles[i].position;
            XMFLOAT3& vel = m_particles[i].velocity;

            // パーティクルを小さなBoxColliderとして扱う
            Engine::BoxCollider particleBox(pos, { particleRadius, particleRadius, particleRadius });

            // 近くのブロックを取得して衝突チェック
            auto& mapCol = Engine::MapCollision::GetInstance();
            auto nearbyBlocks = mapCol.GetNearbyBlocks(pos, 1.5f);

            for (auto* block : nearbyBlocks) {
                XMFLOAT3 pen;
                if (particleBox.ComputePenetration(block, pen)) {
                    // 位置を押し戻し
                    pos.x += pen.x;
                    pos.y += pen.y;
                    pos.z += pen.z;

                    // 衝突した軸の速度を反転・減衰
                    if (pen.x != 0.0f) { vel.x *= -bounce; }
                    if (pen.y != 0.0f) { vel.y *= -bounce; }
                    if (pen.z != 0.0f) { vel.z *= -bounce; }

                    // コライダーの位置を更新して再判定に備える
                    particleBox.SetCenter(pos);
                }
            }
        }
    }

    // ============================================================
    // プレイヤーとの衝突
    // ============================================================
    void SPHFluid::CollideWithPlayers() {
        if (m_playerBoxes.empty()) return;

        float bounce = 0.2f;
        float particleRadius = m_particleScale;

        for (uint32_t i = 0; i < m_particleCount; i++) {
            XMFLOAT3& pos = m_particles[i].position;
            XMFLOAT3& vel = m_particles[i].velocity;

            for (const auto& pbox : m_playerBoxes) {
                // パーティクルがプレイヤーのAABB内にあるか
                float halfX = pbox.halfSize.x;
                float halfY = pbox.halfSize.y;
                float halfZ = pbox.halfSize.z;

                float dx = pos.x - pbox.center.x;
                float dy = pos.y - pbox.center.y;
                float dz = pos.z - pbox.center.z;

                if (fabsf(dx) < halfX + particleRadius &&
                    fabsf(dy) < halfY + particleRadius &&
                    fabsf(dz) < halfZ + particleRadius) {
                    // めり込み量を計算（最小軸で押し出し）
                    float overlapX = (halfX + particleRadius) - fabsf(dx);
                    float overlapY = (halfY + particleRadius) - fabsf(dy);
                    float overlapZ = (halfZ + particleRadius) - fabsf(dz);

                    if (overlapX <= overlapY && overlapX <= overlapZ) {
                        pos.x += (dx > 0 ? overlapX : -overlapX);
                        vel.x *= -bounce;
                    } else if (overlapY <= overlapZ) {
                        pos.y += (dy > 0 ? overlapY : -overlapY);
                        vel.y *= -bounce;
                    } else {
                        pos.z += (dz > 0 ? overlapZ : -overlapZ);
                        vel.z *= -bounce;
                    }
                }
            }
        }
    }


    // ============================================================
    void SPHFluid::SimulateCPU(float dt) {
        float h = m_params.smoothingRadius;
        uint32_t n = m_particleCount;
        float damping = 0.98f;

        // パーティクル数が多すぎる場合は重力だけ適用
        const uint32_t MAX_SIM = 200;
        if (n > MAX_SIM) {
            for (uint32_t i = 0; i < n; i++) {
                m_particles[i].velocity.x += m_params.gravity.x * dt;
                m_particles[i].velocity.y += m_params.gravity.y * dt;
                m_particles[i].velocity.z += m_params.gravity.z * dt;
                m_particles[i].velocity.x *= damping;
                m_particles[i].velocity.y *= damping;
                m_particles[i].velocity.z *= damping;
                float maxVel = 50.0f;
                m_particles[i].velocity.x = fmaxf(-maxVel, fminf(maxVel, m_particles[i].velocity.x));
                m_particles[i].velocity.y = fmaxf(-maxVel, fminf(maxVel, m_particles[i].velocity.y));
                m_particles[i].velocity.z = fmaxf(-maxVel, fminf(maxVel, m_particles[i].velocity.z));
                m_particles[i].position.x += m_particles[i].velocity.x * dt;
                m_particles[i].position.y += m_particles[i].velocity.y * dt;
                m_particles[i].position.z += m_particles[i].velocity.z * dt;
                EnforceBoundary(m_particles[i]);
            }
            return;
        }

        // 少なすぎる場合はシミュレーションスキップ（重力だけ適用）
        if (n < 2) {
            for (uint32_t i = 0; i < n; i++) {
                m_particles[i].velocity.x += m_params.gravity.x * dt;
                m_particles[i].velocity.y += m_params.gravity.y * dt;
                m_particles[i].velocity.z += m_params.gravity.z * dt;
                m_particles[i].velocity.x *= damping;
                m_particles[i].velocity.y *= damping;
                m_particles[i].velocity.z *= damping;
                m_particles[i].position.x += m_particles[i].velocity.x * dt;
                m_particles[i].position.y += m_particles[i].velocity.y * dt;
                m_particles[i].position.z += m_particles[i].velocity.z * dt;
                EnforceBoundary(m_particles[i]);
            }
            return;
        }

        // Density & pressure
        for (uint32_t i = 0; i < n; i++) {
            m_particles[i].density = 0;
            for (uint32_t j = 0; j < n; j++) {
                float dx = m_particles[i].position.x - m_particles[j].position.x;
                float dy = m_particles[i].position.y - m_particles[j].position.y;
                float dz = m_particles[i].position.z - m_particles[j].position.z;
                float r = sqrtf(dx * dx + dy * dy + dz * dz);
                m_particles[i].density += m_particles[j].mass * Poly6Kernel(r, h);
            }
            // ★ 密度の下限を保証（NaN防止）
            if (m_particles[i].density < 1.0f) m_particles[i].density = m_params.restDensity;
            m_particles[i].pressure = m_params.gasConstant * (m_particles[i].density - m_params.restDensity);
        }

        // Forces
        for (uint32_t i = 0; i < n; i++) {
            XMFLOAT3 fP = { 0,0,0 }, fV = { 0,0,0 };
            XMFLOAT3 fCohesion = { 0,0,0 };  // ★追加：凝集力

            for (uint32_t j = 0; j < n; j++) {
                if (i == j) continue;
                float dx = m_particles[i].position.x - m_particles[j].position.x;
                float dy = m_particles[i].position.y - m_particles[j].position.y;
                float dz = m_particles[i].position.z - m_particles[j].position.z;
                float r = sqrtf(dx * dx + dy * dy + dz * dz);

                if (r >= h || r < 1e-6f) continue;

                // 既存の圧力・粘性計算
                float pA = (m_particles[i].pressure + m_particles[j].pressure) * 0.5f;
                XMFLOAT3 g = SpikyGradient({ dx,dy,dz }, r, h);
                float mR = m_particles[j].mass / m_particles[j].density;
                fP.x += -mR * pA * g.x;
                fP.y += -mR * pA * g.y;
                fP.z += -mR * pA * g.z;

                float lap = ViscosityLaplacian(r, h);
                fV.x += mR * (m_particles[j].velocity.x - m_particles[i].velocity.x) * lap;
                fV.y += mR * (m_particles[j].velocity.y - m_particles[i].velocity.y) * lap;
                fV.z += mR * (m_particles[j].velocity.z - m_particles[i].velocity.z) * lap;

                // ★追加：表面張力的な凝集力（パーティクルを引き寄せる）
                float cohesionStrength = 2.0f;
                float cohesion = Poly6Kernel(r, h) * cohesionStrength;
                fCohesion.x -= dx / r * cohesion;
                fCohesion.y -= dy / r * cohesion;
                fCohesion.z -= dz / r * cohesion;
            }

            fV.x *= m_params.viscosity;
            fV.y *= m_params.viscosity;
            fV.z *= m_params.viscosity;

            float inv = 1.0f / m_particles[i].density;
            if (!isfinite(inv)) inv = 1.0f / m_params.restDensity;

            m_particles[i].acceleration.x = (fP.x + fV.x + fCohesion.x) * inv + m_params.gravity.x;
            m_particles[i].acceleration.y = (fP.y + fV.y + fCohesion.y) * inv + m_params.gravity.y;
            m_particles[i].acceleration.z = (fP.z + fV.z + fCohesion.z) * inv + m_params.gravity.z;

            // ★ 加速度の上限クランプ
            float maxAcc = 500.0f;
            m_particles[i].acceleration.x = fmaxf(-maxAcc, fminf(maxAcc, m_particles[i].acceleration.x));
            m_particles[i].acceleration.y = fmaxf(-maxAcc, fminf(maxAcc, m_particles[i].acceleration.y));
            m_particles[i].acceleration.z = fmaxf(-maxAcc, fminf(maxAcc, m_particles[i].acceleration.z));
        }

        // Integrate
        float maxVel = 50.0f;  // ★ 速度上限
        for (uint32_t i = 0; i < n; i++) {
            m_particles[i].velocity.x += m_particles[i].acceleration.x * dt;
            m_particles[i].velocity.y += m_particles[i].acceleration.y * dt;
            m_particles[i].velocity.z += m_particles[i].acceleration.z * dt;

            m_particles[i].velocity.x *= damping;
            m_particles[i].velocity.y *= damping;
            m_particles[i].velocity.z *= damping;

            // ★ 速度クランプ
            m_particles[i].velocity.x = fmaxf(-maxVel, fminf(maxVel, m_particles[i].velocity.x));
            m_particles[i].velocity.y = fmaxf(-maxVel, fminf(maxVel, m_particles[i].velocity.y));
            m_particles[i].velocity.z = fmaxf(-maxVel, fminf(maxVel, m_particles[i].velocity.z));

            m_particles[i].position.x += m_particles[i].velocity.x * dt;
            m_particles[i].position.y += m_particles[i].velocity.y * dt;
            m_particles[i].position.z += m_particles[i].velocity.z * dt;

            // ★ NaN検出 → リセット
            if (!isfinite(m_particles[i].position.x) || !isfinite(m_particles[i].position.y) || !isfinite(m_particles[i].position.z)) {
                m_particles[i].position = { 0, 0, 0 };
                m_particles[i].velocity = { 0, 0, 0 };
                m_particles[i].acceleration = { 0, 0, 0 };
            }

            EnforceBoundary(m_particles[i]);
        }
        // コリジョン
        if (m_mapCollisionEnabled) {
            CollideWithMap();
        }
        if (m_playerCollisionEnabled) {
            CollideWithPlayers();
        }
    }


    // ============================================================
    void SPHFluid::Update(ID3D11DeviceContext* ctx, float dt) {
        if (m_particleCount == 0) return;
        for (size_t i = 0; i < m_particles.size(); ) {
            m_particles[i].lifetime -= dt;
            if (m_particles[i].lifetime <= 0) {
                m_particles[i] = m_particles.back(); m_particles.pop_back(); m_particleCount--;
            } else ++i;
        }
        m_params.particleCount = m_particleCount;
        if (m_particleCount == 0) return;
        SimulateCPU(dt);
    }

    // ============================================================
    void SPHFluid::Draw(ID3D11DeviceContext* ctx) {
        static int drawLog = 0;
        if (drawLog < 10 || drawLog % 600 == 0) {
            char buf[256];
            sprintf_s(buf, "SPHFluid::Draw init=%d cnt=%u ss=%d ssInit=%d\n",
                m_initialized ? 1 : 0, m_particleCount, m_screenSpaceEnabled ? 1 : 0, m_ssInitialized ? 1 : 0);
            OutputDebugStringA(buf);
        }
        drawLog++;

        if (!m_initialized || m_particleCount == 0) return;

        if (m_screenSpaceEnabled && !m_ssInitialized) {
            if (!InitializeScreenSpace()) {
                OutputDebugStringA("SPHFluid: SS init FAILED! Disabling.\n");
                m_screenSpaceEnabled = false;
            }
        }

        if (m_screenSpaceEnabled && m_ssInitialized && m_pDepthVS && m_depthRT) {
            static int ssLog = 0;
            if (ssLog < 5) {
                char buf[512];
                sprintf_s(buf, "SPHFluid::DrawSS cnt=%u p0=(%.1f,%.1f,%.1f) cam=(%.1f,%.1f,%.1f)->at(%.1f,%.1f,%.1f) fov=%.2f\n",
                    m_particleCount,
                    m_particles[0].position.x, m_particles[0].position.y, m_particles[0].position.z,
                    m_camPos.x, m_camPos.y, m_camPos.z,
                    m_camAt.x, m_camAt.y, m_camAt.z, m_fov);
                OutputDebugStringA(buf);
                ssLog++;
            }
            DrawScreenSpace(ctx);
        } else {
            DrawParticles(ctx);
        }
    }

    // ============================================================
    void SPHFluid::DrawScreenSpace(ID3D11DeviceContext* ctx) {
        // ===== Save ALL state =====
        ComPtr<ID3D11RenderTargetView> prevRTV;
        ComPtr<ID3D11DepthStencilView> prevDSV;
        ctx->OMGetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.GetAddressOf());

        ComPtr<ID3D11BlendState> prevBlend; FLOAT prevBF[4]; UINT prevSM;
        ctx->OMGetBlendState(prevBlend.GetAddressOf(), prevBF, &prevSM);

        ComPtr<ID3D11DepthStencilState> prevDS; UINT prevSR;
        ctx->OMGetDepthStencilState(prevDS.GetAddressOf(), &prevSR);

        ComPtr<ID3D11RasterizerState> prevRS;
        ctx->RSGetState(prevRS.GetAddressOf());

        D3D11_VIEWPORT prevVP; UINT numVP = 1;
        ctx->RSGetViewports(&numVP, &prevVP);

        ComPtr<ID3D11VertexShader> prevVS;
        ComPtr<ID3D11PixelShader>  prevPS;
        ComPtr<ID3D11InputLayout>  prevIL;
        D3D11_PRIMITIVE_TOPOLOGY   prevTopo;
        ctx->VSGetShader(prevVS.GetAddressOf(), nullptr, nullptr);
        ctx->PSGetShader(prevPS.GetAddressOf(), nullptr, nullptr);
        ctx->IAGetInputLayout(prevIL.GetAddressOf());
        ctx->IAGetPrimitiveTopology(&prevTopo);

        ComPtr<ID3D11Buffer> prevVSCB0, prevVSCB1, prevPSCB0;
        ctx->VSGetConstantBuffers(0, 1, prevVSCB0.GetAddressOf());
        ctx->VSGetConstantBuffers(1, 1, prevVSCB1.GetAddressOf());
        ctx->PSGetConstantBuffers(0, 1, prevPSCB0.GetAddressOf());

        ComPtr<ID3D11SamplerState> prevSamp0, prevSamp1;
        ctx->PSGetSamplers(0, 1, prevSamp0.GetAddressOf());
        ctx->PSGetSamplers(1, 1, prevSamp1.GetAddressOf());

        ComPtr<ID3D11ShaderResourceView> prevSRV0, prevSRV1;
        ctx->PSGetShaderResources(0, 1, prevSRV0.GetAddressOf());
        ctx->PSGetShaderResources(1, 1, prevSRV1.GetAddressOf());

        ComPtr<ID3D11Buffer> prevVB0, prevVB1, prevIB;
        UINT prevStride0, prevStride1, prevOff0, prevOff1;
        DXGI_FORMAT prevIBFmt; UINT prevIBOff;
        ctx->IAGetVertexBuffers(0, 1, prevVB0.GetAddressOf(), &prevStride0, &prevOff0);
        ctx->IAGetVertexBuffers(1, 1, prevVB1.GetAddressOf(), &prevStride1, &prevOff1);
        ctx->IAGetIndexBuffer(prevIB.GetAddressOf(), &prevIBFmt, &prevIBOff);

        // ===== Matrices =====
        XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&m_camPos), XMLoadFloat3(&m_camAt), XMLoadFloat3(&m_camUp));
        XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearZ, m_farZ);

        UpdateInstanceBuffer(ctx);

        D3D11_VIEWPORT fullVP = {};
        fullVP.Width = (float)m_screenWidth;
        fullVP.Height = (float)m_screenHeight;
        fullVP.MaxDepth = 1.0f;

        D3D11_VIEWPORT halfVP = {};
        halfVP.Width = (float)(m_screenWidth / 2);
        halfVP.Height = (float)(m_screenHeight / 2);
        halfVP.MaxDepth = 1.0f;

        ctx->RSSetState(m_pNoCullRS.Get());
        ctx->OMSetDepthStencilState(m_pDepthDisabledState.Get(), 0);

        ID3D11ShaderResourceView* nullSRV = nullptr;
        ID3D11RenderTargetView* nullRTV = nullptr;

        // ========== PASS 1: Depth ==========
        {
            float clr[4] = { 0, 0, 0, 0 };
            ctx->ClearRenderTargetView(m_depthRT->GetRTV(), clr);
            ID3D11RenderTargetView* rtv = m_depthRT->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);
            ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
            ctx->RSSetViewports(1, &fullVP);

            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pCameraCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBCamera* cb = (CBCamera*)mp.pData;
                XMStoreFloat4x4(&cb->View, XMMatrixTranspose(view));
                XMStoreFloat4x4(&cb->Proj, XMMatrixTranspose(proj));
                cb->PointRadius = m_particleScale * 4.0f;
                cb->ScreenSize = { (float)m_screenWidth, (float)m_screenHeight };
                cb->_pad0 = m_farZ;
                ctx->Unmap(m_pCameraCB.Get(), 0);
            }
            ctx->VSSetConstantBuffers(0, 1, m_pCameraCB.GetAddressOf());
            ctx->PSSetConstantBuffers(0, 1, m_pCameraCB.GetAddressOf());

            UINT strides[2] = { sizeof(XMFLOAT2), sizeof(XMFLOAT3) };
            UINT offsets[2] = { 0, 0 };
            ID3D11Buffer* vbs[2] = { m_pBillboardVB.Get(), m_pInstanceBuffer.Get() };
            ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
            ctx->IASetIndexBuffer(m_pBillboardIB.Get(), DXGI_FORMAT_R32_UINT, 0);
            ctx->IASetInputLayout(m_pBillboardLayout.Get());
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(m_pDepthVS.Get(), nullptr, 0);
            ctx->PSSetShader(m_pDepthPS.Get(), nullptr, 0);
            ctx->DrawIndexedInstanced(6, m_particleCount, 0, 0, 0);

            // ★重要: RTVをアンバインド
            ID3D11RenderTargetView* nullRTV = nullptr;
            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
        }

        // ========== PASS 2: Blur Horizontal ==========
        {
            ctx->PSSetShaderResources(0, 1, &nullSRV);

            float clr[4] = { 0, 0, 0, 0 };
            ctx->ClearRenderTargetView(m_blurRT1->GetRTV(), clr);

            ID3D11RenderTargetView* rtv = m_blurRT1->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);
            ctx->RSSetViewports(1, &fullVP);

            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pBlurCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBBlur* cb = (CBBlur*)mp.pData;
                cb->BlurDir = { 1.0f / m_screenWidth, 0.0f };
                cb->BlurScale = 0.8f;
                cb->BlurDepthFalloff = 1000.0f;
                cb->FilterRadius = 5;
                cb->_pad1 = { 0, 0, 0 };
                ctx->Unmap(m_pBlurCB.Get(), 0);
            }
            ctx->PSSetConstantBuffers(0, 1, m_pBlurCB.GetAddressOf());

            ID3D11ShaderResourceView* srv = m_depthRT->GetSRV();
            ctx->PSSetShaderResources(0, 1, &srv);
            ctx->PSSetSamplers(0, 1, m_pPointSampler.GetAddressOf());

            ctx->IASetInputLayout(nullptr);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(m_pQuadVS.Get(), nullptr, 0);
            ctx->PSSetShader(m_pBilateralBlurPS.Get(), nullptr, 0);
            ctx->Draw(3, 0);

            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
            ctx->PSSetShaderResources(0, 1, &nullSRV);
        }

        // ========== PASS 3: Blur Vertical ==========
        {
            float clr[4] = { 0, 0, 0, 0 };
            ctx->ClearRenderTargetView(m_blurRT2->GetRTV(), clr);

            ID3D11RenderTargetView* rtv = m_blurRT2->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);

            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pBlurCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBBlur* cb = (CBBlur*)mp.pData;
                cb->BlurDir = { 0.0f, 1.0f / m_screenHeight };
                cb->BlurScale = 0.8f;
                cb->BlurDepthFalloff = 1000.0f;
                cb->FilterRadius = 5;
                cb->_pad1 = { 0, 0, 0 };
                ctx->Unmap(m_pBlurCB.Get(), 0);
            }

            ID3D11ShaderResourceView* srv = m_blurRT1->GetSRV();
            ctx->PSSetShaderResources(0, 1, &srv);

            ctx->Draw(3, 0);

            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
            ctx->PSSetShaderResources(0, 1, &nullSRV);
        }

        // ========== PASS 4: Thickness ==========
        {
            float clr[4] = { 0, 0, 0, 0 };
            ctx->ClearRenderTargetView(m_thicknessRT->GetRTV(), clr);

            ID3D11RenderTargetView* rtv = m_thicknessRT->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);
            ctx->OMSetBlendState(m_pAdditiveBlendState.Get(), nullptr, 0xFFFFFFFF);
            ctx->RSSetViewports(1, &halfVP);

            ctx->VSSetConstantBuffers(0, 1, m_pCameraCB.GetAddressOf());

            UINT strides[2] = { sizeof(XMFLOAT2), sizeof(XMFLOAT3) };
            UINT offsets[2] = { 0, 0 };
            ID3D11Buffer* vbs[2] = { m_pBillboardVB.Get(), m_pInstanceBuffer.Get() };
            ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
            ctx->IASetIndexBuffer(m_pBillboardIB.Get(), DXGI_FORMAT_R32_UINT, 0);
            ctx->IASetInputLayout(m_pBillboardLayout.Get());
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(m_pDepthVS.Get(), nullptr, 0);
            ctx->PSSetShader(m_pThicknessPS.Get(), nullptr, 0);
            ctx->DrawIndexedInstanced(6, m_particleCount, 0, 0, 0);

            ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

            ID3D11RenderTargetView* nullRTV = nullptr;
            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
        }

        // ========== PASS 5: Final ==========
        {
            ID3D11RenderTargetView* rtv = prevRTV.Get();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);
            ctx->OMSetBlendState(m_pAlphaBlendState.Get(), nullptr, 0xFFFFFFFF);
            ctx->OMSetDepthStencilState(m_pDepthDisabledState.Get(), 0);
            ctx->RSSetViewports(1, &fullVP);

            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pFinalCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBFinal* cb = (CBFinal*)mp.pData;
                XMStoreFloat4x4(&cb->InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, proj)));
                cb->TexelSize = { 1.0f / m_screenWidth, 1.0f / m_screenHeight };
                cb->WaterAlpha = 0.8f;
                cb->FresnelPower = 4.0f;
                cb->WaterColor = { m_particleColor.x, m_particleColor.y, m_particleColor.z };
                cb->_pad2 = m_nearZ;
                cb->WaterDeepColor = { 0.05f, 0.2f, 0.5f };
                cb->_pad3 = m_farZ;
                cb->LightDir = { 0.3f, 1.0f, 0.5f };
                cb->SpecPower = 64.0f;
                cb->AbsorptionCoeff = { 0.4f, 0.08f, 0.04f };
                cb->RefractScale = 0.02f;
                ctx->Unmap(m_pFinalCB.Get(), 0);
            }
            ctx->PSSetConstantBuffers(0, 1, m_pFinalCB.GetAddressOf());

            // ★ブラー後の深度を使用
            ID3D11ShaderResourceView* srvs[2] = { m_blurRT2->GetSRV(), m_thicknessRT->GetSRV() };
            ctx->PSSetShaderResources(0, 2, srvs);
            ID3D11SamplerState* samps[2] = { m_pPointSampler.Get(), m_pLinearSampler.Get() };
            ctx->PSSetSamplers(0, 2, samps);

            ctx->IASetInputLayout(nullptr);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(m_pQuadVS.Get(), nullptr, 0);
            ctx->PSSetShader(m_pFinalPS.Get(), nullptr, 0);
            ctx->Draw(3, 0);

            // クリーンアップ
            ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
            ctx->PSSetShaderResources(0, 2, nullSRVs);
        }

        // ===== Restore ALL state =====
        ctx->OMSetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.Get());
        ctx->OMSetBlendState(prevBlend.Get(), prevBF, prevSM);
        ctx->OMSetDepthStencilState(prevDS.Get(), prevSR);
        ctx->RSSetState(prevRS.Get());
        ctx->RSSetViewports(1, &prevVP);
        ctx->IASetInputLayout(prevIL.Get());
        ctx->IASetPrimitiveTopology(prevTopo);
        ctx->VSSetShader(prevVS.Get(), nullptr, 0);
        ctx->PSSetShader(prevPS.Get(), nullptr, 0);

        ID3D11Buffer* vb0 = prevVB0.Get();
        ctx->IASetVertexBuffers(0, 1, &vb0, &prevStride0, &prevOff0);
        ID3D11Buffer* vb1 = prevVB1.Get();
        ctx->IASetVertexBuffers(1, 1, &vb1, &prevStride1, &prevOff1);
        ctx->IASetIndexBuffer(prevIB.Get(), prevIBFmt, prevIBOff);

        ctx->VSSetConstantBuffers(0, 1, prevVSCB0.GetAddressOf());
        ctx->VSSetConstantBuffers(1, 1, prevVSCB1.GetAddressOf());
        ctx->PSSetConstantBuffers(0, 1, prevPSCB0.GetAddressOf());
        ctx->PSSetSamplers(0, 1, prevSamp0.GetAddressOf());
        ctx->PSSetSamplers(1, 1, prevSamp1.GetAddressOf());
        ID3D11ShaderResourceView* rSRV[2] = { prevSRV0.Get(), prevSRV1.Get() };
        ctx->PSSetShaderResources(0, 2, rSRV);

        static int frameCount = 0;
        frameCount++;
        if (frameCount % 300 == 0 && m_particleCount > 0) {
            char buf[256];
            sprintf_s(buf, "SPHFluid: Frame %d, particles=%u, scale=%.3f, radius=%.3f\n",
                frameCount, m_particleCount, m_particleScale, m_particleScale * 2.5f);
            OutputDebugStringA(buf);
        }
    }


    // ============================================================
    void SPHFluid::DrawParticles(ID3D11DeviceContext* ctx) {
        (void)ctx;
        static int log = 0;
        if (log++ < 3) OutputDebugStringA("SPHFluid::DrawParticles fallback\n");
    }

} // namespace Engine
