//==============================================================================
//  File   : game_manager.h
//  Brief  : ゲームマネージャー - ゲーム全体の統括管理（シングルトン）
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/11
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "main.h"
#include "Game/game.h"
#include <vector>

namespace Game {

//==========================================================
// 前方宣言
//==========================================================
class GameObject;
class Map;
class MapRenderer;
class Player;

//==========================================================
// ゲームマネージャークラス（シングルトン）
//==========================================================
class GameManager {
public:
    // --- シングルトンアクセス ---
    static GameManager& Instance();

    // --- ライフサイクル管理 ---
    HRESULT Initialize();
    void Finalize();
    void Update();
    void Draw();

    // --- シーン遷移管理へのアクセス ---
    Game& GetGame() { return m_game; }
    const Game& GetGame() const { return m_game; }

    // --- ワールドオブジェクト管理（現在のゲームシーン経由） ---
    std::vector<std::shared_ptr<GameObject>>& GetWorldObjects();
    const std::vector<std::shared_ptr<GameObject>>& GetWorldObjects() const;

    // --- 主要オブジェクト参照（現在のゲームシーン経由） ---
    Map* GetMap() const;
    MapRenderer* GetMapRenderer() const;
    GameObject* GetLocalPlayerGameObject() const;

    // --- FPS情報 ---
    double GetFPS() const { return m_fps; }

private:
    // --- シングルトン実装 ---
    GameManager();
    ~GameManager();
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    // --- メンバ変数 ---
    Game m_game;                // シーン遷移管理
    double m_fps = 0.0;         // 現在のFPS
    uint32_t m_inputSeq = 0;    // 入力シーケンス番号

    // --- ワールドオブジェクト（ゲームシーンが無い場合のフォールバック用） ---
    static std::vector<std::shared_ptr<GameObject>> s_emptyWorldObjects;
};

} // namespace Game
