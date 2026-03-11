//==============================================================================
//  File   : game_object.h
//  Brief  : ゲームオブジェクト - 3Dオブジェクトの基底クラス
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include <memory>
#include "Engine/Core/renderer.h"
#include "Engine/Graphics/vertex.h"
#include "Engine/Graphics/mesh.h"
#include "Engine/Collision/box_collider.h"
#include "Engine/Graphics/material.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace Game {

//==========================================================
// オブジェクト種別を識別する列挙型
//==========================================================
enum class ObjectTag {
    NONE,       // 未設定
    PLAYER,     // プレイヤー
    ENEMY,      // 敵
    BULLET,     // 弾丸
    MAP_BLOCK,  // マップブロック
    TRIGGER,    // トリガー
    CAMERA      // カメラ
};

//==========================================================
// ゲームオブジェクトクラス
//==========================================================
class GameObject {
public:
    // --- 位置・回転・スケール ---
    XMFLOAT3 position;
    XMFLOAT3 velocity;
    XMFLOAT3 scale;
    XMFLOAT3 rotation;

    // --- 衝突判定 ---
    std::unique_ptr<Engine::BoxCollider> boxCollider;

    // --- メッシュデータ（旧式: 頂点配列） ---
    const Engine::Vertex3D* meshVertices;
    int meshVertexCount;
    ID3D11ShaderResourceView* texture;

    ID3D11Buffer* vertexBuffer;
    bool bufferNeedsUpdate;

    // --- メッシュデータ（新式: Meshクラス） ---
    std::vector<std::shared_ptr<Engine::Mesh>> m_meshes;

    // --- コンストラクタ・デストラクタ ---
    GameObject();
    virtual ~GameObject();

    // --- コピー禁止・ムーブ許可 ---
    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;
    GameObject(GameObject&& other) noexcept;
    GameObject& operator=(GameObject&& other) noexcept;

    // --- ライフサイクル仮想関数 ---
    virtual void Initialize() {}
    virtual void Finalize() {}
    virtual void Update(float deltaTime) {}
    virtual void OnCollision(GameObject* other) {}

    // --- コライダー設定 ---
    void setBoxCollider(const XMFLOAT3& size);
    
    // --- メッシュ設定（旧式: 頂点配列） ---
    void setMesh(const Engine::Vertex3D* vertices, int count, ID3D11ShaderResourceView* tex);

    // --- メッシュ設定（新式: Meshクラス） ---
    void setMesh(std::shared_ptr<Engine::Mesh> mesh, ID3D11ShaderResourceView* tex);
    void setMeshes(const std::vector<std::shared_ptr<Engine::Mesh>>& meshes, ID3D11ShaderResourceView* tex);

    // --- 描画処理 ---
    virtual void draw();

    // --- バッファ更新マーク ---
    void markBufferForUpdate() { bufferNeedsUpdate = true; m_worldMatrixDirty = true; }

    // --- コライダーアクセス ---
    Engine::BoxCollider* GetBoxCollider() const { return boxCollider.get(); }

    // --- ID管理 ---
    uint32_t getId() const { return id; }
    void setId(uint32_t v) { id = v; }

    // --- 位置・速度・回転アクセサ ---
    XMFLOAT3 getPosition() const { return position; }
    void setPosition(const XMFLOAT3& p);

    XMFLOAT3 getVelocity() const { return velocity; }
    void setVelocity(const XMFLOAT3& v) { velocity = v; }

    XMFLOAT3 getRotation() const { return rotation; }
    void setRotation(const XMFLOAT3& r);

    XMFLOAT3 getScale() const { return scale; }

    // --- 共通ヘルパー関数 ---
    void Move(const XMFLOAT3& direction, float speed, float deltaTime);
    bool IsActive() const { return m_active; }
    void SetActive(bool active) { m_active = active; }
    bool IsMarkedForDelete() const { return m_deleteFlag; }
    void MarkForDelete() { m_deleteFlag = true; }
    ObjectTag GetTag() const { return m_tag; }
    void SetTag(ObjectTag tag) { m_tag = tag; }

protected:
    ObjectTag m_tag = ObjectTag::NONE;
    bool m_active = true;
    bool m_deleteFlag = false;

private:
    uint32_t id = 0;
    XMMATRIX m_cachedWorldMatrix = XMMatrixIdentity();
    bool m_worldMatrixDirty = true;

    void createVertexBuffer();
    void updateColliderTransform();

    // --- ネットワーク補間用 ---
    XMFLOAT3 m_netTargetPos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_netTargetRot = { 0.0f, 0.0f, 0.0f };
    bool m_hasNetTarget = false;
    bool m_netIsAlive = true;

public:
    // --- ネットワーク受信時の補間ターゲットを設定 ---
    void setNetworkTarget(const XMFLOAT3& targetPos, const XMFLOAT3& targetRot, bool isAlive = true);

    // --- 毎フレーム呼んで補間ターゲットに向かって滑らかに移動 ---
    void updateNetworkInterpolation(float lerpFactor = 0.2f);

    // --- 補間ターゲットがあるか ---
    bool hasNetworkTarget() const { return m_hasNetTarget; }

    // --- ネットワーク経由の生存フラグを取得 ---
    bool getNetworkAlive() const { return m_netIsAlive; }
};

} // namespace Game
