//==============================================================================
//  File   : gun.cpp
//  Brief  : 銃クラス - 3Dモデルの読み込みとプレイヤー追従
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "gun.h"
#include "player.h"
#include "Engine/Core/renderer.h"
#include "Engine/Core/debug_log.h"
#include <cmath>

// Engine名前空間のDebugLogを使用
using Engine::DebugLog;

namespace Game {

//==========================================================
// コンストラクタ
//==========================================================
Gun::Gun()
    : m_offset(0.3f, 0.9f, 0.5f)         // デフォルト: 右前方、目の高さあたり
    , m_rotationOffset(0.0f, 0.0f, 0.0f) {
    DebugLog("[Gun::Gun] Constructor called\n");
}

//==========================================================
// デストラクタ
//==========================================================
Gun::~Gun() {
    DebugLog("[Gun::~Gun] Destructor called, releasing textures\n");
    // テクスチャを解放
    m_modelData.ReleaseTextures();
}

//==========================================================
// ファイルから銃モデルを読み込む
//==========================================================
bool Gun::LoadFromFile(const std::string& filePath, ID3D11ShaderResourceView* texture) {
    DebugLog("[Gun::LoadFromFile] START: %s\n", filePath.c_str());
    
    ID3D11Device* pDevice = Engine::GetDevice();
    if (!pDevice) {
        DebugLog("[Gun::LoadFromFile] FAILED: pDevice is null\n");
        return false;
    }
    
    // --- モデルデータを読み込む（テクスチャパス情報も取得） ---
    DebugLog("[Gun::LoadFromFile] Calling LoadModelWithTextures...\n");
    m_modelData = Engine::MeshFactory::LoadModelWithTextures(filePath, pDevice);
    
    if (m_modelData.subMeshes.empty()) {
        DebugLog("[Gun::LoadFromFile] FAILED: No subMeshes loaded\n");
        return false;
    }

    DebugLog("[Gun::LoadFromFile] Loaded %d subMeshes\n", (int)m_modelData.subMeshes.size());

    // --- メッシュリストを構築 ---
    std::vector<std::shared_ptr<Engine::Mesh>> meshes;
    meshes.reserve(m_modelData.subMeshes.size());
    for (size_t i = 0; i < m_modelData.subMeshes.size(); i++) {
        const auto& subMesh = m_modelData.subMeshes[i];
        if (subMesh.mesh) {
            DebugLog("[Gun::LoadFromFile] SubMesh[%d]: vertices=%d, indices=%d\n", 
                     (int)i, (int)subMesh.mesh->GetVertexCount(), (int)subMesh.mesh->GetIndexCount());
            meshes.push_back(subMesh.mesh);
        }
    }

    // --- 使用するテクスチャを決定 ---
    // 1. 外部から指定されたテクスチャがあればそれを使用
    // 2. なければモデルに埋め込まれているテクスチャを使用
    ID3D11ShaderResourceView* texToUse = texture;
    if (!texToUse) {
        DebugLog("[Gun::LoadFromFile] No external texture, searching in model...\n");
        // モデルから読み込まれた最初の有効なテクスチャを使用
        for (size_t i = 0; i < m_modelData.subMeshes.size(); i++) {
            const auto& subMesh = m_modelData.subMeshes[i];
            if (subMesh.texture) {
                DebugLog("[Gun::LoadFromFile] Using texture from SubMesh[%d]\n", (int)i);
                texToUse = subMesh.texture;
                break;
            }
        }
    } else {
        DebugLog("[Gun::LoadFromFile] Using external texture\n");
    }

    if (!texToUse) {
        DebugLog("[Gun::LoadFromFile] WARNING: No texture found for gun model\n");
    }

    // --- GameObjectにメッシュを設定 ---
    DebugLog("[Gun::LoadFromFile] Setting %d meshes to visual object\n", (int)meshes.size());
    m_visualObject.setMeshes(meshes, texToUse);
    
    // デフォルトのスケール（銃モデルに応じて調整が必要な場合がある）
    m_visualObject.scale = XMFLOAT3(0.02f, 0.02f, 0.02f);
    
    DebugLog("[Gun::LoadFromFile] SUCCESS\n");
    return true;
}

//==========================================================
// 更新処理（プレイヤー追従）
//==========================================================
void Gun::Update(const Player* owner, float deltaTime) {
    if (!owner) return;

    // --- プレイヤーの位置と回転を取得 ---
    XMFLOAT3 playerPos = owner->GetPosition();
    XMFLOAT3 playerRot = owner->GetRotation();

    // --- プレイヤーの向きをラジアンに変換 ---
    float yawRad = XMConvertToRadians(playerRot.y);

    // --- 前方・右方ベクトルを計算 ---
    XMFLOAT3 forward = { sinf(yawRad), 0.0f, cosf(yawRad) };
    XMFLOAT3 right = { cosf(yawRad), 0.0f, -sinf(yawRad) };

    // --- オフセットを適用して銃の位置を計算 ---
    // m_offset.x = 右方向, m_offset.y = 上方向, m_offset.z = 前方向
    XMFLOAT3 gunPos;
    gunPos.x = playerPos.x + right.x * m_offset.x + forward.x * m_offset.z;
    gunPos.y = playerPos.y + m_offset.y;
    gunPos.z = playerPos.z + right.z * m_offset.x + forward.z * m_offset.z;

    // --- 銃の回転（プレイヤーの向き + 回転オフセット） ---
    XMFLOAT3 gunRot;
    gunRot.x = playerRot.x + m_rotationOffset.x;
    gunRot.y = playerRot.y + m_rotationOffset.y;
    gunRot.z = playerRot.z + m_rotationOffset.z;

    // --- GameObjectを更新 ---
    m_visualObject.position = gunPos;
    m_visualObject.rotation = gunRot;
    m_visualObject.markBufferForUpdate();
}

//==========================================================
// スケール設定
//==========================================================
void Gun::SetScale(const XMFLOAT3& scale) {
    m_visualObject.scale = scale;
    m_visualObject.markBufferForUpdate();
}

//==========================================================
// 描画処理
//==========================================================
void Gun::Draw() {
    m_visualObject.draw();
}

} // namespace Game
