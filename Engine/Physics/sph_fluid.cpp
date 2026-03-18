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
        m_params.smoothingRadius = 0.4f;
        m_params.restDensity = 1000.0f;
        m_params.gasConstant = 1500.0f;
        m_params.viscosity = 150.0f;
        m_params.gravity = XMFLOAT3(0.0f, -9.81f, 0.0f);
        m_params.deltaTime = 1.0f / 60.0f;
        m_params.boundaryMin = XMFLOAT3(-5.0f, -22.0f, -5.0f);
        m_params.boundaryMax = XMFLOAT3(5.0f, 10.0f, 5.0f);
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
    // シェーダー初期化
    //==========================================================
    bool SPHFluid::InitializeShaders(ID3D11Device* device) {
        HRESULT hr;
        ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

        //----------------------------------------------------------
        // 深度シェーダー
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
                float2 TexCoord : TEXCOORD0;
                float Depth     : TEXCOORD1;
            };
            VS_OUTPUT VS_Depth(VS_INPUT input) {
                VS_OUTPUT output;
                output.Position = mul(float4(input.Position, 1.0), WorldViewProjection);
                output.TexCoord = input.TexCoord;
                float4 viewPos = mul(float4(input.Position, 1.0), View);
                output.Depth = -viewPos.z;
                return output;
            }
            float4 PS_Depth(VS_OUTPUT input) : SV_Target {
                float2 uv = input.TexCoord * 2.0 - 1.0;
                float dist = length(uv);
                if (dist > 1.0) discard;
                float z = sqrt(1.0 - dist * dist);
                float depth = input.Depth - z * PointRadius;
                return float4(depth, depth, depth, 1);
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
        // ブラーシェーダー
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
                if (centerDepth <= 0) return float4(0, 0, 0, 0);
                float sum = 0;
                float wsum = 0;
                for (int x = -5; x <= 5; x++) {
                    float2 offset = float2(x * TexelSize.x * BlurScale, 0);
                    float sampleDepth = DepthTexture.Sample(PointSampler, input.TexCoord + offset).r;
                    if (sampleDepth <= 0) continue;
                    float depthDiff = abs(centerDepth - sampleDepth);
                    float w = exp(-depthDiff * BlurDepthFalloff) * exp(-x * x / 10.0);
                    sum += sampleDepth * w;
                    wsum += w;
                }
                float result = sum / max(wsum, 0.0001);
                return float4(result, result, result, 1);
            }
            float4 PS_BlurV(VS_OUTPUT input) : SV_Target {
                float centerDepth = DepthTexture.Sample(PointSampler, input.TexCoord).r;
                if (centerDepth <= 0) return float4(0, 0, 0, 0);
                float sum = 0;
                float wsum = 0;
                for (int y = -5; y <= 5; y++) {
                    float2 offset = float2(0, y * TexelSize.y * BlurScale);
                    float sampleDepth = DepthTexture.Sample(PointSampler, input.TexCoord + offset).r;
                    if (sampleDepth <= 0) continue;
                    float depthDiff = abs(centerDepth - sampleDepth);
                    float w = exp(-depthDiff * BlurDepthFalloff) * exp(-y * y / 10.0);
                    sum += sampleDepth * w;
                    wsum += w;
                }
                float result = sum / max(wsum, 0.0001);
                return float4(result, result, result, 1);
            }
        )";

        // クアッド用入力レイアウト
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
        // 最終合成シェーダー
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
                normal.x = (depthL - depthR) * 0.5;
                normal.y = (depthT - depthB) * 0.5;
                normal.z = 0.1;
                return normalize(normal);
            }
            float4 PS_Final(VS_OUTPUT input) : SV_Target {
                float depth = DepthTexture.Sample(LinearSampler, input.TexCoord).r;
                float4 sceneColor = SceneTexture.Sample(LinearSampler, input.TexCoord);
                if (depth <= 0) {
                    return sceneColor;
                }
                float3 normal = ComputeNormal(input.TexCoord);
                float3 viewDir = float3(0, 0, 1);
                float fresnel = pow(1.0 - saturate(dot(normal, viewDir)), FresnelPower);
                float3 lightDir = normalize(LightDir);
                float diffuse = max(dot(normal, lightDir), 0.0) * 0.5 + 0.5;
                float3 halfVec = normalize(lightDir + viewDir);
                float specular = pow(max(dot(normal, halfVec), 0.0), 64.0);
                float2 refractUV = input.TexCoord + normal.xy * 0.03;
                float3 refractColor = SceneTexture.Sample(LinearSampler, refractUV).rgb;
                float3 reflectColor = float3(0.5, 0.7, 0.9);
                float3 waterBase = WaterColor * diffuse;
                float3 finalColor = lerp(refractColor * 0.7 + waterBase * 0.3, reflectColor, fresnel * 0.4);
                finalColor += specular * float3(1, 1, 1) * 0.5;
                return float4(finalColor, WaterAlpha);
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

        // 球メッシュ読み込み
        m_sphereMesh = MeshFactory::CreateFromFile("resource/model/sphere.fbx");
        if (!m_sphereMesh) {
            OutputDebugStringA("SPHFluid: sphere.fbx 読み込み失敗、プリミティブ使用\n");
            m_sphereMesh = MeshFactory::CreateSphere(0.5f, 12, 8);
        }
        if (!m_sphereMesh) return false;

        if (!m_sphereMesh->IsUploaded()) {
            if (!m_sphereMesh->Upload(device)) return false;
        }

        // スクリーンスペース用リソース
        auto& renderer = Renderer::GetInstance();
        uint32_t width = renderer.GetScreenWidth();
        uint32_t height = renderer.GetScreenHeight();

        m_depthRT = std::make_unique<RenderTarget>();
        if (!m_depthRT->Create(device, width, height, DXGI_FORMAT_R32_FLOAT, true)) {
            OutputDebugStringA("SPHFluid: 深度RT作成失敗\n");
            m_screenSpaceEnabled = false;
        }

        m_blurRT1 = std::make_unique<RenderTarget>();
        if (!m_blurRT1->Create(device, width, height, DXGI_FORMAT_R32_FLOAT, false)) {
            m_screenSpaceEnabled = false;
        }

        m_blurRT2 = std::make_unique<RenderTarget>();
        if (!m_blurRT2->Create(device, width, height, DXGI_FORMAT_R32_FLOAT, false)) {
            m_screenSpaceEnabled = false;
        }

        m_fullscreenQuad = std::make_unique<FullscreenQuad>();
        if (!m_fullscreenQuad->Initialize(device)) {
            m_screenSpaceEnabled = false;
        }

        // シェーダー初期化
        if (!InitializeShaders(device)) {
            OutputDebugStringA("SPHFluid: シェーダー初期化失敗、通常描画にフォールバック\n");
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
        for (uint32_t i = 0; i < count && m_particleCount < m_maxParticles; ++i) {
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
        const float maxSpeed = 10.0f;
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
        const float eps = 0.001f;
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
        while (acc >= fixedDt && steps < 2) {
            SimulateCPU(fixedDt);
            acc -= fixedDt;
            steps++;
        }
    }

    //==========================================================
    // 通常描画（球体）
    //==========================================================
    void SPHFluid::DrawParticles(ID3D11DeviceContext* context) {
        auto& renderer = Renderer::GetInstance();
        uint32_t drawCount = std::min(m_particleCount, 500u);

        for (uint32_t i = 0; i < drawCount; ++i) {
            const SPHParticle& p = m_particles[i];
            float speed = sqrtf(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y + p.velocity.z * p.velocity.z);
            float t = std::min(speed / 5.0f, 1.0f);

            struct MaterialData {
                XMFLOAT4 diffuse, ambient, specular, emission;
                float shininess; float padding[3];
            } mat = {};
            mat.diffuse = XMFLOAT4(0.1f + t * 0.2f, 0.4f + t * 0.3f, 0.9f + t * 0.1f, 1.0f);
            mat.ambient = XMFLOAT4(0.1f, 0.2f, 0.4f, 1.0f);
            mat.specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
            mat.shininess = 50.0f;
            context->UpdateSubresource(renderer.GetMaterialBuffer(), 0, nullptr, &mat, 0, 0);

            XMMATRIX world = XMMatrixScaling(m_particleScale, m_particleScale, m_particleScale) *
                XMMatrixTranslation(p.position.x, p.position.y, p.position.z);
            renderer.SetWorldMatrix(world);

            ID3D11ShaderResourceView* whiteTex = renderer.GetWhiteTexture();
            if (whiteTex) context->PSSetShaderResources(0, 1, &whiteTex);

            m_sphereMesh->Bind(context);
            m_sphereMesh->Draw(context);
        }
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

        // 現在のレンダーターゲットを保存
        ComPtr<ID3D11RenderTargetView> prevRTV;
        ComPtr<ID3D11DepthStencilView> prevDSV;
        context->OMGetRenderTargets(1, &prevRTV, &prevDSV);

        //----------------------------------------------------------
        // パス1: 深度描画
        //----------------------------------------------------------
        m_depthRT->Clear(context, 0, 0, 0, 0);
        m_depthRT->SetAsTarget(context);

        // 流体定数バッファ更新
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            context->Map(m_pFluidCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            CBFluid* cb = static_cast<CBFluid*>(mapped.pData);
            // View/Projection行列はRendererから取得できないので簡易設定
            // 本来はRendererにゲッター追加が必要
            XMStoreFloat4x4(&cb->View, XMMatrixIdentity());
            XMStoreFloat4x4(&cb->Projection, XMMatrixIdentity());
            cb->PointRadius = m_particleScale;
            context->Unmap(m_pFluidCB.Get(), 0);
        }
        context->VSSetConstantBuffers(2, 1, m_pFluidCB.GetAddressOf());
        context->PSSetConstantBuffers(2, 1, m_pFluidCB.GetAddressOf());

        context->VSSetShader(m_pDepthVS.Get(), nullptr, 0);
        context->PSSetShader(m_pDepthPS.Get(), nullptr, 0);

        // パーティクルを深度として描画（ビルボード）
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
            cb->BlurScale = 2.0f;
            cb->BlurDepthFalloff = 10.0f;
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
        // パス4: 最終合成
        //----------------------------------------------------------
    //----------------------------------------------------------
    // ★重要：レンダーステートを元に戻す
    //----------------------------------------------------------

    // シェーダーリソースをクリア
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        context->PSSetShaderResources(0, 2, nullSRVs);

        // 元のレンダーターゲットに戻す（すでにやってるけど確実に）
        context->OMSetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.Get());

        // ビューポートを戻す
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
            cb->WaterColor = XMFLOAT3(0.2f, 0.5f, 0.8f);
            cb->WaterAlpha = 0.9f;
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
        context->PSSetShaderResources(1, 1, &nullSRV);  // シーンテクスチャ（今は無し）
        context->PSSetSamplers(0, 1, m_pLinearSampler.GetAddressOf());

        m_fullscreenQuad->Draw(context);

        // リソースをクリア
        context->PSSetShaderResources(0, 1, &nullSRV);
        context->PSSetShaderResources(1, 1, &nullSRV);


    }

    //==========================================================
    // 描画
    //==========================================================
    void SPHFluid::Draw(ID3D11DeviceContext* context) {
        if (!m_initialized || m_particleCount == 0) return;

        auto& renderer = Renderer::GetInstance();

        // ★★★ 現在の全状態を保存 ★★★
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

        // 描画
        if (m_screenSpaceEnabled && m_depthRT && m_blurRT1 && m_blurRT2) {
            DrawScreenSpace(context);
        } else {
            DrawParticles(context);
        }

        // ★★★ 全状態を復元 ★★★
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
