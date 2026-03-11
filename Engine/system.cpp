//==============================================================================
//  File   : system.cpp
//  Brief  : エンジンシステム統括 - 全サブシステムの初期化・更新・終了処理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "system.h"

// --- サブシステムヘッダー ---
#include "Engine/Core/renderer.h"
#include "Engine/Core/timer.h"
#include "Engine/Input/input_manager.h"
#include "Engine/Collision/collision_manager.h"
#include "Engine/Graphics/sprite_2d.h"
#include "Engine/Graphics/sprite_3d.h"
#include "Engine/Graphics/primitive.h"

namespace Engine {

    //==========================================================
    // シングルトンインスタンス取得
    //==========================================================
    System& System::GetInstance() {
        static System instance;
        return instance;
    }

    //==========================================================
    // Initialize - 全サブシステムの初期化
    //==========================================================
    bool System::Initialize(HINSTANCE hInstance, HWND hWnd, bool windowed) {
        // --- 1. タイマー初期化 ---
        // フレーム間デルタタイム計算のための高精度タイマーを準備
        SystemTimer_Initialize();

        // --- 2. レンダラー初期化 ---
        // D3D11デバイス、スワップチェーン、シェーダーを作成
        if (!Renderer::GetInstance().Initialize(hInstance, hWnd, windowed)) {
            return false;
        }

        // --- 3. 入力マネージャー初期化 ---
        // キーボード、マウス、ゲームコントローラーを準備
        InputManager::GetInstance().Initialize(hWnd);

        // --- 4. 衝突判定マネージャー初期化 ---
        // コライダー管理システムを準備
        CollisionManager::GetInstance().Initialize();

        // --- 5. グラフィックスサブシステム初期化 ---
        // 2D/3Dスプライトとプリミティブ描画システムを準備
        auto* pDevice = Renderer::GetInstance().GetDevice();
        Sprite2D::Initialize(pDevice);
        Sprite3D::Initialize(pDevice);
        InitPrimitives(pDevice);

        // --- 初期化完了 ---
        m_lastTime = SystemTimer_GetTime();
        m_initialized = true;
        return true;
    }

    //==========================================================
    // Finalize - 全サブシステムの終了処理（初期化の逆順）
    //==========================================================
    void System::Finalize() {
        if (!m_initialized) return;

        // --- グラフィックスサブシステム終了 ---
        UninitPrimitives();
        Sprite3D::Finalize();
        Sprite2D::Finalize();

        // --- 各マネージャー終了 ---
        CollisionManager::GetInstance().Shutdown();
        InputManager::GetInstance().Finalize();
        Renderer::GetInstance().Finalize();

        m_initialized = false;
    }

    //==========================================================
    // Update - フレーム更新処理
    //==========================================================
    void System::Update() {
        // --- デルタタイム計算 ---
        double currentTime = SystemTimer_GetTime();
        m_deltaTime = static_cast<float>(currentTime - m_lastTime);
        m_lastTime = currentTime;

        // --- 入力状態更新 ---
        InputManager::GetInstance().Update();
    }

    //==========================================================
    // サブシステムアクセサ - 各シングルトンへの参照を返す
    //==========================================================
    Renderer& System::GetRenderer() { return Renderer::GetInstance(); }
    InputManager& System::GetInput() { return InputManager::GetInstance(); }
    CollisionManager& System::GetCollision() { return CollisionManager::GetInstance(); }

    //==========================================================
    // グローバル便利関数 - System経由せず直接アクセス可能
    //==========================================================
    System& GetSystem() { return System::GetInstance(); }
    float GetDeltaTime() { return GetSystem().GetDeltaTime(); }

} // namespace Engine
