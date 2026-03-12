//==============================================================================
//  File   : game.cpp
//  Brief  : ゲームシーン管理 - シーンの初期化・更新・描画処理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "game.h"
#include "Engine/Core/renderer.h"
#include "Engine/Graphics/vertex.h"
#include "Engine/Graphics/material.h"
#include "Engine/Graphics/primitive.h"
#include "Engine/Collision/collision_system.h"
#include "Engine/Collision/map_collision.h"
#include "Engine/Input/keyboard.h"
#include "Game/Map/map.h"
#include "Game/Map/map_renderer.h"
#include "Game/Objects/player.h"
#include "Game/Managers/player_manager.h"
#include "Game/Managers/bullet_manager.h"
#include "Game/Objects/camera.h"
#include "NetWork/network_manager.h"
#include "Game/Objects/bullet.h"
#include "Game/Objects/game_object.h"
#include "Engine/Core/timer.h"
#include <iostream>
#include <sstream>

namespace Game {

//==========================================================
// ヘルパー関数
//==========================================================

// 外部で寿命管理されるオブジェクトを非所有shared_ptrとしてラップする
template<typename T>
std::shared_ptr<T> MakeNonOwning(T* ptr) {
    return std::shared_ptr<T>(ptr, [](T*) {});
}

//==========================================================
// マクロ定義
//==========================================================
#define CLASS_NAME "GOD_FPS_SHOOTING_GAME"

//==========================================================
// SceneGame 実装
//==========================================================
SceneGame::SceneGame() {
    m_sceneType = SceneType::GAME;
}

SceneGame::~SceneGame() {
}

HRESULT SceneGame::Initialize() {
    // --- マップ初期化 ---
    m_pMap = new Map();
    if (m_pMap) {
        m_pMap->Initialize(Engine::GetDefaultTexture());
    }
    m_pMapRenderer = new MapRenderer();
    if (m_pMapRenderer) {
        m_pMapRenderer->Initialize(m_pMap);
    }

    // --- ホスト/クライアント選択ダイアログ ---
    HWND hWnd = FindWindowA(CLASS_NAME, nullptr);
    int msgRes = MessageBox(hWnd,
        "Yes = HOST (Player1, TPS)\nNo = CLIENT (Player2, FPS)",
        "Select Role", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);
    bool isHost = (msgRes == IDYES);

    PlayerManager::GetInstance().SetInitialActivePlayer(isHost ? 1 : 2);

    // --- 両方のプレイヤーを初期化 ---
    InitializePlayers(m_pMap, Engine::GetDefaultTexture());

    // --- カメラ初期化 ---
    InitializeCameraSystem();

    // --- ワールドオブジェクトにマップブロックを登録 ---
    m_worldObjects.clear();
    if (m_pMap) {
        const auto& blocks = m_pMap->GetBlockObjects();
        for (const auto& sp : blocks) {
            m_worldObjects.push_back(sp);
        }
    }

    // --- 衝突システム初期化 ---
    Engine::CollisionSystem::GetInstance().Initialize();
    Engine::MapCollision::GetInstance().Initialize(2.0f);
    if (m_pMap) {
        const auto& blocks = m_pMap->GetBlockObjects();
        for (const auto& block : blocks) {
            Engine::MapCollision::GetInstance().RegisterBlock(block->GetBoxCollider());
        }
    }

    // --- 衝突コールバック設定 ---
    Engine::CollisionSystem::GetInstance().SetCallback(
        [](const Engine::CollisionHit& hit) {
            Bullet* bullet = nullptr;
            Player* player = nullptr;
            if (Engine::HasFlag(hit.dataA->layer, Engine::CollisionLayer::PROJECTILE))
                bullet = static_cast<Bullet*>(hit.dataA->userData);
            if (Engine::HasFlag(hit.dataB->layer, Engine::CollisionLayer::PROJECTILE))
                bullet = static_cast<Bullet*>(hit.dataB->userData);
            if (Engine::HasFlag(hit.dataA->layer, Engine::CollisionLayer::PLAYER))
                player = static_cast<Player*>(hit.dataA->userData);
            if (Engine::HasFlag(hit.dataB->layer, Engine::CollisionLayer::PLAYER))
                player = static_cast<Player*>(hit.dataB->userData);
            if (bullet && player && bullet->active && player->IsAlive()) {
                if (bullet->ownerPlayerId == player->GetPlayerId()) return;
                bullet->Deactivate();
                player->TakeDamage(1);
                std::cout << "[Hit!] Player " << player->GetPlayerId()
                    << " HP=" << player->GetHP() << "/" << player->GetMaxHP() << "\n";
                if (!player->IsAlive()) {
                    std::cout << "[Kill!] Player " << player->GetPlayerId() << " eliminated!\n";
                }
            }
        }
    );

    // =========================================
    // 破壊可能な壁の初期化
    // =========================================
    if (m_destructibleWall.LoadFromFile("resource/model/break_block.fbx", Engine::GetDevice())) {
        // 壁の位置を設定（マップに合わせて調整）
        m_destructibleWall.SetPosition({ 0.0f, -10.0f, 0.0f });
        m_destructibleWall.SetScale({ 1.0f, 1.0f, 1.0f });
        m_destructibleWall.SetGroundY(-23.0f);  // 地面の高さを設定

        // BulletManagerに壁を登録
        BulletManager::GetInstance().RegisterWall(&m_destructibleWall);

        std::cout << "[SceneGame] DestructibleWall loaded: "
            << m_destructibleWall.GetFragmentCount() << " fragments\n";
    } else {
        std::cout << "[SceneGame] WARNING: Failed to load destructible wall\n";
    }

    // --- ネットワーク起動 ---
    if (isHost) {
        if (g_network.start_as_host()) {
            std::cout << "[SceneGame] HOST started - waiting for client...\n";
        }
    } else {
        if (g_network.start_as_client()) {
            std::string hostIp;
            if (g_network.discover_and_join(hostIp)) {
                std::cout << "[SceneGame] CLIENT connected to " << hostIp << "\n";
            } else {
                std::cout << "[SceneGame] CLIENT: host not found\n";
            }
        }
    }

    // --- ローカルプレイヤーのID設定 ---
    GameObject* localGo = GetLocalPlayerGameObject();
    if (localGo) {
        localGo->setId(isHost ? 1 : 2);
        m_worldObjects.push_back(MakeNonOwning(localGo));
    }

    // --- 相手プレイヤーのGameObjectもworldObjectsに登録 ---
    Player* otherPlayer = PlayerManager::GetInstance().GetPlayer(isHost ? 2 : 1);
    if (otherPlayer) {
        GameObject* otherGo = otherPlayer->GetGameObject();
        if (otherGo) {
            otherGo->setId(isHost ? 2 : 1);
            m_worldObjects.push_back(MakeNonOwning(otherGo));
        }
    }

    return S_OK;
}

void SceneGame::Finalize() {
    // BulletManagerから壁の登録を解除
    BulletManager::GetInstance().ClearWalls();

    if (m_pMapRenderer) {
        m_pMapRenderer->Uninitialize();
        delete m_pMapRenderer;
        m_pMapRenderer = nullptr;
    }

    m_worldObjects.clear();

    if (m_pMap) {
        m_pMap->Uninitialize();
        delete m_pMap;
        m_pMap = nullptr;
    }

    Engine::CollisionSystem::GetInstance().Shutdown();
    Engine::MapCollision::GetInstance().Shutdown();
}

void SceneGame::Update() {
    static int frameCounter = 0;
    frameCounter++;

    // --- ネットワーク更新 ---
    GameObject* localGo = GetLocalPlayerGameObject();
    constexpr float fixedDt = 1.0f / 60.0f;
    g_network.update(fixedDt, localGo, m_worldObjects);

    // --- ネットワーク補間（ローカル以外） ---
    int localId = 0;
    if (localGo) localId = (int)localGo->getId();

    for (const auto& go : m_worldObjects) {
        if (!go) continue;
        int goId = (int)go->getId();

        if (goId == localId && localId != 0) continue;
        if (goId != 1 && goId != 2) continue;

        if (go->hasNetworkTarget()) {
            go->updateNetworkInterpolation(0.2f);

            bool isAlive = go->getNetworkAlive();
            PlayerManager::GetInstance().ForceUpdatePlayer(
                goId, go->getPosition(), go->getRotation(), isAlive);
        }
    }

    // --- フレーム同期（3フレームごと） ---
    if (frameCounter % 3 == 0) {
        bool isNetworkActive = g_network.is_host() || g_network.getMyPlayerId() != 0;
        if (isNetworkActive) {
            g_network.FrameSync(localGo, m_worldObjects);
        }
    }

    // --- ゲームロジック更新 ---
    UpdateCameraSystem();
    UpdatePlayer();
    Engine::CollisionSystem::GetInstance().Update();

    // =========================================
    // 破壊可能な壁の更新
    // =========================================
    m_destructibleWall.Update(fixedDt);
}

void SceneGame::Draw() {
    // --- 3D描画 ---
    if (m_pMapRenderer) {
        m_pMapRenderer->Draw();
    }

    // =========================================
    // 破壊可能な壁の描画
    // =========================================
    m_destructibleWall.Draw();

    DrawPlayers();

    // --- 2D UI描画 ---
    DrawHPDisplay();
}

//==========================================================
// DrawHPDisplay - 画面左上にHPバーとテキストを描画
//==========================================================
    void SceneGame::DrawHPDisplay() {
        Player* activePlayer = PlayerManager::GetInstance().GetActivePlayer();
        if (!activePlayer) return;

        int currentHP = activePlayer->GetHP();
        int maxHP = activePlayer->GetMaxHP();

        // --- HPバーの設定 ---
        const float barX = 20.0f;       // 左端からの位置
        const float barY = 20.0f;       // 上端からの位置
        const float barWidth = 200.0f;  // バーの最大幅
        const float barHeight = 20.0f;  // バーの高さ

        // HP割合に応じたバーの幅
        float hpRatio = (maxHP > 0) ? (float)currentHP / (float)maxHP : 0.0f;
        float filledWidth = barWidth * hpRatio;

        // --- HPバーの色（HPが減ると緑→黄→赤に変化） ---
        XMFLOAT4 barColor;
        if (hpRatio > 0.5f) {
            barColor = { 0.0f, 1.0f, 0.0f, 1.0f };  // 50%以上: 緑
        } else if (hpRatio > 0.2f) {
            barColor = { 1.0f, 1.0f, 0.0f, 1.0f };  // 20%〜50%: 黄色
        } else {
            barColor = { 1.0f, 0.0f, 0.0f, 1.0f };  // 20%以下: 赤
        }

        // --- 描画: 背景（暗い灰色のバー） ---
        Engine::Renderer::GetInstance().DrawRect2D(
            barX, barY, barWidth, barHeight,
            { 0.3f, 0.3f, 0.3f, 0.8f }
        );

        // --- 描画: HP残量バー ---
        if (filledWidth > 0.0f) {
            Engine::Renderer::GetInstance().DrawRect2D(
                barX, barY, filledWidth, barHeight,
                barColor
            );
        }

        // --- 描画: HPテキスト ---
        char hpText[64];
        sprintf_s(hpText, "HP: %d / %d", currentHP, maxHP);
        Engine::Renderer::GetInstance().DrawText2D(
            barX + 5.0f,
            barY + 2.0f,
            hpText,
            { 1.0f, 1.0f, 1.0f, 1.0f }
        );
    }

//==========================================================
// SceneTitle 実装（将来の拡張用スタブ）
//==========================================================
    SceneTitle::SceneTitle() {
        m_sceneType = SceneType::TITLE;
    }

    SceneTitle::~SceneTitle() {
    }

    HRESULT SceneTitle::Initialize() {
        return S_OK;
    }

    void SceneTitle::Finalize() {
    }

    void SceneTitle::Update() {
    }

    void SceneTitle::Draw() {
    }

//==========================================================
// SceneResult 実装（将来の拡張用スタブ）
//==========================================================
    SceneResult::SceneResult() {
        m_sceneType = SceneType::RESULT;
    }

    SceneResult::~SceneResult() {
    }

    HRESULT SceneResult::Initialize() {
        return S_OK;
    }

    void SceneResult::Finalize() {
    }

    void SceneResult::Update() {
    }

    void SceneResult::Draw() {
    }

//==========================================================
// Game クラス実装（シーン遷移管理）
//==========================================================
    Game::Game() {
    }

    Game::~Game() {
    }

    HRESULT Game::Initialize(SceneType firstScene) {
        m_pCurrentScene = CreateScene(firstScene);
        if (!m_pCurrentScene) {
            return E_FAIL;
        }

        m_currentSceneType = firstScene;
        return m_pCurrentScene->Initialize();
    }

    void Game::Finalize() {
        if (m_pCurrentScene) {
            m_pCurrentScene->Finalize();
            m_pCurrentScene.reset();
        }
        m_currentSceneType = SceneType::NONE;
    }

    void Game::Update() {
        if (m_sceneChangeRequested) {
            ExecuteSceneChange();
        }

        if (m_pCurrentScene) {
            m_pCurrentScene->Update();
        }
    }

    void Game::Draw() {
        if (m_pCurrentScene) {
            m_pCurrentScene->Draw();
        }
    }

    void Game::RequestSceneChange(SceneType nextScene) {
        if (nextScene != m_currentSceneType) {
            m_nextSceneType = nextScene;
            m_sceneChangeRequested = true;
        }
    }

    SceneGame* Game::GetSceneGame() const {
        if (m_currentSceneType == SceneType::GAME && m_pCurrentScene) {
            return static_cast<SceneGame*>(m_pCurrentScene.get());
        }
        return nullptr;
    }

    void Game::ExecuteSceneChange() {
        m_sceneChangeRequested = false;

        if (m_pCurrentScene) {
            m_pCurrentScene->Finalize();
            m_pCurrentScene.reset();
        }

        m_pCurrentScene = CreateScene(m_nextSceneType);
        if (m_pCurrentScene) {
            if (SUCCEEDED(m_pCurrentScene->Initialize())) {
                m_currentSceneType = m_nextSceneType;
                std::cout << "[Game] Scene changed to: " << static_cast<int>(m_currentSceneType) << "\n";
            } else {
                std::cout << "[Game] ERROR: Failed to initialize scene " << static_cast<int>(m_nextSceneType) << "\n";
                m_pCurrentScene.reset();
                m_currentSceneType = SceneType::NONE;
            }
        }

        m_nextSceneType = SceneType::NONE;
    }

    std::unique_ptr<Scene> Game::CreateScene(SceneType type) {
        switch (type) {
        case SceneType::TITLE:
            return std::make_unique<SceneTitle>();
        case SceneType::GAME:
            return std::make_unique<SceneGame>();
        case SceneType::RESULT:
            return std::make_unique<SceneResult>();
        default:
            return nullptr;
        }
    }

} // namespace Game
