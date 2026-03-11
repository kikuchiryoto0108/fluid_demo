//==============================================================================
//  File   : player_manager.h
//  Brief  : プレイヤーマネージャー - 複数プレイヤーの一括管理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "Game/Objects/player.h"
#include "Game/Objects/camera.h"
#include <memory>
#include <vector>
#include <d3d11.h>

namespace Game {

class Map;
class Bullet;

//==========================================================
// プレイヤーマネージャークラス（シングルトン）
//==========================================================
class PlayerManager {
private:
    static PlayerManager* instance;
    Player player1;             // プレイヤー1
    Player player2;             // プレイヤー2
    int activePlayerId;         // アクティブなプレイヤーID
    bool player1Initialized;    // プレイヤー1初期化フラグ
    bool player2Initialized;    // プレイヤー2初期化フラグ
    bool initialPlayerLocked;   // 初期プレイヤーロックフラグ
    
    // --- プレイヤー用テクスチャのキャッシュ ---
    ID3D11ShaderResourceView* m_playerTexture = nullptr;

    PlayerManager();

public:
    // --- シングルトンアクセス ---
    static PlayerManager& GetInstance();

    // --- 初期化・更新・描画 ---
    void Initialize(Map* map, ID3D11ShaderResourceView* texture);
    void SetInitialActivePlayer(int playerId);
    void Update(float deltaTime);
    void Draw();

    // --- アクティブプレイヤー管理 ---
    void SetActivePlayer(int playerId);
    int GetActivePlayerId() const { return activePlayerId; }

    // --- プレイヤー取得 ---
    Player* GetActivePlayer();
    Player* GetPlayer(int playerId);

    // --- 入力処理 ---
    void HandleInput(float deltaTime);

    // --- ネットワーク同期: 相手の位置を外部から更新 ---
    void ForceUpdatePlayer(int playerId, const XMFLOAT3& pos, const XMFLOAT3& rot, bool isAlive);
};

//==========================================================
// グローバル関数
//==========================================================
void InitializePlayers(Map* map, ID3D11ShaderResourceView* texture);
void UpdatePlayers();
void DrawPlayers();
GameObject* GetActivePlayerGameObject();
Player* GetActivePlayer();

} // namespace Game
