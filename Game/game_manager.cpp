//==============================================================================
//  File   : game_manager.cpp
//==============================================================================
#include "pch.h"
#include "game_manager.h"
#include "Engine/Core/renderer.h"
#include "Engine/Core/timer.h"
#include "Engine/Physics/sph_fluid.h"
#include "Game/Objects/game_object.h"
#include "Game/Objects/camera.h"
#include <iostream>
#include <Windows.h>

namespace Game {

#define CLASS_NAME "DX21 Window"

    std::vector<std::shared_ptr<GameObject>> GameManager::s_emptyWorldObjects;

    GameManager::GameManager() {
    }

    GameManager::~GameManager() {
    }

    GameManager& GameManager::Instance() {
        static GameManager instance;
        return instance;
    }

    GameObject* GameManager::GetLocalPlayerGameObject() const {
        return ::Game::GetLocalPlayerGameObject();
    }

    std::vector<std::shared_ptr<GameObject>>& GameManager::GetWorldObjects() {
        SceneGame* sg = m_game.GetSceneGame();
        if (sg) return sg->GetWorldObjects();
        return s_emptyWorldObjects;
    }

    const std::vector<std::shared_ptr<GameObject>>& GameManager::GetWorldObjects() const {
        const SceneGame* sg = m_game.GetSceneGame();
        if (sg) return sg->GetWorldObjects();
        return s_emptyWorldObjects;
    }

    Map* GameManager::GetMap() const {
        const SceneGame* sg = m_game.GetSceneGame();
        if (sg) return sg->GetMap();
        return nullptr;
    }

    MapRenderer* GameManager::GetMapRenderer() const {
        const SceneGame* sg = m_game.GetSceneGame();
        if (sg) return sg->GetMapRenderer();
        return nullptr;
    }

    void GameManager::SpawnFluidParticles(const DirectX::XMFLOAT3& position, uint32_t count, float radius) {
        if (m_fluid) {
            m_fluid->SpawnParticles(position, count, radius);
        }
    }

    HRESULT GameManager::Initialize() {
#ifdef _DEBUG
        {
            int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
            flags |= _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF;
            _CrtSetDbgFlag(flags);
        }
#endif

        if (FAILED(m_game.Initialize(SceneType::GAME))) {
            return E_FAIL;
        }

        //==========================================================
        // SPH流体初期化
        //==========================================================
        {
            auto& renderer = Engine::Renderer::GetInstance();

            m_fluid = std::make_unique<Engine::SPHFluid>();

            if (!m_fluid->Initialize(renderer.GetDevice(), 10000)) {  // 粒子数を増やす
                OutputDebugStringA("SPHFluid: Initialize failed\n");
                m_fluid.reset();
            } else {
                OutputDebugStringA("SPHFluid: Initialize success!\n");

                // 境界を広く設定（マップ全体をカバー）
                m_fluid->SetBoundary(
                    DirectX::XMFLOAT3(-30.0f, -30.0f, -30.0f),
                    DirectX::XMFLOAT3(30.0f, 30.0f, 30.0f)
                );

                // 衝突判定を有効化
                m_fluid->SetMapCollisionEnabled(true);
                m_fluid->SetPlayerCollisionEnabled(true);

                // 色・サイズ設定
                m_fluid->SetParticleColor(DirectX::XMFLOAT4(0.15f, 0.45f, 0.85f, 0.65f));
                m_fluid->SetParticleScale(0.05f);

                // スクリーンスペース有効化
                m_fluid->SetScreenSpaceEnabled(true);

                // 初期粒子は少なめ（水鉄砲から出す）
                //m_fluid->SpawnParticles(
                //    DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
                //    100,
                //    1.0f
                //);
            }
        }

        return S_OK;
    }

    void GameManager::Finalize() {
        if (m_fluid) {
            m_fluid->Finalize();
            m_fluid.reset();
        }

        m_game.Finalize();
    }

    void GameManager::Update() {
        m_game.Update();

        // 流体更新
        if (m_fluid) {
            auto& renderer = Engine::Renderer::GetInstance();
            m_fluid->Update(renderer.GetContext(), 1.0f / 60.0f);
        }

        // FPS計測
        static int frameCount = 0;
        static double lastFpsTime = SystemTimer_GetTime();

        frameCount++;
        double currentTime = SystemTimer_GetTime();
        double elapsed = currentTime - lastFpsTime;

        if (elapsed >= 0.5) {
            m_fps = frameCount / elapsed;
            frameCount = 0;
            lastFpsTime = currentTime;

            char title[256];
            uint32_t particleCount = m_fluid ? m_fluid->GetParticleCount() : 0;
            sprintf_s(title, "BREAK_SHOOTING - FPS: %.1f | Particles: %u", m_fps, particleCount);

            HWND hWnd = FindWindowA(CLASS_NAME, nullptr);
            if (hWnd) {
                SetWindowTextA(hWnd, title);
            }
        }
    }

    void GameManager::Draw() {
        Engine::Renderer::GetInstance().Clear();

        m_game.Draw();

        // 流体描画
        if (m_fluid) {
            auto& renderer = Engine::Renderer::GetInstance();
            renderer.SetDepthEnable(true);
            m_fluid->Draw(renderer.GetContext());
        }

        Engine::Renderer::GetInstance().Present();
    }

} // namespace Game
