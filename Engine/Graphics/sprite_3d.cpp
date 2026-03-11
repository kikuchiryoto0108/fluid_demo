//==============================================================================
//  File   : sprite_3d.cpp
//  Brief  : 3Dスプライト - 3D空間に配置する板ポリゴン描画実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "sprite_3d.h"
#include "Engine/Core/renderer.h"
#include <cmath>

namespace Engine {
    ID3D11Buffer* Sprite3D::s_pVertexBuffer = nullptr;

    //==========================================================
    // 初期化 - 動的頂点バッファ作成
    //==========================================================
    bool Sprite3D::Initialize(ID3D11Device* pDevice) {
        if (!pDevice) return false;

        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(Vertex3D) * 4;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = pDevice->CreateBuffer(&bd, nullptr, &s_pVertexBuffer);
        return SUCCEEDED(hr);
    }

    //==========================================================
    // 終了 - リソース解放
    //==========================================================
    void Sprite3D::Finalize() {
        if (s_pVertexBuffer) {
            s_pVertexBuffer->Release();
            s_pVertexBuffer = nullptr;
        }
    }

    //==========================================================
    // 描画 - 3D空間に配置するスプライト
    //==========================================================
    void Sprite3D::Draw(
        ID3D11DeviceContext* pContext,
        ID3D11Buffer* pMaterialBuffer,
        const XMFLOAT3& position,
        const XMFLOAT2& size,
        const XMFLOAT4& color,
        ID3D11ShaderResourceView* pTexture,
        const XMFLOAT3& rotation) {
        if (!pContext || !s_pVertexBuffer) return;

        float hw = size.x * 0.5f;
        float hh = size.y * 0.5f;

        // --- ローカル頂点（XY平面） ---
        D3D11_MAPPED_SUBRESOURCE msr;
        if (SUCCEEDED(pContext->Map(s_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) {
            Vertex3D* vertices = static_cast<Vertex3D*>(msr.pData);

            vertices[0] = { { -hw,  hh, 0.0f }, { 0, 0, -1 }, color, { 0, 0 } };
            vertices[1] = { {  hw,  hh, 0.0f }, { 0, 0, -1 }, color, { 1, 0 } };
            vertices[2] = { { -hw, -hh, 0.0f }, { 0, 0, -1 }, color, { 0, 1 } };
            vertices[3] = { {  hw, -hh, 0.0f }, { 0, 0, -1 }, color, { 1, 1 } };

            pContext->Unmap(s_pVertexBuffer, 0);
        }

        // --- ワールド行列設定 ---
        XMMATRIX rotMatrix = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotation.x),
            XMConvertToRadians(rotation.y),
            XMConvertToRadians(rotation.z)
        );
        XMMATRIX transMatrix = XMMatrixTranslation(position.x, position.y, position.z);
        XMMATRIX worldMatrix = rotMatrix * transMatrix;

        Renderer::GetInstance().SetWorldMatrix(worldMatrix);

        // --- マテリアル設定 ---
        if (pMaterialBuffer) {
            MaterialData mat = {};
            mat.diffuse = { 1, 1, 1, 1 };
            pContext->UpdateSubresource(pMaterialBuffer, 0, nullptr, &mat, 0, 0);
        }

        // --- テクスチャ設定 ---
        if (pTexture) {
            pContext->PSSetShaderResources(0, 1, &pTexture);
        }

        // --- 描画 ---
        UINT stride = sizeof(Vertex3D);
        UINT offset = 0;
        pContext->IASetVertexBuffers(0, 1, &s_pVertexBuffer, &stride, &offset);
        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        pContext->Draw(4, 0);
    }

    //==========================================================
    // ビルボード描画 - 常にカメラを向く
    //==========================================================
    void Sprite3D::DrawBillboard(
        ID3D11DeviceContext* pContext,
        ID3D11Buffer* pMaterialBuffer,
        const XMFLOAT3& position,
        const XMFLOAT2& size,
        const XMFLOAT4& color,
        const XMMATRIX& viewMatrix,
        ID3D11ShaderResourceView* pTexture) {
        if (!pContext || !s_pVertexBuffer) return;

        float hw = size.x * 0.5f;
        float hh = size.y * 0.5f;

        // --- 頂点データ ---
        D3D11_MAPPED_SUBRESOURCE msr;
        if (SUCCEEDED(pContext->Map(s_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) {
            Vertex3D* vertices = static_cast<Vertex3D*>(msr.pData);

            vertices[0] = { { -hw,  hh, 0.0f }, { 0, 0, -1 }, color, { 0, 0 } };
            vertices[1] = { {  hw,  hh, 0.0f }, { 0, 0, -1 }, color, { 1, 0 } };
            vertices[2] = { { -hw, -hh, 0.0f }, { 0, 0, -1 }, color, { 0, 1 } };
            vertices[3] = { {  hw, -hh, 0.0f }, { 0, 0, -1 }, color, { 1, 1 } };

            pContext->Unmap(s_pVertexBuffer, 0);
        }

        // --- ビルボード行列：ビュー行列の逆回転を適用 ---
        XMMATRIX invView = XMMatrixInverse(nullptr, viewMatrix);

        // 平行移動成分を除去して回転成分だけを取得
        invView.r[3] = XMVectorSet(0, 0, 0, 1);

        XMMATRIX transMatrix = XMMatrixTranslation(position.x, position.y, position.z);
        XMMATRIX worldMatrix = invView * transMatrix;

        Renderer::GetInstance().SetWorldMatrix(worldMatrix);

        // --- マテリアル設定 ---
        if (pMaterialBuffer) {
            MaterialData mat = {};
            mat.diffuse = { 1, 1, 1, 1 };
            pContext->UpdateSubresource(pMaterialBuffer, 0, nullptr, &mat, 0, 0);
        }

        // --- テクスチャ設定 ---
        if (pTexture) {
            pContext->PSSetShaderResources(0, 1, &pTexture);
        }

        // --- 描画 ---
        UINT stride = sizeof(Vertex3D);
        UINT offset = 0;
        pContext->IASetVertexBuffers(0, 1, &s_pVertexBuffer, &stride, &offset);
        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        pContext->Draw(4, 0);
    }
}
