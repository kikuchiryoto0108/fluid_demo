//==============================================================================
//  File   : sph_fluid.cpp
//  Brief  : SPH流体シミュレーション + スクリーンスペースレンダリング
//==============================================================================
#include "pch.h"
#include "sph_fluid.h"
#include "Engine/Core/renderer.h"
#include "Engine/Graphics/mesh.h"
#include "Engine/Graphics/mesh_factory.h"
#include "Engine/Graphics/primitive.h"
#include "Game/Objects/camera.h" 
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

#pragma comment(lib, "d3dcompiler.lib")

namespace Engine {

    constexpr float PI = 3.14159265358979323846f;

    //==========================================================
    // 定数バッファ構造体
    //==========================================================
    struct CBFluid {
        XMFLOAT4X4 View;
        XMFLOAT4X4 Projection;
        float PointRadius;
        float Padding[3];
    };

    struct CBBlur {
        XMFLOAT2 TexelSize;
        float BlurScale;
        float BlurDepthFalloff;
    };

    struct CBFinal {
        XMFLOAT4X4 InvProjection;
        XMFLOAT2 TexelSize;
        XMFLOAT2 Padding;
        XMFLOAT3 WaterColor;
        float WaterAlpha;
        XMFLOAT3 LightDir;
        float FresnelPower;
    };

    //==========================================================
    // コンストラクタ
    //==========================================================
    SPHFluid::SPHFluid() {
        m_params.smoothingRadius = 0.5f;
        m_params.restDensity = 1000.0f;
        m_params.gasConstant = 2000.0f;
        m_params.viscosity = 200.0f;
        m_params.gravity = XMFLOAT3(0.0f, -9.81f, 0.0f);
        m_params.deltaTime = 1.0f / 60.0f;
        m_params.boundaryMin = XMFLOAT3(-10.0f, -22.0f, -10.0f);
        m_params.boundaryMax = XMFLOAT3(10.0f, 15.0f, 10.0f);
        m_params.particleCount = 0;

        srand(static_cast<unsigned>(time(nullptr)));
    }

    //==========================================================
    // デストラクタ
    //==========================================================
    SPHFluid::~SPHFluid() {
        Finalize();
    }

    //==========================================================
    // ブレンドステート作成
    //==========================================================
    bool SPHFluid::CreateBlendStates(ID3D11Device* device) {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        HRESULT hr = device->CreateBlendState(&blendDesc, m_pAlphaBlendState.GetAddressOf());
        if (FAILED(hr)) {
            OutputDebugStringA("SPHFluid: ブレンドステート作成失敗\n");
            return false;
        }
        return true;
    }

    //==========================================================
    // シェーダー初期化
    //==========================================================
    bool SPHFluid::InitializeShaders(ID3D11Device* device) {
        HRESULT hr;
        ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

        //----------------------------------------------------------
        // 深度シェーダー（シンプル版 - 球メッシュ用）
        //----------------------------------------------------------
        const char* depthShader = R"(
    cbuffer CBMatrix : register(b0) {
        float4x4 WorldViewProjection;
    };
    cbuffer CBFluid : register(b2) {
        float4x4 View;
        float4x4 Projection;
        float PointRadius;
        float3 Padding;
    };
    struct VS_INPUT {
        float3 Position : POSITION;
        float3 Normal   : NORMAL;
        float4 Color    : COLOR;
        float2 TexCoord : TEXCOORD;
    };
    struct VS_OUTPUT {
        float4 Position : SV_Position;
        float Depth     : TEXCOORD0;
    };
    VS_OUTPUT VS_Depth(VS_INPUT input) {
        VS_OUTPUT output;
        output.Position = mul(float4(input.Position, 1.0), WorldViewProjection);
        // NDC深度をそのまま使う
        output.Depth = output.Position.z / output.Position.w;
        return output;
    }
    float4 PS_Depth(VS_OUTPUT input) : SV_Target {
        // 0〜1の深度値を出力
        return float4(input.Depth, input.Depth, input.Depth, 1.0);
    }
)";

        hr = D3DCompile(depthShader, strlen(depthShader), nullptr, nullptr, nullptr,
            "VS_Depth", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            return false;
        }
        device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_pDepthVS);

        hr = D3DCompile(depthShader, strlen(depthShader), nullptr, nullptr, nullptr,
            "PS_Depth", "ps_4_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            return false;
        }
        device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pDepthPS);

        //----------------------------------------------------------
        // ブラーシェーダー（適度な強さ）
        //----------------------------------------------------------
        const char* blurShader = R"(
    Texture2D DepthTexture : register(t0);
    SamplerState PointSampler : register(s0);
    
    cbuffer CBBlur : register(b0) {
        float2 TexelSize;
        float BlurScale;
        float BlurDepthFalloff;
    };
    
    struct VS_OUTPUT {
        float4 Position : SV_Position;
        float2 TexCoord : TEXCOORD;
    };
    
    VS_OUTPUT VS_Blur(float3 pos : POSITION, float2 uv : TEXCOORD) {
        VS_OUTPUT output;
        output.Position = float4(pos, 1.0);
        output.TexCoord = uv;
        return output;
    }
    
    float4 PS_BlurH(VS_OUTPUT input) : SV_Target {
        float centerDepth = DepthTexture.Sample(PointSampler, input.TexCoord).r;
        
        // 中心に深度がなければスキップ
        if (centerDepth <= 0.001) return float4(0, 0, 0, 0);
        
        float sum = centerDepth;
        float wsum = 1.0;
        
        for (int x = -7; x <= 7; x++) {
            if (x == 0) continue;
            
            float2 offset = float2(x * TexelSize.x * BlurScale, 0);
            float2 sampleUV = input.TexCoord + offset;
            
            // UV範囲チェック
            if (sampleUV.x < 0.0 || sampleUV.x > 1.0) continue;
            
            float sampleDepth = DepthTexture.Sample(PointSampler, sampleUV).r;
            
            // 深度がある場所のみブラー
            if (sampleDepth <= 0.001) continue;
            
            // 深度差が大きすぎる場合は無視（エッジ保持）
            float depthDiff = abs(centerDepth - sampleDepth);
            if (depthDiff > 0.1) continue;
            
            float w = exp(-float(x * x) / 25.0);
            sum += sampleDepth * w;
            wsum += w;
        }
        
        float result = sum / wsum;
        return float4(result, result, result, 1);
    }
    
    float4 PS_BlurV(VS_OUTPUT input) : SV_Target {
        float centerDepth = DepthTexture.Sample(PointSampler, input.TexCoord).r;
        
        if (centerDepth <= 0.001) return float4(0, 0, 0, 0);
        
        float sum = centerDepth;
        float wsum = 1.0;
        
        for (int y = -7; y <= 7; y++) {
            if (y == 0) continue;
            
            float2 offset = float2(0, y * TexelSize.y * BlurScale);
            float2 sampleUV = input.TexCoord + offset;
            
            if (sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;
            
            float sampleDepth = DepthTexture.Sample(PointSampler, sampleUV).r;
            
            if (sampleDepth <= 0.001) continue;
            
            float depthDiff = abs(centerDepth - sampleDepth);
            if (depthDiff > 0.1) continue;
            
            float w = exp(-float(y * y) / 25.0);
            sum += sampleDepth * w;
            wsum += w;
        }
        
        float result = sum / wsum;
        return float4(result, result, result, 1);
    }
)";


        D3D11_INPUT_ELEMENT_DESC quadLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = D3DCompile(blurShader, strlen(blurShader), nullptr, nullptr, nullptr,
            "VS_Blur", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
        if (FAILED(hr)) return false;
        device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_pBlurVS);
        device->CreateInputLayout(quadLayout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_pQuadInputLayout);

        hr = D3DCompile(blurShader, strlen(blurShader), nullptr, nullptr, nullptr,
            "PS_BlurH", "ps_4_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr)) return false;
        device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pBlurHPS);

        hr = D3DCompile(blurShader, strlen(blurShader), nullptr, nullptr, nullptr,
            "PS_BlurV", "ps_4_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr)) return false;
        device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pBlurVPS);

        //----------------------------------------------------------
        // 最終合成シェーダー（水表現強化版）
        //----------------------------------------------------------
        const char* finalShader = R"(
    Texture2D DepthTexture : register(t0);
    Texture2D SceneTexture : register(t1);
    SamplerState LinearSampler : register(s0);
    
    cbuffer CBFinal : register(b0) {
        float4x4 InvProjection;
        float2 TexelSize;
        float2 Padding;
        float3 WaterColor;
        float WaterAlpha;
        float3 LightDir;
        float FresnelPower;
    };
    
    struct VS_OUTPUT {
        float4 Position : SV_Position;
        float2 TexCoord : TEXCOORD;
    };
    
    VS_OUTPUT VS_Final(float3 pos : POSITION, float2 uv : TEXCOORD) {
        VS_OUTPUT output;
        output.Position = float4(pos, 1.0);
        output.TexCoord = uv;
        return output;
    }
    
    float3 ComputeNormal(float2 uv) {
        float depthL = DepthTexture.Sample(LinearSampler, uv - float2(TexelSize.x, 0)).r;
        float depthR = DepthTexture.Sample(LinearSampler, uv + float2(TexelSize.x, 0)).r;
        float depthT = DepthTexture.Sample(LinearSampler, uv - float2(0, TexelSize.y)).r;
        float depthB = DepthTexture.Sample(LinearSampler, uv + float2(0, TexelSize.y)).r;
        
        float3 normal;
        normal.x = (depthL - depthR) * 2.0;
        normal.y = (depthT - depthB) * 2.0;
        normal.z = 0.05;  // 小さくすると法線が急になる
        return normalize(normal);
    }
    
    float4 PS_Final(VS_OUTPUT input) : SV_Target {
        float depth = DepthTexture.Sample(LinearSampler, input.TexCoord).r;
        
        // 深度がない場所は透明
        if (depth <= 0.001 || depth >= 0.999) {
            discard;
        }
        
        // 法線計算
        float3 normal = ComputeNormal(input.TexCoord);
        
        // 視線方向（画面に向かって）
        float3 viewDir = float3(0, 0, 1);
        
        // フレネル効果（縁が明るく）
        float fresnel = pow(1.0 - saturate(dot(normal, viewDir)), FresnelPower);
        
        // ライティング
        float3 lightDir = normalize(LightDir);
        float diffuse = max(dot(normal, lightDir), 0.0) * 0.6 + 0.4;
        
        // スペキュラ（ハイライト）
        float3 halfVec = normalize(lightDir + viewDir);
        float specular = pow(max(dot(normal, halfVec), 0.0), 128.0);
        
        // 水の基本色
        float3 waterBase = WaterColor * diffuse;
        
        // 反射色（空の色っぽく）
        float3 reflectColor = float3(0.6, 0.8, 1.0);
        
        // 最終色：水色 + フレネルで反射 + スペキュラ
        float3 finalColor = lerp(waterBase, reflectColor, fresnel * 0.5);
        finalColor += specular * float3(1.0, 1.0, 1.0) * 0.8;
        
        // エッジを少しソフトに
        float edgeSoft = smoothstep(0.001, 0.05, depth) * smoothstep(0.999, 0.95, depth);
        
        return float4(finalColor, WaterAlpha * edgeSoft);
    }
)";


        hr = D3DCompile(finalShader, strlen(finalShader), nullptr, nullptr, nullptr,
            "VS_Final", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
        if (FAILED(hr)) return false;
        device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_pFinalVS);

        hr = D3DCompile(finalShader, strlen(finalShader), nullptr, nullptr, nullptr,
            "PS_Final", "ps_4_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr)) return false;
        device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pFinalPS);

        //----------------------------------------------------------
        // 定数バッファ作成
        //----------------------------------------------------------
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        cbDesc.ByteWidth = sizeof(CBFluid);
        device->CreateBuffer(&cbDesc, nullptr, &m_pFluidCB);

        cbDesc.ByteWidth = sizeof(CBBlur);
        device->CreateBuffer(&cbDesc, nullptr, &m_pBlurCB);

        cbDesc.ByteWidth = sizeof(CBFinal);
        device->CreateBuffer(&cbDesc, nullptr, &m_pFinalCB);

        //----------------------------------------------------------
        // サンプラー作成
        //----------------------------------------------------------
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        device->CreateSamplerState(&sampDesc, &m_pPointSampler);

        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        device->CreateSamplerState(&sampDesc, &m_pLinearSampler);

        OutputDebugStringA("SPHFluid: シェーダー初期化成功\n");
        return true;
    }

    //==========================================================
    // 初期化
    //==========================================================
    bool SPHFluid::Initialize(ID3D11Device* device, uint32_t maxParticleCount) {
        if (!device || maxParticleCount == 0) return false;

        m_pDevice = device;
        m_maxParticles = maxParticleCount;
        m_particleCount = 0;

        m_particles.reserve(m_maxParticles);
        m_neighbors.resize(m_maxParticles);

        // ブレンドステート作成
        if (!CreateBlendStates(device)) {
            OutputDebugStringA("SPHFluid: ブレンドステート作成失敗\n");
            return false;
        }

        // 球メッシュ読み込み
        m_sphereMesh = MeshFactory::CreateFromFile("resource/model/sphere.fbx");
        if (!m_sphereMesh) {
            OutputDebugStringA("SPHFluid: sphere.fbx 読み込み失敗、プリミティブ使用\n");
            m_sphereMesh = MeshFactory::CreateSphere(0.5f, 12, 8);
        }
        if (!m_sphereMesh) {
            OutputDebugStringA("SPHFluid: メッシュ作成失敗\n");
            return false;
        }

        if (!m_sphereMesh->IsUploaded()) {
            if (!m_sphereMesh->Upload(device)) {
                OutputDebugStringA("SPHFluid: メッシュアップロード失敗\n");
                return false;
            }
        }

        // スクリーンスペース用リソース（オプション）
        auto& renderer = Renderer::GetInstance();
        uint32_t width = renderer.GetScreenWidth();
        uint32_t height = renderer.GetScreenHeight();

        m_depthRT = std::make_unique<RenderTarget>();
        m_blurRT1 = std::make_unique<RenderTarget>();
        m_blurRT2 = std::make_unique<RenderTarget>();
        m_fullscreenQuad = std::make_unique<FullscreenQuad>();

        bool ssSuccess = true;
        if (!m_depthRT->Create(device, width, height, DXGI_FORMAT_R32_FLOAT, true)) ssSuccess = false;
        if (!m_blurRT1->Create(device, width, height, DXGI_FORMAT_R32_FLOAT, false)) ssSuccess = false;
        if (!m_blurRT2->Create(device, width, height, DXGI_FORMAT_R32_FLOAT, false)) ssSuccess = false;
        if (!m_fullscreenQuad->Initialize(device)) ssSuccess = false;
        if (!InitializeShaders(device)) ssSuccess = false;

        if (!ssSuccess) {
            OutputDebugStringA("SPHFluid: スクリーンスペース初期化失敗、通常描画モード\n");
            m_screenSpaceEnabled = false;
        }

        m_initialized = true;
        OutputDebugStringA("SPHFluid: 初期化成功\n");
        return true;
    }

    //==========================================================
    // 終了処理
    //==========================================================
    void SPHFluid::Finalize() {
        m_sphereMesh.reset();
        m_depthRT.reset();
        m_blurRT1.reset();
        m_blurRT2.reset();
        m_fullscreenQuad.reset();

        m_pAlphaBlendState.Reset();
        m_pDepthVS.Reset();
        m_pDepthPS.Reset();
        m_pBlurVS.Reset();
        m_pBlurHPS.Reset();
        m_pBlurVPS.Reset();
        m_pFinalVS.Reset();
        m_pFinalPS.Reset();
        m_pQuadInputLayout.Reset();
        m_pFluidCB.Reset();
        m_pBlurCB.Reset();
        m_pFinalCB.Reset();
        m_pPointSampler.Reset();
        m_pLinearSampler.Reset();

        m_particles.clear();
        m_neighbors.clear();
        m_particleCount = 0;
        m_initialized = false;
    }

    //==========================================================
    // パーティクル生成
    //==========================================================
    void SPHFluid::SpawnParticles(const XMFLOAT3& center, uint32_t count, float radius) {
        uint32_t spawnCount = std::min(count, m_maxParticles - m_particleCount);

        for (uint32_t i = 0; i < spawnCount; ++i) {
            SPHParticle p;

            float theta = static_cast<float>(rand()) / RAND_MAX * 2.0f * PI;
            float phi = static_cast<float>(rand()) / RAND_MAX * PI;
            float r = static_cast<float>(rand()) / RAND_MAX * radius;

            p.position.x = center.x + r * sinf(phi) * cosf(theta);
            p.position.y = center.y + r * sinf(phi) * sinf(theta);
            p.position.z = center.z + r * cosf(phi);

            p.velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
            p.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
            p.density = m_params.restDensity;
            p.pressure = 0.0f;
            p.mass = 1.0f;

            m_particles.push_back(p);
            m_particleCount++;
        }
        m_params.particleCount = m_particleCount;

        char buf[128];
        sprintf_s(buf, "SPHFluid: %u粒子生成（合計: %u）\n", spawnCount, m_particleCount);
        OutputDebugStringA(buf);
    }

    void SPHFluid::Clear() {
        m_particles.clear();
        m_particleCount = 0;
        m_params.particleCount = 0;
    }

    void SPHFluid::SetBoundary(const XMFLOAT3& min, const XMFLOAT3& max) {
        m_params.boundaryMin = min;
        m_params.boundaryMax = max;
    }

    void SPHFluid::SetGravity(const XMFLOAT3& gravity) {
        m_params.gravity = gravity;
    }

    //==========================================================
    // カーネル関数
    //==========================================================
    float SPHFluid::Poly6Kernel(float rSquared, float h) {
        float h2 = h * h;
        if (rSquared > h2) return 0.0f;
        float diff = h2 - rSquared;
        float h9 = h2 * h2 * h2 * h2 * h;
        return 315.0f / (64.0f * PI * h9) * diff * diff * diff;
    }

    XMFLOAT3 SPHFluid::SpikyKernelGradient(const XMFLOAT3& r, float dist, float h) {
        if (dist > h || dist < 0.0001f) return XMFLOAT3(0, 0, 0);
        float h6 = h * h * h * h * h * h;
        float coeff = -45.0f / (PI * h6) * (h - dist) * (h - dist) / dist;
        return XMFLOAT3(r.x * coeff, r.y * coeff, r.z * coeff);
    }

    float SPHFluid::ViscosityKernelLaplacian(float r, float h) {
        if (r > h) return 0.0f;
        float h6 = h * h * h * h * h * h;
        return 45.0f / (PI * h6) * (h - r);
    }

    //==========================================================
    // シミュレーション
    //==========================================================
    void SPHFluid::FindNeighbors() {
        float h2 = m_params.smoothingRadius * m_params.smoothingRadius;
        for (uint32_t i = 0; i < m_particleCount; ++i) {
            m_neighbors[i].clear();
            for (uint32_t j = 0; j < m_particleCount; ++j) {
                if (i == j) continue;
                float dx = m_particles[i].position.x - m_particles[j].position.x;
                float dy = m_particles[i].position.y - m_particles[j].position.y;
                float dz = m_particles[i].position.z - m_particles[j].position.z;
                if (dx * dx + dy * dy + dz * dz < h2) {
                    m_neighbors[i].push_back(j);
                }
            }
        }
    }

    void SPHFluid::ComputeDensityPressure() {
        float h = m_params.smoothingRadius;
        for (uint32_t i = 0; i < m_particleCount; ++i) {
            m_particles[i].density = m_particles[i].mass * Poly6Kernel(0.0f, h);
            for (uint32_t j : m_neighbors[i]) {
                float dx = m_particles[i].position.x - m_particles[j].position.x;
                float dy = m_particles[i].position.y - m_particles[j].position.y;
                float dz = m_particles[i].position.z - m_particles[j].position.z;
                m_particles[i].density += m_particles[j].mass * Poly6Kernel(dx * dx + dy * dy + dz * dz, h);
            }
            m_particles[i].pressure = std::max(0.0f,
                m_params.gasConstant * (m_particles[i].density - m_params.restDensity));
        }
    }

    void SPHFluid::ComputeForces() {
        float h = m_params.smoothingRadius;
        for (uint32_t i = 0; i < m_particleCount; ++i) {
            XMFLOAT3 pressureForce = { 0, 0, 0 };
            XMFLOAT3 viscosityForce = { 0, 0, 0 };

            for (uint32_t j : m_neighbors[i]) {
                float dx = m_particles[i].position.x - m_particles[j].position.x;
                float dy = m_particles[i].position.y - m_particles[j].position.y;
                float dz = m_particles[i].position.z - m_particles[j].position.z;
                float dist = sqrtf(dx * dx + dy * dy + dz * dz);
                if (dist < 0.0001f) continue;

                XMFLOAT3 r = { dx, dy, dz };
                XMFLOAT3 grad = SpikyKernelGradient(r, dist, h);
                float pTerm = (m_particles[i].pressure + m_particles[j].pressure)
                    / (2.0f * std::max(m_particles[j].density, 0.0001f));

                pressureForce.x -= m_particles[j].mass * pTerm * grad.x;
                pressureForce.y -= m_particles[j].mass * pTerm * grad.y;
                pressureForce.z -= m_particles[j].mass * pTerm * grad.z;

                float lap = ViscosityKernelLaplacian(dist, h);
                float vTerm = m_params.viscosity * m_particles[j].mass * lap
                    / std::max(m_particles[j].density, 0.0001f);

                viscosityForce.x += vTerm * (m_particles[j].velocity.x - m_particles[i].velocity.x);
                viscosityForce.y += vTerm * (m_particles[j].velocity.y - m_particles[i].velocity.y);
                viscosityForce.z += vTerm * (m_particles[j].velocity.z - m_particles[i].velocity.z);
            }

            float invD = 1.0f / std::max(m_particles[i].density, 0.0001f);
            m_particles[i].acceleration.x = (pressureForce.x + viscosityForce.x) * invD + m_params.gravity.x;
            m_particles[i].acceleration.y = (pressureForce.y + viscosityForce.y) * invD + m_params.gravity.y;
            m_particles[i].acceleration.z = (pressureForce.z + viscosityForce.z) * invD + m_params.gravity.z;
        }
    }

    void SPHFluid::Integrate(float dt) {
        const float maxSpeed = 15.0f;
        for (uint32_t i = 0; i < m_particleCount; ++i) {
            m_particles[i].velocity.x += m_particles[i].acceleration.x * dt;
            m_particles[i].velocity.y += m_particles[i].acceleration.y * dt;
            m_particles[i].velocity.z += m_particles[i].acceleration.z * dt;

            float speed = sqrtf(
                m_particles[i].velocity.x * m_particles[i].velocity.x +
                m_particles[i].velocity.y * m_particles[i].velocity.y +
                m_particles[i].velocity.z * m_particles[i].velocity.z);
            if (speed > maxSpeed) {
                float s = maxSpeed / speed;
                m_particles[i].velocity.x *= s;
                m_particles[i].velocity.y *= s;
                m_particles[i].velocity.z *= s;
            }

            m_particles[i].position.x += m_particles[i].velocity.x * dt;
            m_particles[i].position.y += m_particles[i].velocity.y * dt;
            m_particles[i].position.z += m_particles[i].velocity.z * dt;
        }
    }

    void SPHFluid::HandleBoundaries() {
        const float damping = 0.3f;
        const float eps = 0.01f;
        for (uint32_t i = 0; i < m_particleCount; ++i) {
            SPHParticle& p = m_particles[i];
            if (p.position.x < m_params.boundaryMin.x + eps) { p.position.x = m_params.boundaryMin.x + eps; p.velocity.x *= -damping; }
            if (p.position.x > m_params.boundaryMax.x - eps) { p.position.x = m_params.boundaryMax.x - eps; p.velocity.x *= -damping; }
            if (p.position.y < m_params.boundaryMin.y + eps) { p.position.y = m_params.boundaryMin.y + eps; p.velocity.y *= -damping; }
            if (p.position.y > m_params.boundaryMax.y - eps) { p.position.y = m_params.boundaryMax.y - eps; p.velocity.y *= -damping; }
            if (p.position.z < m_params.boundaryMin.z + eps) { p.position.z = m_params.boundaryMin.z + eps; p.velocity.z *= -damping; }
            if (p.position.z > m_params.boundaryMax.z - eps) { p.position.z = m_params.boundaryMax.z - eps; p.velocity.z *= -damping; }
        }
    }

    void SPHFluid::SimulateCPU(float dt) {
        if (m_particleCount == 0) return;
        FindNeighbors();
        ComputeDensityPressure();
        ComputeForces();
        Integrate(dt);
        HandleBoundaries();
    }

    void SPHFluid::Update(ID3D11DeviceContext* context, float deltaTime) {
        if (!m_initialized) return;
        const float fixedDt = 1.0f / 120.0f;
        static float acc = 0.0f;
        acc += deltaTime;
        int steps = 0;
        while (acc >= fixedDt && steps < 4) {
            SimulateCPU(fixedDt);
            acc -= fixedDt;
            steps++;
        }
        if (acc > fixedDt * 4) acc = 0.0f;
    }

    //==========================================================
    // 通常描画（球体 + アルファブレンド + 深度ソート）
    //==========================================================
    void SPHFluid::DrawParticles(ID3D11DeviceContext* context) {
        if (!m_sphereMesh || m_particleCount == 0) return;

        auto& renderer = Renderer::GetInstance();

        // 現在のブレンドステートを保存
        ComPtr<ID3D11BlendState> prevBlendState;
        FLOAT prevBlendFactor[4];
        UINT prevSampleMask;
        context->OMGetBlendState(prevBlendState.GetAddressOf(), prevBlendFactor, &prevSampleMask);

        // アルファブレンド有効化
        float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        context->OMSetBlendState(m_pAlphaBlendState.Get(), blendFactor, 0xffffffff);

        renderer.SetupFor3D();
        renderer.SetDepthEnable(true);

        // 白テクスチャを設定
        ID3D11ShaderResourceView* whiteTex = renderer.GetWhiteTexture();
        if (whiteTex) {
            context->PSSetShaderResources(0, 1, &whiteTex);
        }

        ID3D11Buffer* matBuffer = renderer.GetMaterialBuffer();
        uint32_t drawCount = std::min(m_particleCount, 500u);

        //----------------------------------------------------------
        // カメラ位置と視線方向を取得して深度ソート
        //----------------------------------------------------------
        Game::Camera& camera = Game::GetMainCamera();
        XMFLOAT3 cameraPos = camera.position;
        XMFLOAT3 cameraAt = camera.Atposition;

        // 視線方向ベクトルを計算
        XMFLOAT3 viewDir = {
            cameraAt.x - cameraPos.x,
            cameraAt.y - cameraPos.y,
            cameraAt.z - cameraPos.z
        };
        // 正規化
        float viewLen = sqrtf(viewDir.x * viewDir.x + viewDir.y * viewDir.y + viewDir.z * viewDir.z);
        if (viewLen > 0.0001f) {
            viewDir.x /= viewLen;
            viewDir.y /= viewLen;
            viewDir.z /= viewLen;
        }

        struct SortedParticle {
            uint32_t index;
            float depth;
        };
        std::vector<SortedParticle> sortedParticles(drawCount);

        for (uint32_t i = 0; i < drawCount; ++i) {
            const SPHParticle& p = m_particles[i];
            // カメラから粒子へのベクトル
            float dx = p.position.x - cameraPos.x;
            float dy = p.position.y - cameraPos.y;
            float dz = p.position.z - cameraPos.z;
            // 視線方向への射影（内積）= 深度
            sortedParticles[i].index = i;
            sortedParticles[i].depth = dx * viewDir.x + dy * viewDir.y + dz * viewDir.z;
        }

        // 遠い順にソート（後ろから描画）
        std::sort(sortedParticles.begin(), sortedParticles.end(),
            [](const SortedParticle& a, const SortedParticle& b) {
                return a.depth > b.depth;
            });

        //----------------------------------------------------------
        // ソート順に描画
        //----------------------------------------------------------
        for (uint32_t i = 0; i < drawCount; ++i) {
            const SPHParticle& p = m_particles[sortedParticles[i].index];

            // 速度に応じた色変化
            float speed = sqrtf(p.velocity.x * p.velocity.x +
                p.velocity.y * p.velocity.y +
                p.velocity.z * p.velocity.z);
            float t = std::min(speed / 10.0f, 1.0f);

            // マテリアル設定
            struct MaterialData {
                XMFLOAT4 Diffuse;
                XMFLOAT4 Ambient;
                XMFLOAT4 Specular;
                XMFLOAT4 Emission;
                float Shininess;
                float padding[3];
            };

            MaterialData mat = {};
            mat.Diffuse = XMFLOAT4(
                m_particleColor.x + t * 0.2f,
                m_particleColor.y + t * 0.2f,
                m_particleColor.z + t * 0.1f,
                m_particleColor.w
            );
            mat.Ambient = XMFLOAT4(0.05f, 0.15f, 0.3f, 1.0f);
            mat.Specular = XMFLOAT4(0.9f, 0.95f, 1.0f, 1.0f);
            mat.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            mat.Shininess = 64.0f;

            context->UpdateSubresource(matBuffer, 0, nullptr, &mat, 0, 0);

            // ワールド行列
            XMMATRIX world = XMMatrixScaling(m_particleScale, m_particleScale, m_particleScale) *
                XMMatrixTranslation(p.position.x, p.position.y, p.position.z);
            renderer.SetWorldMatrix(world);

            // 描画
            m_sphereMesh->Bind(context);
            m_sphereMesh->Draw(context);
        }

        // ブレンドステートを元に戻す
        context->OMSetBlendState(prevBlendState.Get(), prevBlendFactor, prevSampleMask);
    }

    //==========================================================
    // スクリーンスペース描画
    //==========================================================
    void SPHFluid::DrawScreenSpace(ID3D11DeviceContext* context) {
        if (!m_depthRT || !m_blurRT1 || !m_blurRT2 || !m_fullscreenQuad) {
            DrawParticles(context);
            return;
        }

        auto& renderer = Renderer::GetInstance();
        uint32_t width = renderer.GetScreenWidth();
        uint32_t height = renderer.GetScreenHeight();

        ComPtr<ID3D11RenderTargetView> prevRTV;
        ComPtr<ID3D11DepthStencilView> prevDSV;
        context->OMGetRenderTargets(1, &prevRTV, &prevDSV);

        //----------------------------------------------------------
        // パス1: 深度描画
        //----------------------------------------------------------
        m_depthRT->Clear(context, 0, 0, 0, 0);
        m_depthRT->SetAsTarget(context);

        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            context->Map(m_pFluidCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            CBFluid* cb = static_cast<CBFluid*>(mapped.pData);
            XMStoreFloat4x4(&cb->View, XMMatrixIdentity());
            XMStoreFloat4x4(&cb->Projection, XMMatrixIdentity());
            cb->PointRadius = m_particleScale;
            context->Unmap(m_pFluidCB.Get(), 0);
        }
        context->VSSetConstantBuffers(2, 1, m_pFluidCB.GetAddressOf());
        context->PSSetConstantBuffers(2, 1, m_pFluidCB.GetAddressOf());

        context->VSSetShader(m_pDepthVS.Get(), nullptr, 0);
        context->PSSetShader(m_pDepthPS.Get(), nullptr, 0);

        uint32_t drawCount = std::min(m_particleCount, 500u);
        for (uint32_t i = 0; i < drawCount; ++i) {
            const SPHParticle& p = m_particles[i];
            XMMATRIX world = XMMatrixScaling(m_particleScale * 2, m_particleScale * 2, m_particleScale * 2) *
                XMMatrixTranslation(p.position.x, p.position.y, p.position.z);
            renderer.SetWorldMatrix(world);
            m_sphereMesh->Bind(context);
            m_sphereMesh->Draw(context);
        }

        //----------------------------------------------------------
        // パス2: 水平ブラー
        //----------------------------------------------------------
        m_blurRT1->Clear(context, 0, 0, 0, 0);
        m_blurRT1->SetAsTarget(context);

        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            context->Map(m_pBlurCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            CBBlur* cb = static_cast<CBBlur*>(mapped.pData);
            cb->TexelSize = XMFLOAT2(1.0f / width, 1.0f / height);
            cb->BlurScale = 3.0f;         // ピクセル間隔の倍率
            cb->BlurDepthFalloff = 0.0f;  // 使わない
            context->Unmap(m_pBlurCB.Get(), 0);
        }
        context->VSSetConstantBuffers(0, 1, m_pBlurCB.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, m_pBlurCB.GetAddressOf());

        context->IASetInputLayout(m_pQuadInputLayout.Get());
        context->VSSetShader(m_pBlurVS.Get(), nullptr, 0);
        context->PSSetShader(m_pBlurHPS.Get(), nullptr, 0);

        ID3D11ShaderResourceView* depthSRV = m_depthRT->GetSRV();
        context->PSSetShaderResources(0, 1, &depthSRV);
        context->PSSetSamplers(0, 1, m_pPointSampler.GetAddressOf());

        m_fullscreenQuad->Draw(context);

        //----------------------------------------------------------
        // パス3: 垂直ブラー
        //----------------------------------------------------------
        m_blurRT2->Clear(context, 0, 0, 0, 0);
        m_blurRT2->SetAsTarget(context);

        context->PSSetShader(m_pBlurVPS.Get(), nullptr, 0);

        ID3D11ShaderResourceView* blur1SRV = m_blurRT1->GetSRV();
        context->PSSetShaderResources(0, 1, &blur1SRV);

        m_fullscreenQuad->Draw(context);

        //----------------------------------------------------------
        // パス3.5〜3.8: 追加ブラー（2回目）
        //----------------------------------------------------------
        // 水平
        m_blurRT1->Clear(context, 0, 0, 0, 0);
        m_blurRT1->SetAsTarget(context);
        context->PSSetShader(m_pBlurHPS.Get(), nullptr, 0);
        {
            ID3D11ShaderResourceView* srv = m_blurRT2->GetSRV();
            context->PSSetShaderResources(0, 1, &srv);
        }
        m_fullscreenQuad->Draw(context);

        // 垂直
        m_blurRT2->Clear(context, 0, 0, 0, 0);
        m_blurRT2->SetAsTarget(context);
        context->PSSetShader(m_pBlurVPS.Get(), nullptr, 0);
        {
            ID3D11ShaderResourceView* srv = m_blurRT1->GetSRV();
            context->PSSetShaderResources(0, 1, &srv);
        }
        m_fullscreenQuad->Draw(context);

        //----------------------------------------------------------
        // パス3.9〜3.12: 追加ブラー（3回目）
        //----------------------------------------------------------
        // 水平
        m_blurRT1->Clear(context, 0, 0, 0, 0);
        m_blurRT1->SetAsTarget(context);
        context->PSSetShader(m_pBlurHPS.Get(), nullptr, 0);
        {
            ID3D11ShaderResourceView* srv = m_blurRT2->GetSRV();
            context->PSSetShaderResources(0, 1, &srv);
        }
        m_fullscreenQuad->Draw(context);

        // 垂直
        m_blurRT2->Clear(context, 0, 0, 0, 0);
        m_blurRT2->SetAsTarget(context);
        context->PSSetShader(m_pBlurVPS.Get(), nullptr, 0);
        {
            ID3D11ShaderResourceView* srv = m_blurRT1->GetSRV();
            context->PSSetShaderResources(0, 1, &srv);
        }
        m_fullscreenQuad->Draw(context);
        //----------------------------------------------------------
        // パス4: 最終合成
        //----------------------------------------------------------
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        context->PSSetShaderResources(0, 2, nullSRVs);

        context->OMSetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.Get());

        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);

        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            context->Map(m_pFinalCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            CBFinal* cb = static_cast<CBFinal*>(mapped.pData);
            XMStoreFloat4x4(&cb->InvProjection, XMMatrixIdentity());
            cb->TexelSize = XMFLOAT2(1.0f / width, 1.0f / height);
            cb->WaterColor = XMFLOAT3(m_particleColor.x, m_particleColor.y, m_particleColor.z);
            cb->WaterAlpha = m_particleColor.w;
            cb->LightDir = XMFLOAT3(0.5f, 1.0f, 0.3f);
            cb->FresnelPower = 2.0f;
            context->Unmap(m_pFinalCB.Get(), 0);
        }
        context->VSSetConstantBuffers(0, 1, m_pFinalCB.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, m_pFinalCB.GetAddressOf());

        context->VSSetShader(m_pFinalVS.Get(), nullptr, 0);
        context->PSSetShader(m_pFinalPS.Get(), nullptr, 0);

        ID3D11ShaderResourceView* blur2SRV = m_blurRT2->GetSRV();
        ID3D11ShaderResourceView* nullSRV = nullptr;
        context->PSSetShaderResources(0, 1, &blur2SRV);
        context->PSSetShaderResources(1, 1, &nullSRV);
        context->PSSetSamplers(0, 1, m_pLinearSampler.GetAddressOf());

        m_fullscreenQuad->Draw(context);

        context->PSSetShaderResources(0, 1, &nullSRV);
        context->PSSetShaderResources(1, 1, &nullSRV);
    }

    //==========================================================
    // 描画メイン
    //==========================================================
    void SPHFluid::Draw(ID3D11DeviceContext* context) {
        if (!m_initialized || m_particleCount == 0) return;

        auto& renderer = Renderer::GetInstance();

        // 現在の全状態を保存
        ComPtr<ID3D11VertexShader> prevVS;
        ComPtr<ID3D11PixelShader> prevPS;
        ComPtr<ID3D11InputLayout> prevLayout;
        ComPtr<ID3D11RenderTargetView> prevRTV;
        ComPtr<ID3D11DepthStencilView> prevDSV;
        ComPtr<ID3D11Buffer> prevVSCB0, prevVSCB1, prevVSCB2;
        ComPtr<ID3D11Buffer> prevPSCB0, prevPSCB1, prevPSCB2;
        ComPtr<ID3D11SamplerState> prevSampler;
        ComPtr<ID3D11ShaderResourceView> prevSRV0, prevSRV1;
        D3D11_VIEWPORT prevVP;
        UINT numVP = 1;
        D3D11_PRIMITIVE_TOPOLOGY prevTopology;

        context->VSGetShader(&prevVS, nullptr, nullptr);
        context->PSGetShader(&prevPS, nullptr, nullptr);
        context->IAGetInputLayout(&prevLayout);
        context->IAGetPrimitiveTopology(&prevTopology);
        context->OMGetRenderTargets(1, &prevRTV, &prevDSV);
        context->RSGetViewports(&numVP, &prevVP);
        context->VSGetConstantBuffers(0, 1, &prevVSCB0);
        context->VSGetConstantBuffers(1, 1, &prevVSCB1);
        context->VSGetConstantBuffers(2, 1, &prevVSCB2);
        context->PSGetConstantBuffers(0, 1, &prevPSCB0);
        context->PSGetConstantBuffers(1, 1, &prevPSCB1);
        context->PSGetConstantBuffers(2, 1, &prevPSCB2);
        context->PSGetSamplers(0, 1, &prevSampler);
        context->PSGetShaderResources(0, 1, &prevSRV0);
        context->PSGetShaderResources(1, 1, &prevSRV1);

        // 描画実行
        if (m_screenSpaceEnabled && m_depthRT && m_blurRT1 && m_blurRT2 && m_pDepthVS) {
            DrawScreenSpace(context);
        } else {
            DrawParticles(context);
        }

        // 全状態を復元
        context->VSSetShader(prevVS.Get(), nullptr, 0);
        context->PSSetShader(prevPS.Get(), nullptr, 0);
        context->IASetInputLayout(prevLayout.Get());
        context->IASetPrimitiveTopology(prevTopology);
        context->OMSetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.Get());
        context->RSSetViewports(1, &prevVP);

        ID3D11Buffer* vsCBs[3] = { prevVSCB0.Get(), prevVSCB1.Get(), prevVSCB2.Get() };
        context->VSSetConstantBuffers(0, 3, vsCBs);

        ID3D11Buffer* psCBs[3] = { prevPSCB0.Get(), prevPSCB1.Get(), prevPSCB2.Get() };
        context->PSSetConstantBuffers(0, 3, psCBs);

        context->PSSetSamplers(0, 1, prevSampler.GetAddressOf());

        ID3D11ShaderResourceView* srvs[2] = { prevSRV0.Get(), prevSRV1.Get() };
        context->PSSetShaderResources(0, 2, srvs);
    }

} // namespace Engine
