//==============================================================================
//  File   : bullet.h
//  Brief  : 弾丸クラス - 発射体の管理と衝突判定
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once
#include "main.h"
#include "Game/Objects/game_object.h"
#include "Engine/Collision/box_collider.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace Game {

//==========================================================
// 弾丸クラス
//==========================================================
class Bullet {
public:
    // --- 弾丸の状態 ---
    XMFLOAT3 position;          // 現在位置
    XMFLOAT3 velocity;          // 移動速度
    float lifeTime;             // 残り寿命（秒）
    bool active;                // アクティブフラグ
    Engine::BoxCollider collider;   // 衝突判定用コライダー
    GameObject visual;          // 見た目用ゲームオブジェクト
    uint32_t m_collisionId = 0; // 衝突システム登録ID

    // --- 所有者情報 ---
    int ownerPlayerId = 0;      // この弾を撃ったプレイヤーのID（自弾には当たらない）

    // --- コンストラクタ・デストラクタ ---
    Bullet();
    ~Bullet();

    // --- コピー禁止・ムーブ許可 ---
    Bullet(const Bullet&) = delete;
    Bullet& operator=(const Bullet&) = delete;
    Bullet(Bullet&&) noexcept;
    Bullet& operator=(Bullet&&) noexcept;

    // --- ライフサイクル関数 ---
    void Initialize(ID3D11ShaderResourceView* texture, const XMFLOAT3& pos, const XMFLOAT3& dir, int ownerId = 0);
    void Update(float deltaTime);
    void Draw();
    void Deactivate();
};

} // namespace Game
