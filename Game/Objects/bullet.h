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
#include "Game/Objects/game_object_base.h"
#include "Engine/Collision/box_collider.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace Game {

//==========================================================
// 弾丸クラス
// GameObjectBaseを継承してコライダー管理を共通化
//==========================================================
class Bullet : public GameObjectBase {
public:
    // --- 弾丸の状態（位置はGameObjectBaseから継承） ---
    XMFLOAT3 velocity;          // 移動速度
    float lifeTime;             // 残り寿命（秒）
    GameObject visual;          // 見た目用ゲームオブジェクト

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

    // --- ライフサイクル関数（Update/DrawはGameObjectBaseからオーバーライド） ---
    void Initialize(ID3D11ShaderResourceView* texture, const XMFLOAT3& pos, const XMFLOAT3& dir, int ownerId = 0);
    void Update(float deltaTime) override;
    void Draw() override;
    void Deactivate();

    // --- アクティブ状態取得（GameObjectBaseのm_activeを使用） ---
    bool IsActive() const { return m_active; }
};

} // namespace Game
