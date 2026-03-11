//==============================================================================
//  File   : mesh.cpp
//  Brief  : メッシュ - 頂点・インデックスデータとGPUバッファ管理実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "mesh.h"
#include <algorithm>
#include <limits>

namespace Engine {

    //==========================================================
    // コンストラクタ
    //==========================================================
    Mesh::Mesh() {
    }

    //==========================================================
    // デストラクタ
    //==========================================================
    Mesh::~Mesh() {
        ReleaseBuffers();
    }

    //==========================================================
    // ムーブコンストラクタ
    //==========================================================
    Mesh::Mesh(Mesh&& other) noexcept
        : m_vertices(std::move(other.m_vertices))
        , m_indices(std::move(other.m_indices))
        , m_pVertexBuffer(other.m_pVertexBuffer)
        , m_pIndexBuffer(other.m_pIndexBuffer)
        , m_boundsMin(other.m_boundsMin)
        , m_boundsMax(other.m_boundsMax)
        , m_boundsDirty(other.m_boundsDirty) {
        other.m_pVertexBuffer = nullptr;
        other.m_pIndexBuffer = nullptr;
    }

    //==========================================================
    // ムーブ代入演算子
    //==========================================================
    Mesh& Mesh::operator=(Mesh&& other) noexcept {
        if (this != &other) {
            ReleaseBuffers();

            m_vertices = std::move(other.m_vertices);
            m_indices = std::move(other.m_indices);
            m_pVertexBuffer = other.m_pVertexBuffer;
            m_pIndexBuffer = other.m_pIndexBuffer;
            m_boundsMin = other.m_boundsMin;
            m_boundsMax = other.m_boundsMax;
            m_boundsDirty = other.m_boundsDirty;

            other.m_pVertexBuffer = nullptr;
            other.m_pIndexBuffer = nullptr;
        }
        return *this;
    }

    //==========================================================
    // 頂点データ設定
    //==========================================================
    void Mesh::SetVertices(const std::vector<Vertex3D>& vertices) {
        m_vertices = vertices;
        m_boundsDirty = true;
    }

    void Mesh::SetVertices(std::vector<Vertex3D>&& vertices) {
        m_vertices = std::move(vertices);
        m_boundsDirty = true;
    }

    void Mesh::SetIndices(const std::vector<uint32_t>& indices) {
        m_indices = indices;
    }

    void Mesh::SetIndices(std::vector<uint32_t>&& indices) {
        m_indices = std::move(indices);
    }

    //==========================================================
    // GPUへアップロード - 頂点・インデックスバッファ作成
    //==========================================================
    bool Mesh::Upload(ID3D11Device* pDevice) {
        if (!pDevice || m_vertices.empty()) return false;

        ReleaseBuffers();

        // --- 頂点バッファ作成 ---
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex3D) * m_vertices.size());
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = m_vertices.data();

        HRESULT hr = pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer);
        if (FAILED(hr)) return false;

        // --- インデックスバッファ作成（あれば） ---
        if (!m_indices.empty()) {
            D3D11_BUFFER_DESC ibDesc = {};
            ibDesc.Usage = D3D11_USAGE_DEFAULT;
            ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * m_indices.size());
            ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA ibData = {};
            ibData.pSysMem = m_indices.data();

            hr = pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer);
            if (FAILED(hr)) {
                ReleaseBuffers();
                return false;
            }
        }

        return true;
    }

    //==========================================================
    // バインド - 入力アセンブラにバッファをセット
    //==========================================================
    void Mesh::Bind(ID3D11DeviceContext* pContext) {
        if (!pContext || !m_pVertexBuffer) return;

        UINT stride = sizeof(Vertex3D);
        UINT offset = 0;
        pContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

        if (m_pIndexBuffer) {
            pContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        }

        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    //==========================================================
    // 描画 - バインドして描画コール発行
    //==========================================================
    void Mesh::Draw(ID3D11DeviceContext* pContext) {
        if (!pContext || !m_pVertexBuffer) return;

        Bind(pContext);

        if (m_pIndexBuffer && !m_indices.empty()) {
            pContext->DrawIndexed(static_cast<UINT>(m_indices.size()), 0, 0);
        } else {
            pContext->Draw(static_cast<UINT>(m_vertices.size()), 0);
        }
    }

    //==========================================================
    // 境界ボックス取得
    //==========================================================
    void Mesh::GetBounds(XMFLOAT3& outMin, XMFLOAT3& outMax) const {
        if (m_boundsDirty) {
            const_cast<Mesh*>(this)->CalculateBounds();
        }
        outMin = m_boundsMin;
        outMax = m_boundsMax;
    }

    //==========================================================
    // 境界ボックス計算
    //==========================================================
    void Mesh::CalculateBounds() {
        if (m_vertices.empty()) {
            m_boundsMin = { 0.0f, 0.0f, 0.0f };
            m_boundsMax = { 0.0f, 0.0f, 0.0f };
            m_boundsDirty = false;
            return;
        }

        constexpr float maxFloat = std::numeric_limits<float>::max();
        constexpr float minFloat = std::numeric_limits<float>::lowest();

        m_boundsMin = { maxFloat, maxFloat, maxFloat };
        m_boundsMax = { minFloat, minFloat, minFloat };

        // 全頂点を走査して最小・最大を求める
        for (const auto& vertex : m_vertices) {
            if (vertex.position.x < m_boundsMin.x) m_boundsMin.x = vertex.position.x;
            if (vertex.position.y < m_boundsMin.y) m_boundsMin.y = vertex.position.y;
            if (vertex.position.z < m_boundsMin.z) m_boundsMin.z = vertex.position.z;

            if (vertex.position.x > m_boundsMax.x) m_boundsMax.x = vertex.position.x;
            if (vertex.position.y > m_boundsMax.y) m_boundsMax.y = vertex.position.y;
            if (vertex.position.z > m_boundsMax.z) m_boundsMax.z = vertex.position.z;
        }

        m_boundsDirty = false;
    }

    //==========================================================
    // GPUバッファ解放
    //==========================================================
    void Mesh::ReleaseBuffers() {
        if (m_pVertexBuffer) {
            m_pVertexBuffer->Release();
            m_pVertexBuffer = nullptr;
        }
        if (m_pIndexBuffer) {
            m_pIndexBuffer->Release();
            m_pIndexBuffer = nullptr;
        }
    }
}
