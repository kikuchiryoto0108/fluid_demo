//==============================================================================
//  File   : material.h
//  Brief  : マテリアル - シェーダーに渡すマテリアルデータ管理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include <string>

namespace Engine {
    using namespace DirectX;

    //==========================================================
    // マテリアルデータ構造体（GPU定数バッファ用）
    //==========================================================
    struct MaterialData {
        XMFLOAT4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };   // ディフューズカラー
        XMFLOAT4 ambient = { 0.2f, 0.2f, 0.2f, 1.0f };   // アンビエントカラー
        XMFLOAT4 specular = { 0.0f, 0.0f, 0.0f, 1.0f };  // スペキュラカラー
        XMFLOAT4 emission = { 0.0f, 0.0f, 0.0f, 1.0f };  // エミッションカラー
        float shininess = 0.0f;                          // 光沢度
        float padding[3] = { 0.0f, 0.0f, 0.0f };         // 16byte境界パディング
    };

    //==========================================================
    // マテリアルクラス
    //==========================================================
    class Material {
    public:
        Material();
        ~Material();

        // --- コピー禁止、ムーブ許可 ---
        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;
        Material(Material&& other) noexcept;
        Material& operator=(Material&& other) noexcept;

        // --- プロパティ設定 ---
        void SetDiffuse(const XMFLOAT4& color);
        void SetAmbient(const XMFLOAT4& color);
        void SetSpecular(const XMFLOAT4& color, float shininess);
        void SetEmission(const XMFLOAT4& color);

        // --- テクスチャ設定 ---
        void SetTexture(ID3D11ShaderResourceView* pTexture);
        ID3D11ShaderResourceView* GetTexture() const { return m_pTexture; }

        // --- GPU適用 ---
        void Apply(ID3D11DeviceContext* pContext, ID3D11Buffer* pMaterialBuffer);

        // --- アクセサ ---
        const MaterialData& GetData() const { return m_data; }

        // --- ファクトリ ---
        static std::shared_ptr<Material> CreateDefault();

    private:
        MaterialData m_data;                             // マテリアルデータ
        ID3D11ShaderResourceView* m_pTexture = nullptr;  // テクスチャリソース
        bool m_ownsTexture = false;                      // テクスチャ所有フラグ
    };
}
