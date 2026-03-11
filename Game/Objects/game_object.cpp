//==============================================================================
//  File   : game_object.cpp
//  Brief  : ゲームオブジェクト - 3Dオブジェクトの描画と状態管理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "game_object.h"
#include <string>

namespace Game {

//==========================================================
// コンストラクタ
//==========================================================
GameObject::GameObject()
    : position(0.0f, 0.0f, 0.0f)
    , velocity(0.0f, 0.0f, 0.0f)
    , scale(1.0f, 1.0f, 1.0f)
    , rotation(0.0f, 0.0f, 0.0f)
    , boxCollider(nullptr)
    , meshVertices(nullptr)
    , meshVertexCount(0)
    , texture(nullptr)
    , vertexBuffer(nullptr)
    , bufferNeedsUpdate(true)
    , m_tag(ObjectTag::NONE)
    , m_active(true)
    , m_deleteFlag(false)
    , id(0)
    , m_worldMatrixDirty(true) {
}

//==========================================================
// デストラクタ
//==========================================================
GameObject::~GameObject() {
    if (vertexBuffer) {
        vertexBuffer->Release();
        vertexBuffer = nullptr;
    }
}

//==========================================================
// ムーブコンストラクタ
//==========================================================
GameObject::GameObject(GameObject&& other) noexcept
    : position(other.position)
    , velocity(other.velocity)
    , scale(other.scale)
    , rotation(other.rotation)
    , boxCollider(std::move(other.boxCollider))
    , meshVertices(other.meshVertices)
    , meshVertexCount(other.meshVertexCount)
    , texture(other.texture)
    , vertexBuffer(other.vertexBuffer)
    , bufferNeedsUpdate(other.bufferNeedsUpdate)
    , m_tag(other.m_tag)
    , m_active(other.m_active)
    , m_deleteFlag(other.m_deleteFlag)
    , id(other.id)
    , m_cachedWorldMatrix(other.m_cachedWorldMatrix)
    , m_worldMatrixDirty(other.m_worldMatrixDirty) {
    other.vertexBuffer = nullptr;
    other.meshVertices = nullptr;
    other.texture = nullptr;
}

//==========================================================
// ムーブ代入演算子
//==========================================================
GameObject& GameObject::operator=(GameObject&& other) noexcept {
    if (this != &other) {
        if (vertexBuffer) vertexBuffer->Release();

        position = other.position;
        velocity = other.velocity;
        scale = other.scale;
        rotation = other.rotation;
        boxCollider = std::move(other.boxCollider);
        meshVertices = other.meshVertices;
        meshVertexCount = other.meshVertexCount;
        texture = other.texture;
        vertexBuffer = other.vertexBuffer;
        bufferNeedsUpdate = other.bufferNeedsUpdate;
        m_tag = other.m_tag;
        m_active = other.m_active;
        m_deleteFlag = other.m_deleteFlag;
        id = other.id;
        m_cachedWorldMatrix = other.m_cachedWorldMatrix;
        m_worldMatrixDirty = other.m_worldMatrixDirty;

        other.vertexBuffer = nullptr;
        other.meshVertices = nullptr;
        other.texture = nullptr;
    }
    return *this;
}

//==========================================================
// 位置設定
//==========================================================
void GameObject::setPosition(const XMFLOAT3& p) {
    position = p;
    m_worldMatrixDirty = true;
    bufferNeedsUpdate = true;
    updateColliderTransform();
}

//==========================================================
// 回転設定
//==========================================================
void GameObject::setRotation(const XMFLOAT3& r) {
    rotation = r;
    m_worldMatrixDirty = true;
    bufferNeedsUpdate = true;
    updateColliderTransform();
}

//==========================================================
// コライダートランスフォーム更新
//==========================================================
void GameObject::updateColliderTransform() {
    if (boxCollider) {
        boxCollider->SetTransform(position, rotation, scale);
    }
}

//==========================================================
// ボックスコライダー設定
//==========================================================
void GameObject::setBoxCollider(const XMFLOAT3& size) {
    boxCollider = std::make_unique<Engine::BoxCollider>(position, size);
    boxCollider->SetTransform(position, rotation, scale);
}

//==========================================================
// メッシュ設定（旧式: 頂点配列）
//==========================================================
void GameObject::setMesh(const Engine::Vertex3D* vertices, int count, ID3D11ShaderResourceView* tex) {
    meshVertices = vertices;
    meshVertexCount = count;
    texture = tex;
    bufferNeedsUpdate = true;
}

//==========================================================
// メッシュ設定（新式: 単一Mesh）
//==========================================================
void GameObject::setMesh(std::shared_ptr<Engine::Mesh> mesh, ID3D11ShaderResourceView* tex) {
    m_meshes.clear();
    if (mesh) {
        m_meshes.push_back(mesh);
        mesh->Upload(Engine::GetDevice());
    }
    texture = tex;
    meshVertices = nullptr;
    meshVertexCount = 0;
}

//==========================================================
// メッシュ設定（新式: 複数Mesh）
//==========================================================
void GameObject::setMeshes(const std::vector<std::shared_ptr<Engine::Mesh>>& meshes, ID3D11ShaderResourceView* tex) {
    m_meshes = meshes;
    for (auto& mesh : m_meshes) {
        if (mesh) {
            mesh->Upload(Engine::GetDevice());
        }
    }
    texture = tex;
    meshVertices = nullptr;
    meshVertexCount = 0;
}

//==========================================================
// 頂点バッファ作成
//==========================================================
void GameObject::createVertexBuffer() {
    if (vertexBuffer) {
        vertexBuffer->Release();
        vertexBuffer = nullptr;
    }

    if (!meshVertices || meshVertexCount == 0) return;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Engine::Vertex3D) * meshVertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA subResource = {};
    subResource.pSysMem = meshVertices;

    Engine::GetDevice()->CreateBuffer(&bd, &subResource, &vertexBuffer);
}

//==========================================================
// 描画処理
//==========================================================
void GameObject::draw() {
    // 旧式頂点配列または新式Meshオブジェクトがあるかチェック
    bool hasOldStyle = (meshVertices && meshVertexCount > 0);
    bool hasNewStyle = !m_meshes.empty();
    
    if (!hasOldStyle && !hasNewStyle) return;

    // --- ワールド行列の更新（必要な場合のみ） ---
    if (m_worldMatrixDirty) {
        XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotation.x),
            XMConvertToRadians(rotation.y),
            XMConvertToRadians(rotation.z));
        XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
        m_cachedWorldMatrix = S * R * T;
        m_worldMatrixDirty = false;
    }

    Engine::Renderer::GetInstance().SetWorldMatrix(m_cachedWorldMatrix);

    if (texture) {
        Engine::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);
    }

    Engine::MaterialData mat = {};
    mat.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    auto* ctx = Engine::Renderer::GetInstance().GetContext();
    auto* buf = Engine::Renderer::GetInstance().GetMaterialBuffer();
    if (ctx && buf) {
        ctx->UpdateSubresource(buf, 0, nullptr, &mat, 0, 0);
    }

    // --- 新式Meshクラスで描画 ---
    if (hasNewStyle) {
        for (size_t i = 0; i < m_meshes.size(); i++) {
            auto& mesh = m_meshes[i];
            if (mesh && mesh->IsUploaded()) {
                ID3D11ShaderResourceView* meshTex = mesh->GetTexture();

                // 最初の1回だけログを出す
                static bool once = false;
                if (!once) {
                    char buf[512];
                    sprintf_s(buf, "[GameObject::draw] mesh[%d] meshTex=%p fallbackTex=%p\n",
                        (int)i, meshTex, texture);
                    OutputDebugStringA(buf);
                    once = true;
                }

                if (meshTex) {
                    Engine::GetDeviceContext()->PSSetShaderResources(0, 1, &meshTex);
                } else if (texture) {
                    Engine::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);
                }
                mesh->Draw(ctx);
            }
        }
    }
    // --- 旧式頂点配列で描画 ---
    else if (hasOldStyle) {
        if (bufferNeedsUpdate || !vertexBuffer) {
            createVertexBuffer();
            bufferNeedsUpdate = false;
        }

        if (!vertexBuffer) return;

        UINT stride = sizeof(Engine::Vertex3D);
        UINT offset = 0;
        Engine::GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        Engine::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        Engine::GetDeviceContext()->Draw(meshVertexCount, 0);
    }
}

//==========================================================
// 移動処理
//==========================================================
void GameObject::Move(const XMFLOAT3& direction, float speed, float deltaTime) {
    position.x += direction.x * speed * deltaTime;
    position.y += direction.y * speed * deltaTime;
    position.z += direction.z * speed * deltaTime;
    m_worldMatrixDirty = true;
    bufferNeedsUpdate = true;
    updateColliderTransform();
}

//==========================================================
// ネットワーク補間ターゲット設定
//==========================================================
void GameObject::setNetworkTarget(const XMFLOAT3& targetPos, const XMFLOAT3& targetRot, bool isAlive) {
    m_netTargetPos = targetPos;
    m_netTargetRot = targetRot;
    m_netIsAlive = isAlive;
    m_hasNetTarget = true;
}

//==========================================================
// ネットワーク補間更新
//==========================================================
void GameObject::updateNetworkInterpolation(float lerpFactor) {
    if (!m_hasNetTarget) return;

    // --- 位置を補間 ---
    position.x += (m_netTargetPos.x - position.x) * lerpFactor;
    position.y += (m_netTargetPos.y - position.y) * lerpFactor;
    position.z += (m_netTargetPos.z - position.z) * lerpFactor;

    // --- 回転を補間 ---
    rotation.x += (m_netTargetRot.x - rotation.x) * lerpFactor;
    rotation.y += (m_netTargetRot.y - rotation.y) * lerpFactor;
    rotation.z += (m_netTargetRot.z - rotation.z) * lerpFactor;

    m_worldMatrixDirty = true;
    bufferNeedsUpdate = true;
    updateColliderTransform();

    // --- 補間完了チェック（位置が十分近づいたら） ---
    float dx = m_netTargetPos.x - position.x;
    float dy = m_netTargetPos.y - position.y;
    float dz = m_netTargetPos.z - position.z;
    if ((dx * dx + dy * dy + dz * dz) < 0.0001f) {
        // 位置は到達したが、フラグはリセットしない
        // 次のネットワーク更新が来るまで現在の状態を維持
    }
}

} // namespace Game
