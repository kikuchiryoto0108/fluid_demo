//==============================================================================
//  File   : destructible_wall.cpp
//  Brief  : 破壊可能な壁の実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "destructible_wall.h"
#include "Engine/Graphics/mesh_factory.h"
#include "Engine/Core/renderer.h"
#include "Engine/Core/debug_log.h"
#include "Engine/Graphics/material.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

using Engine::DebugLog;

namespace Game {

    //==========================================================
    // コンストラクタ
    // GameObjectの基底クラスを初期化
    //==========================================================
    DestructibleWall::DestructibleWall() : GameObject() {
        // タグを設定（マップブロックとして扱う）
        SetTag(ObjectTag::MAP_BLOCK);
    }

    //==========================================================
    // デストラクタ
    //==========================================================
    DestructibleWall::~DestructibleWall() {
        m_modelData.ReleaseTextures();
    }

    //==========================================================
    // ファイルから読み込み
    //==========================================================
    bool DestructibleWall::LoadFromFile(const std::string& filepath, ID3D11Device* pDevice) {
        DebugLog("[DestructibleWall] Loading: %s\n", filepath.c_str());

        if (!pDevice) {
            DebugLog("[DestructibleWall] FAILED: pDevice is null\n");
            return false;
        }

        // モデルデータを読み込む
        m_modelData = Engine::MeshFactory::LoadModelWithTextures(filepath, pDevice);

        if (m_modelData.subMeshes.empty()) {
            DebugLog("[DestructibleWall] FAILED: No meshes loaded\n");
            return false;
        }

        DebugLog("[DestructibleWall] Loaded %d fragments\n", (int)m_modelData.subMeshes.size());

        // 各サブメッシュを破片として登録
        m_fragments.clear();
        m_fragments.reserve(m_modelData.subMeshes.size());

        for (size_t i = 0; i < m_modelData.subMeshes.size(); i++) {
            auto& subMesh = m_modelData.subMeshes[i];
            if (!subMesh.mesh) continue;

            WallFragment frag;
            frag.mesh = subMesh.mesh;
            frag.texture = subMesh.texture;
            frag.state = FragmentState::FROZEN;
            frag.scale = { 1.0f, 1.0f, 1.0f };

            // メッシュの境界を計算
            CalculateFragmentBounds(frag);

            // 初期位置はメッシュの中心
            frag.position = frag.center;
            frag.initialPosition = frag.position;
            frag.rotation = { 0, 0, 0 };
            frag.initialRotation = { 0, 0, 0 };

            // コライダーを設定
            XMFLOAT3 size = {
                frag.boundsMax.x - frag.boundsMin.x,
                frag.boundsMax.y - frag.boundsMin.y,
                frag.boundsMax.z - frag.boundsMin.z
            };
            frag.collider.SetCenter(frag.position);
            frag.collider.SetSize(size);

            DebugLog("[DestructibleWall] Fragment[%d]: center=(%.2f,%.2f,%.2f) size=(%.2f,%.2f,%.2f)\n",
                (int)i, frag.center.x, frag.center.y, frag.center.z,
                size.x, size.y, size.z);

            m_fragments.push_back(std::move(frag));
        }

        DebugLog("[DestructibleWall] SUCCESS: %d fragments ready\n", (int)m_fragments.size());

        // 全体のバウンディングボックスからBoxColliderを設定
        UpdateOverallCollider();

        return true;
    }

    //==========================================================
    // 全体のバウンディングボックスを計算してコライダーを更新
    // 全破片を包含するボックスコライダーを作成
    //==========================================================
    void DestructibleWall::UpdateOverallCollider() {
        if (m_fragments.empty()) return;

        // 全破片の境界を計算
        XMFLOAT3 overallMin = { FLT_MAX, FLT_MAX, FLT_MAX };
        XMFLOAT3 overallMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (const auto& frag : m_fragments) {
            // FROZENの破片のみを考慮
            if (frag.state != FragmentState::FROZEN) continue;

            // ワールド座標での破片の境界を計算
            XMFLOAT3 fragWorldPos = {
                frag.initialPosition.x + position.x,
                frag.initialPosition.y + position.y,
                frag.initialPosition.z + position.z
            };

            XMFLOAT3 halfSize = {
                (frag.boundsMax.x - frag.boundsMin.x) * 0.5f,
                (frag.boundsMax.y - frag.boundsMin.y) * 0.5f,
                (frag.boundsMax.z - frag.boundsMin.z) * 0.5f
            };

            // 最小・最大点を更新
            overallMin.x = (std::min)(overallMin.x, fragWorldPos.x - halfSize.x);
            overallMin.y = (std::min)(overallMin.y, fragWorldPos.y - halfSize.y);
            overallMin.z = (std::min)(overallMin.z, fragWorldPos.z - halfSize.z);
            overallMax.x = (std::max)(overallMax.x, fragWorldPos.x + halfSize.x);
            overallMax.y = (std::max)(overallMax.y, fragWorldPos.y + halfSize.y);
            overallMax.z = (std::max)(overallMax.z, fragWorldPos.z + halfSize.z);
        }

        // 有効な境界があればコライダーを設定
        if (overallMin.x < overallMax.x) {
            XMFLOAT3 colliderSize = {
                overallMax.x - overallMin.x,
                overallMax.y - overallMin.y,
                overallMax.z - overallMin.z
            };

            // BoxColliderを設定（GameObjectの機能を使用）
            setBoxCollider(colliderSize);

            DebugLog("[DestructibleWall] Overall collider: size=(%.2f,%.2f,%.2f)\n",
                colliderSize.x, colliderSize.y, colliderSize.z);
        }
    }

    //==========================================================
    // 破片のバウンディングボックスを計算
    //==========================================================
    void DestructibleWall::CalculateFragmentBounds(WallFragment& frag) {
        if (!frag.mesh) return;

        frag.mesh->GetBounds(frag.boundsMin, frag.boundsMax);

        // 中心を計算
        frag.center.x = (frag.boundsMin.x + frag.boundsMax.x) * 0.5f;
        frag.center.y = (frag.boundsMin.y + frag.boundsMax.y) * 0.5f;
        frag.center.z = (frag.boundsMin.z + frag.boundsMax.z) * 0.5f;
    }

    //==========================================================
    // 位置設定
    // GameObjectのpositionを使用して壁全体の位置を設定
    //==========================================================
    void DestructibleWall::SetWallPosition(const XMFLOAT3& pos) {
        XMFLOAT3 delta = {
            pos.x - position.x,
            pos.y - position.y,
            pos.z - position.z
        };
        
        // GameObjectの位置を更新
        setPosition(pos);

        // 全破片の位置を更新
        for (auto& frag : m_fragments) {
            frag.position.x += delta.x;
            frag.position.y += delta.y;
            frag.position.z += delta.z;
            frag.initialPosition.x += delta.x;
            frag.initialPosition.y += delta.y;
            frag.initialPosition.z += delta.z;
            frag.collider.SetCenter(frag.position);
        }

        // コライダーを更新
        UpdateOverallCollider();
    }

    //==========================================================
    // 回転設定
    // GameObjectのrotationを使用
    //==========================================================
    void DestructibleWall::SetWallRotation(const XMFLOAT3& rot) {
        setRotation(rot);
        // 必要に応じて破片の位置を回転
    }

    //==========================================================
    // スケール設定
    // GameObjectのscaleを使用して壁全体のスケールを設定
    //==========================================================
    void DestructibleWall::SetWallScale(const XMFLOAT3& scl) {
        scale = scl;
        for (auto& frag : m_fragments) {
            frag.scale = scl;
        }
        
        // コライダーを更新
        UpdateOverallCollider();
    }

    //==========================================================
    // 更新処理
    //==========================================================
    void DestructibleWall::Update(float deltaTime) {
        for (auto& frag : m_fragments) {
            if (frag.state == FragmentState::FALLING) {
                UpdateFragment(frag, deltaTime);
            }
        }
    }

    //==========================================================
    // 破片の物理更新
    //==========================================================
    void DestructibleWall::UpdateFragment(WallFragment& frag, float deltaTime) {
        // 重力
        frag.velocity.y += GRAVITY * deltaTime;

        // 速度制限
        if (frag.velocity.y < -50.0f) frag.velocity.y = -50.0f;

        // 位置更新
        frag.position.x += frag.velocity.x * deltaTime;
        frag.position.y += frag.velocity.y * deltaTime;
        frag.position.z += frag.velocity.z * deltaTime;

        // 回転更新
        frag.rotation.x += frag.angularVelocity.x * deltaTime;
        frag.rotation.y += frag.angularVelocity.y * deltaTime;
        frag.rotation.z += frag.angularVelocity.z * deltaTime;

        // 地面との衝突
        float halfHeight = (frag.boundsMax.y - frag.boundsMin.y) * frag.scale.y * 0.5f;
        if (frag.position.y - halfHeight < m_groundY) {
            frag.position.y = GROUND_Y + halfHeight;

            // 跳ね返り
            if (frag.velocity.y < -1.0f) {
                frag.velocity.y = -frag.velocity.y * BOUNCE_FACTOR;
                frag.velocity.x *= FRICTION;
                frag.velocity.z *= FRICTION;
                frag.angularVelocity.x *= FRICTION;
                frag.angularVelocity.y *= FRICTION;
                frag.angularVelocity.z *= FRICTION;
            } else {
                // 静止
                frag.velocity = { 0, 0, 0 };
                frag.angularVelocity = { 0, 0, 0 };
                frag.state = FragmentState::RESTING;
            }
        }

        // コライダー更新
        frag.collider.SetCenter(frag.position);

        // 寿命
        frag.lifetime += deltaTime;
        if (frag.lifetime > WallFragment::MAX_LIFETIME) {
            frag.state = FragmentState::DESTROYED;
        }
    }

    //==========================================================
    // 描画（GameObjectのdraw()をオーバーライド）
    // 各破片を個別に描画
    //==========================================================
    void DestructibleWall::draw() {
        auto* ctx = Engine::Renderer::GetInstance().GetContext();
        if (!ctx) return;

        for (const auto& frag : m_fragments) {
            if (frag.state == FragmentState::DESTROYED) continue;
            if (!frag.mesh || !frag.mesh->IsUploaded()) continue;

            // ワールド行列を計算
            // 破片の中心からのオフセットを考慮
            XMMATRIX S = XMMatrixScaling(frag.scale.x, frag.scale.y, frag.scale.z);
            XMMATRIX R = XMMatrixRotationRollPitchYaw(
                XMConvertToRadians(frag.rotation.x),
                XMConvertToRadians(frag.rotation.y),
                XMConvertToRadians(frag.rotation.z)
            );

            // FROZENの場合は初期位置のまま描画
            XMFLOAT3 drawPos = frag.position;
            if (frag.state == FragmentState::FROZEN) {
                // 壁全体のトランスフォームを適用（GameObjectのpositionを使用）
                drawPos.x = frag.initialPosition.x + position.x;
                drawPos.y = frag.initialPosition.y + position.y;
                drawPos.z = frag.initialPosition.z + position.z;
            }

            // メッシュの中心からオフセット（メッシュは原点基準）
            XMMATRIX T = XMMatrixTranslation(drawPos.x, drawPos.y, drawPos.z);

            // 破片はメッシュの中心を基準に描画する必要がある
            // メッシュのローカル原点を中心に移動
            XMMATRIX centerOffset = XMMatrixTranslation(-frag.center.x, -frag.center.y, -frag.center.z);

            XMMATRIX world = centerOffset * S * R * T;
            Engine::Renderer::GetInstance().SetWorldMatrix(world);

            // テクスチャ設定
            if (frag.texture) {
                ctx->PSSetShaderResources(0, 1, &frag.texture);
            }

            // マテリアル設定
            Engine::MaterialData mat = {};
            mat.diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
            auto* buf = Engine::Renderer::GetInstance().GetMaterialBuffer();
            if (buf) {
                ctx->UpdateSubresource(buf, 0, nullptr, &mat, 0, 0);
            }

            // 描画
            frag.mesh->Draw(ctx);
        }
    }

    //==========================================================
    // 指定位置を中心に破片を落下させる
    //==========================================================
    int DestructibleWall::BreakAtPoint(const XMFLOAT3& hitPoint, float radius) {
        int brokenCount = 0;
        float radiusSq = radius * radius;

        DebugLog("[DestructibleWall::BreakAtPoint] hit=(%.2f,%.2f,%.2f) radius=%.2f\n",
            hitPoint.x, hitPoint.y, hitPoint.z, radius);

        for (auto& frag : m_fragments) {
            if (frag.state != FragmentState::FROZEN) continue;

            // ワールド座標での破片位置（GameObjectのpositionを使用）
            XMFLOAT3 fragWorldPos = {
                frag.initialPosition.x + position.x,
                frag.initialPosition.y + position.y,
                frag.initialPosition.z + position.z
            };

            float distSq = DistanceSquared(fragWorldPos, hitPoint);

            if (distSq <= radiusSq) {
                // 落下開始
                frag.state = FragmentState::FALLING;
                frag.position = fragWorldPos;
                frag.lifetime = 0.0f;

                // 衝突点から外側に飛ばす
                XMFLOAT3 dir = {
                    fragWorldPos.x - hitPoint.x,
                    fragWorldPos.y - hitPoint.y,
                    fragWorldPos.z - hitPoint.z
                };

                float dist = sqrtf(distSq);
                if (dist > 0.001f) {
                    dir.x /= dist;
                    dir.y /= dist;
                    dir.z /= dist;
                } else {
                    dir = { 0, 1, 0 };
                }

                // 距離に応じた初速（近いほど強く）
                float power = (1.0f - dist / radius) * 8.0f + 2.0f;
                frag.velocity.x = dir.x * power + ((rand() % 100 - 50) * 0.02f);
                frag.velocity.y = dir.y * power + 5.0f;
                frag.velocity.z = dir.z * power + ((rand() % 100 - 50) * 0.02f);

                // ランダム回転
                frag.angularVelocity.x = (rand() % 200 - 100) * 2.0f;
                frag.angularVelocity.y = (rand() % 200 - 100) * 2.0f;
                frag.angularVelocity.z = (rand() % 200 - 100) * 2.0f;

                brokenCount++;
                DebugLog("[DestructibleWall] Fragment broken: pos=(%.2f,%.2f,%.2f)\n",
                    fragWorldPos.x, fragWorldPos.y, fragWorldPos.z);
            }
        }

        // 破壊後にコライダーを更新
        if (brokenCount > 0) {
            UpdateOverallCollider();
        }

        DebugLog("[DestructibleWall::BreakAtPoint] %d fragments broken\n", brokenCount);
        return brokenCount;
    }

    //==========================================================
    // 弾との衝突判定
    //==========================================================
    bool DestructibleWall::CheckBulletHit(const XMFLOAT3& bulletPos, float bulletRadius, XMFLOAT3& outHitPoint) {
        float closestDistSq = FLT_MAX;
        bool hit = false;

        for (const auto& frag : m_fragments) {
            if (frag.state != FragmentState::FROZEN) continue;

            // ワールド座標での破片位置（GameObjectのpositionを使用）
            XMFLOAT3 fragWorldPos = {
                frag.initialPosition.x + position.x,
                frag.initialPosition.y + position.y,
                frag.initialPosition.z + position.z
            };

            // コライダーの位置を更新して衝突判定
            Engine::BoxCollider tempCollider = frag.collider;
            tempCollider.SetCenter(fragWorldPos);

            // 簡易的な球-AABB判定
            XMFLOAT3 bmin, bmax;
            tempCollider.GetBounds(bmin, bmax);

            // 最近点を計算
            float closestX = std::clamp(bulletPos.x, bmin.x, bmax.x);
            float closestY = std::clamp(bulletPos.y, bmin.y, bmax.y);
            float closestZ = std::clamp(bulletPos.z, bmin.z, bmax.z);

            float dx = bulletPos.x - closestX;
            float dy = bulletPos.y - closestY;
            float dz = bulletPos.z - closestZ;
            float distSq = dx * dx + dy * dy + dz * dz;

            if (distSq <= bulletRadius * bulletRadius) {
                if (distSq < closestDistSq) {
                    closestDistSq = distSq;
                    outHitPoint = fragWorldPos;
                    hit = true;
                }
            }
        }

        return hit;
    }

    //==========================================================
    // リセット
    //==========================================================
    void DestructibleWall::Reset() {
        for (auto& frag : m_fragments) {
            frag.position = frag.initialPosition;
            frag.rotation = frag.initialRotation;
            frag.velocity = { 0, 0, 0 };
            frag.angularVelocity = { 0, 0, 0 };
            frag.state = FragmentState::FROZEN;
            frag.lifetime = 0.0f;
            frag.collider.SetCenter(frag.position);
        }
    }

    //==========================================================
    // 状態カウント
    //==========================================================
    size_t DestructibleWall::GetFrozenCount() const {
        return std::count_if(m_fragments.begin(), m_fragments.end(),
            [](const WallFragment& f) { return f.state == FragmentState::FROZEN; });
    }

    size_t DestructibleWall::GetFallingCount() const {
        return std::count_if(m_fragments.begin(), m_fragments.end(),
            [](const WallFragment& f) { return f.state == FragmentState::FALLING; });
    }

    bool DestructibleWall::IsFullyDestroyed() const {
        for (const auto& frag : m_fragments) {
            if (frag.state == FragmentState::FROZEN) return false;
        }
        return true;
    }

    //==========================================================
    // 距離の2乗
    //==========================================================
    float DestructibleWall::DistanceSquared(const XMFLOAT3& a, const XMFLOAT3& b) const {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    //==========================================================
    // ローカル座標をワールド座標に変換
    // GameObjectのscale、positionを使用
    //==========================================================
    XMFLOAT3 DestructibleWall::TransformPoint(const XMFLOAT3& local) const {
        return {
            local.x * scale.x + position.x,
            local.y * scale.y + position.y,
            local.z * scale.z + position.z
        };
    }

} // namespace Game
