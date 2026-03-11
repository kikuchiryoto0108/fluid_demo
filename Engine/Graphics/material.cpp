//==============================================================================
//  File   : material.cpp
//  Brief  : マテリアル - シェーダーに渡すマテリアルデータ管理実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "material.h"

namespace Engine {

    //==========================================================
    // コンストラクタ
    //==========================================================
    Material::Material() {
    }

    //==========================================================
    // デストラクタ
    //==========================================================
    Material::~Material() {
        // 所有しているテクスチャのみ解放
        if (m_ownsTexture && m_pTexture) {
            m_pTexture->Release();
            m_pTexture = nullptr;
        }
    }

    //==========================================================
    // ムーブコンストラクタ
    //==========================================================
    Material::Material(Material&& other) noexcept
        : m_data(other.m_data)
        , m_pTexture(other.m_pTexture)
        , m_ownsTexture(other.m_ownsTexture) {
        other.m_pTexture = nullptr;
        other.m_ownsTexture = false;
    }

    //==========================================================
    // ムーブ代入演算子
    //==========================================================
    Material& Material::operator=(Material&& other) noexcept {
        if (this != &other) {
            // 既存リソースを解放
            if (m_ownsTexture && m_pTexture) {
                m_pTexture->Release();
            }

            m_data = other.m_data;
            m_pTexture = other.m_pTexture;
            m_ownsTexture = other.m_ownsTexture;

            other.m_pTexture = nullptr;
            other.m_ownsTexture = false;
        }
        return *this;
    }

    //==========================================================
    // プロパティ設定
    //==========================================================
    void Material::SetDiffuse(const XMFLOAT4& color) {
        m_data.diffuse = color;
    }

    void Material::SetAmbient(const XMFLOAT4& color) {
        m_data.ambient = color;
    }

    void Material::SetSpecular(const XMFLOAT4& color, float shininess) {
        m_data.specular = color;
        m_data.shininess = shininess;
    }

    void Material::SetEmission(const XMFLOAT4& color) {
        m_data.emission = color;
    }

    //==========================================================
    // テクスチャ設定
    //==========================================================
    void Material::SetTexture(ID3D11ShaderResourceView* pTexture) {
        if (m_ownsTexture && m_pTexture) {
            m_pTexture->Release();
        }
        m_pTexture = pTexture;
        m_ownsTexture = false;  // 外部から受け取ったので所有しない
    }

    //==========================================================
    // GPU適用 - 定数バッファとテクスチャをバインド
    //==========================================================
    void Material::Apply(ID3D11DeviceContext* pContext, ID3D11Buffer* pMaterialBuffer) {
        if (!pContext) return;

        // --- マテリアル定数バッファ更新 ---
        if (pMaterialBuffer) {
            pContext->UpdateSubresource(pMaterialBuffer, 0, nullptr, &m_data, 0, 0);
        }

        // --- テクスチャバインド ---
        if (m_pTexture) {
            pContext->PSSetShaderResources(0, 1, &m_pTexture);
        }
    }

    //==========================================================
    // デフォルトマテリアル生成
    //==========================================================
    std::shared_ptr<Material> Material::CreateDefault() {
        auto pMaterial = std::make_shared<Material>();
        pMaterial->SetDiffuse({ 1.0f, 1.0f, 1.0f, 1.0f });
        return pMaterial;
    }
}
