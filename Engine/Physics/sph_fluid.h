//==============================================================================
//  File   : sph_fluid.h
//==============================================================================
#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <algorithm>
#include "sph_particle.h"
#include "Engine/Graphics/render_target.h"
#include "Engine/Graphics/fullscreen_quad.h"

namespace Engine {
    using namespace DirectX;
    using Microsoft::WRL::ComPtr;

    class Mesh;

    class SPHFluid {
    public:
        SPHFluid();
        ~SPHFluid();

        bool Initialize(ID3D11Device* device, uint32_t maxParticleCount);
        void Finalize();
        void Update(ID3D11DeviceContext* context, float deltaTime);
        void Draw(ID3D11DeviceContext* context);
        void SpawnParticles(const XMFLOAT3& position, uint32_t count, float radius);
        void Clear();
        void SetBoundary(const XMFLOAT3& min, const XMFLOAT3& max);
        void SetGravity(const XMFLOAT3& gravity);
        void SetParticleScale(float scale) { m_particleScale = scale; }
        void SetParticleColor(const XMFLOAT4& color) { m_particleColor = color; }

        uint32_t GetParticleCount() const { return m_particleCount; }
        uint32_t GetMaxParticles() const { return m_maxParticles; }

        void SetScreenSpaceEnabled(bool enabled) { m_screenSpaceEnabled = enabled; }
        bool IsScreenSpaceEnabled() const { return m_screenSpaceEnabled; }

    private:
        // シミュレーション
        void SimulateCPU(float dt);
        float Poly6Kernel(float rSquared, float h);
        XMFLOAT3 SpikyKernelGradient(const XMFLOAT3& r, float dist, float h);
        float ViscosityKernelLaplacian(float r, float h);
        void FindNeighbors();
        void ComputeDensityPressure();
        void ComputeForces();
        void Integrate(float dt);
        void HandleBoundaries();

        // 描画
        void DrawParticles(ID3D11DeviceContext* context);
        void DrawScreenSpace(ID3D11DeviceContext* context);

        // 初期化
        bool InitializeShaders(ID3D11Device* device);
        bool CreateBlendStates(ID3D11Device* device);

    private:
        std::vector<SPHParticle> m_particles;
        std::vector<std::vector<uint32_t>> m_neighbors;
        uint32_t m_particleCount = 0;
        uint32_t m_maxParticles = 0;

        SPHParams m_params;
        float m_particleScale = 0.15f;
        XMFLOAT4 m_particleColor = { 0.1f, 0.4f, 0.8f, 0.7f };

        std::shared_ptr<Mesh> m_sphereMesh;

        ID3D11Device* m_pDevice = nullptr;
        bool m_initialized = false;

        // ブレンドステート
        ComPtr<ID3D11BlendState> m_pAlphaBlendState;

        // スクリーンスペース流体用
        bool m_screenSpaceEnabled = false;  // デフォルトはOFF
        std::unique_ptr<RenderTarget> m_depthRT;
        std::unique_ptr<RenderTarget> m_blurRT1;
        std::unique_ptr<RenderTarget> m_blurRT2;
        std::unique_ptr<FullscreenQuad> m_fullscreenQuad;

        // シェーダー
        ComPtr<ID3D11VertexShader> m_pDepthVS;
        ComPtr<ID3D11PixelShader> m_pDepthPS;
        ComPtr<ID3D11VertexShader> m_pBlurVS;
        ComPtr<ID3D11PixelShader> m_pBlurHPS;
        ComPtr<ID3D11PixelShader> m_pBlurVPS;
        ComPtr<ID3D11VertexShader> m_pFinalVS;
        ComPtr<ID3D11PixelShader> m_pFinalPS;
        ComPtr<ID3D11InputLayout> m_pQuadInputLayout;

        // 定数バッファ
        ComPtr<ID3D11Buffer> m_pFluidCB;
        ComPtr<ID3D11Buffer> m_pBlurCB;
        ComPtr<ID3D11Buffer> m_pFinalCB;

        // サンプラー
        ComPtr<ID3D11SamplerState> m_pPointSampler;
        ComPtr<ID3D11SamplerState> m_pLinearSampler;
    };
}
