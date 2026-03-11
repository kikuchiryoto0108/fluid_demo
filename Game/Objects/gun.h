//==============================================================================
//  File   : gun.h
//  Brief  : 銃クラス - 3Dモデルの読み込みとプレイヤー追従
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/11
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "game_object.h"
#include "Engine/Graphics/mesh.h"
#include "Engine/Graphics/mesh_factory.h"
#include "Engine/Graphics/model_data.h"
#include <DirectXMath.h>
#include <memory>
#include <vector>

using namespace DirectX;

namespace Game {

class Player;

//==========================================================
// 銃クラス
//==========================================================
class Gun {
public:
    Gun();
    ~Gun();

    // --- ファイルから銃モデルを読み込む ---
    // textureがnullptrの場合、モデルファイルからテクスチャパスを自動取得
    bool LoadFromFile(const std::string& filePath, ID3D11ShaderResourceView* texture = nullptr);

    // --- 毎フレーム呼んでプレイヤーに追従させる ---
    void Update(const Player* owner, float deltaTime);

    // --- 描画 ---
    void Draw();

    // --- オフセット設定（プレイヤーからの相対位置） ---
    void SetOffset(const XMFLOAT3& offset) { m_offset = offset; }
    XMFLOAT3 GetOffset() const { return m_offset; }

    // --- スケール設定 ---
    void SetScale(const XMFLOAT3& scale);
    XMFLOAT3 GetScale() const { return m_visualObject.scale; }

    // --- 回転オフセット（銃自体の向き調整） ---
    void SetRotationOffset(const XMFLOAT3& rotOffset) { m_rotationOffset = rotOffset; }

    // --- GameObjectへのアクセス ---
    GameObject* GetGameObject() { return &m_visualObject; }

private:
    GameObject m_visualObject;
    Engine::ModelData m_modelData;  // モデルデータ（メッシュ + テクスチャ）

    // --- プレイヤーからの相対位置（前方、上方、右方） ---
    XMFLOAT3 m_offset;
    
    // --- 銃自体の回転オフセット ---
    XMFLOAT3 m_rotationOffset;
};

} // namespace Game
