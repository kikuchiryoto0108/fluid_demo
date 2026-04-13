//==============================================================================
//  File   : fullscreen_quad.cpp
//==============================================================================
#include "pch.h"
#include "fullscreen_quad.h"

namespace Engine {

    struct QuadVertex {
        float x, y, z;
        float u, v;
    };

    bool FullscreenQuad::Initialize(ID3D11Device* device) {
        // 画面全体を覆う四角形（-1〜1）
        QuadVertex vertices[] = {
            { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },  // 左上
            {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },  // 右上
            { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },  // 左下
            {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },  // 右上
            {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },  // 右下
            { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },  // 左下
        };

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = sizeof(vertices);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = vertices;

        return SUCCEEDED(device->CreateBuffer(&bd, &initData, &m_pVertexBuffer));
    }

    void FullscreenQuad::Draw(ID3D11DeviceContext* context) {
        UINT stride = sizeof(QuadVertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->Draw(6, 0);
    }
}
