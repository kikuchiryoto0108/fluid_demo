#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include "sph_particle.h"
#include "Engine/Graphics/render_target.h"

namespace Engine {

    using Microsoft::WRL::ComPtr;
    using namespace DirectX;

    // NOTE: SPHParams is already defined in sph_particle.h — do NOT redefine here

    // CB structs for screen-space fluid shaders (must match HLSL)
    struct CBCamera {
        XMFLOAT4X4 View;
        XMFLOAT4X4 Proj;
        float      PointRadius;
        XMFLOAT2   ScreenSize;
        float      _pad0;  // NearZとして使用可能
    };

    struct CBBlur {
        XMFLOAT2 BlurDir;
        float    BlurScale;
        float    BlurDepthFalloff;
        int      FilterRadius;
        XMFLOAT3 _pad1;
    };

    struct CBFinal {
        XMFLOAT4X4 InvProj;
        XMFLOAT2   TexelSize;
        float      WaterAlpha;
        float      FresnelPower;
        XMFLOAT3   WaterColor;
        float      _pad2;       // NearZ
        XMFLOAT3   WaterDeepColor;
        float      _pad3;       // FarZ
        XMFLOAT3   LightDir;
        float      SpecPower;
        XMFLOAT3   AbsorptionCoeff;
        float      RefractScale;
    };

    class SPHFluid {
    public:
        SPHFluid();
        ~SPHFluid();

        // Original signature: (device, maxParticles) — screen size obtained from swap chain
        bool Initialize(ID3D11Device* device, int maxParticles);
        void Finalize();
        void Shutdown();

        void Update(ID3D11DeviceContext* ctx, float dt);
        void Draw(ID3D11DeviceContext* ctx);

        void SpawnParticles(const XMFLOAT3& center, uint32_t count, float radius);
        void SpawnParticleWithVelocity(const XMFLOAT3& pos, const XMFLOAT3& vel);

        void SetBoundary(const XMFLOAT3& bmin, const XMFLOAT3& bmax) {
            m_boundaryMin = bmin; m_boundaryMax = bmax;
        }

        void SetCamera(const XMFLOAT3& pos, const XMFLOAT3& at, const XMFLOAT3& up,
            float fov, float aspect, float nearZ, float farZ) {
            m_camPos = pos; m_camAt = at; m_camUp = up;
            m_fov = fov; m_aspect = aspect; m_nearZ = nearZ; m_farZ = farZ;
        }

        void SetScreenSpaceEnabled(bool b) { m_screenSpaceEnabled = b; }
        bool GetScreenSpaceEnabled() const { return m_screenSpaceEnabled; }

        void SetParticleScale(float s) { m_particleScale = s; }
        float GetParticleScale() const { return m_particleScale; }

        void SetParticleLifetime(float t) { m_particleLifetime = t; }
        void SetParticleColor(const XMFLOAT4& c) { m_particleColor = c; }

        void SetMapCollisionEnabled(bool b) { m_mapCollisionEnabled = b; }
        void SetPlayerCollisionEnabled(bool b) { m_playerCollisionEnabled = b; }

        uint32_t GetParticleCount() const { return m_particleCount; }
        uint32_t GetMaxParticles()  const { return m_maxParticles; }
        void SetMaxParticles(uint32_t n) { m_maxParticles = n; }

        SPHParams& GetParams() { return m_params; }

        // シーン深度SRVを外部から設定するためのインターフェース
        void SetSceneDepthSRV(ID3D11ShaderResourceView* srv) { m_sceneDepthSRV = srv; }

    private:
        void SimulateCPU(float dt);
        float Poly6Kernel(float r, float h);
        XMFLOAT3 SpikyGradient(const XMFLOAT3& diff, float r, float h);
        float ViscosityLaplacian(float r, float h);
        void EnforceBoundary(SPHParticle& p);

        bool InitializeScreenSpace();
        bool InitializeShaders();
        bool CreateBillboardResources();
        bool CreateRenderTargets();
        bool CreateStates();
        bool CreateConstantBuffers();
        bool CreateSamplers();

        void DrawScreenSpace(ID3D11DeviceContext* ctx);
        void DrawParticles(ID3D11DeviceContext* ctx);
        void UpdateInstanceBuffer(ID3D11DeviceContext* ctx);

        void CollideWithMap();
        void CollideWithPlayers();
    public:
        struct CollisionBox {
            XMFLOAT3 center;
            XMFLOAT3 halfSize;
        };
        void SetPlayerColliders(const std::vector<CollisionBox>& boxes) { m_playerBoxes = boxes; }

    private:
        std::vector<CollisionBox> m_playerBoxes;

        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;
        bool m_initialized = false;
        bool m_ssInitialized = false;

        UINT m_screenWidth = 0;
        UINT m_screenHeight = 0;

        std::vector<SPHParticle> m_particles;
        uint32_t m_particleCount = 0;
        uint32_t m_maxParticles = 50000;
        float    m_particleScale = 0.2f;
        float    m_particleLifetime = 5.0f;
        bool     m_screenSpaceEnabled = true;
        bool     m_mapCollisionEnabled = false;
        bool     m_playerCollisionEnabled = false;
        XMFLOAT4 m_particleColor = { 0.3f, 0.6f, 0.9f, 0.9f };  // より明るい青

        SPHParams m_params;

        XMFLOAT3 m_boundaryMin = { -10, -25, -10 };
        XMFLOAT3 m_boundaryMax = { 10,   5,  10 };

        XMFLOAT3 m_camPos = { 0, 0, -10 };
        XMFLOAT3 m_camAt = { 0, 0,   0 };
        XMFLOAT3 m_camUp = { 0, 1,   0 };
        float m_fov = XM_PIDIV4;
        float m_aspect = 16.0f / 9.0f;
        float m_nearZ = 0.1f;
        float m_farZ = 1000.0f;

        // Billboard
        ComPtr<ID3D11Buffer>      m_pBillboardVB;
        ComPtr<ID3D11Buffer>      m_pBillboardIB;
        ComPtr<ID3D11Buffer>      m_pInstanceBuffer;
        ComPtr<ID3D11InputLayout> m_pBillboardLayout;

        // Shaders
        ComPtr<ID3D11VertexShader> m_pDepthVS;
        ComPtr<ID3D11PixelShader>  m_pDepthPS;
        ComPtr<ID3D11PixelShader>  m_pThicknessPS;
        ComPtr<ID3D11VertexShader> m_pQuadVS;
        ComPtr<ID3D11PixelShader>  m_pBilateralBlurPS;
        ComPtr<ID3D11PixelShader>  m_pGaussianBlurPS;
        ComPtr<ID3D11PixelShader>  m_pFinalPS;

        // Render targets
        RenderTarget* m_depthRT = nullptr;
        RenderTarget* m_blurRT1 = nullptr;
        RenderTarget* m_blurRT2 = nullptr;
        RenderTarget* m_thicknessRT = nullptr;
        RenderTarget* m_thickBlurRT1 = nullptr;
        RenderTarget* m_thickBlurRT2 = nullptr;

        // Constant buffers
        ComPtr<ID3D11Buffer> m_pCameraCB;
        ComPtr<ID3D11Buffer> m_pBlurCB;
        ComPtr<ID3D11Buffer> m_pFinalCB;

        // States
        ComPtr<ID3D11BlendState>        m_pAlphaBlendState;
        ComPtr<ID3D11BlendState>        m_pAdditiveBlendState;
        ComPtr<ID3D11DepthStencilState> m_pDepthDisabledState;
        ComPtr<ID3D11RasterizerState>   m_pNoCullRS;
        ComPtr<ID3D11BlendState> m_pMinBlendState;

        // Samplers
        ComPtr<ID3D11SamplerState> m_pPointSampler;
        ComPtr<ID3D11SamplerState> m_pLinearSampler;

        // シーン深度テクスチャ（壁の奥の水を棄却するために使用）
        ComPtr<ID3D11ShaderResourceView> m_sceneDepthSRV;
    };

} // namespace Engine
