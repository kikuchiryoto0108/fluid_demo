//==============================================================================
//  File   : map_renderer.cpp
//  Brief  : マップ描画システム - キューブメッシュによる3Dマップ描画
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "map_renderer.h"
#include "Engine/Graphics/primitive.h"
#include "Game/Objects/camera.h"
#include "external/DirectXTex.h"

#if _DEBUG
#pragma comment(lib, "DirectXTex_Debug.lib")
#else
#pragma comment(lib, "DirectXTex_Release.lib")
#endif

namespace Game {

//==========================================================
// マクロ定義
//==========================================================
#define CUBE_VERTEX_COUNT 24  // 1面4頂点 × 6面
#define CUBE_INDEX_COUNT 36   // 6面 × 2三角形 × 3頂点

//==========================================================
// 直方体の頂点データ（24頂点+36インデックス）
//==========================================================
static Engine::Vertex3D g_CubeVertices[CUBE_VERTEX_COUNT] = {
    // --- 前面 (Z = -0.5) ---
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    // --- 背面 (Z = 0.5) ---
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    // --- 左面 (X = -0.5) ---
    {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    // --- 右面 (X = 0.5) ---
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    // --- 上面 (Y = 0.5) ---
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    // --- 下面 (Y = -0.5) ---
    {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
};

// --- インデックスバッファ（36インデックス: 6面×2三角形×3頂点） ---
static WORD g_CubeIndices[CUBE_INDEX_COUNT] = {
    // 前面
    0, 1, 2, 2, 1, 3,
    // 背面
    4, 5, 6, 6, 5, 7,
    // 左面
    8, 9,10,10, 9,11,
    // 右面
    12,13,14,14,13,15,
    // 上面
    16,17,18,18,17,19,
    // 下面
    20,21,22,22,21,23
};

//==========================================================
// コンストラクタ
//==========================================================
MapRenderer::MapRenderer()
{
    m_VertexBuffer = nullptr;
    m_IndexBuffer = nullptr;
    m_Texture = nullptr;
    m_pMap = nullptr;
}

//==========================================================
// デストラクタ
//==========================================================
MapRenderer::~MapRenderer()
{
    Uninitialize();
}

//==========================================================
// 初期化処理
//==========================================================
HRESULT MapRenderer::Initialize(Map* pMap)
{
    m_pMap = pMap;

    // --- 直方体の頂点・インデックスバッファを作成 ---
    CreateCubeBuffers();

    // --- テクスチャの読み込み ---
    DirectX::TexMetadata metadata;
    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromWICFile(L"resource/texture/white.jpg", DirectX::WIC_FLAGS_NONE, &metadata, image);
    if (SUCCEEDED(hr)) {
        hr = DirectX::CreateShaderResourceView(Engine::GetDevice(), image.GetImages(),
            image.GetImageCount(), metadata, &m_Texture);
    }
    if (FAILED(hr)) {
        m_Texture = nullptr;
    }
    return S_OK;
}

//==========================================================
// 終了処理
//==========================================================
void MapRenderer::Uninitialize()
{
    if (m_VertexBuffer) {
        m_VertexBuffer->Release();
        m_VertexBuffer = nullptr;
    }
    if (m_IndexBuffer) {
        m_IndexBuffer->Release();
        m_IndexBuffer = nullptr;
    }
    if (m_Texture) {
        m_Texture->Release();
        m_Texture = nullptr;
    }
}

//==========================================================
// 直方体の頂点・インデックスバッファ作成
//==========================================================
void MapRenderer::CreateCubeBuffers()
{
    // --- 頂点バッファ ---
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Engine::Vertex3D) * CUBE_VERTEX_COUNT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = g_CubeVertices;

    Engine::GetDevice()->CreateBuffer(&bd, &InitData, &m_VertexBuffer);

    // --- インデックスバッファ ---
    D3D11_BUFFER_DESC ibd;
    ZeroMemory(&ibd, sizeof(ibd));
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(WORD) * CUBE_INDEX_COUNT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA iInitData;
    ZeroMemory(&iInitData, sizeof(iInitData));
    iInitData.pSysMem = g_CubeIndices;

    Engine::GetDevice()->CreateBuffer(&ibd, &iInitData, &m_IndexBuffer);
}

//==========================================================
// 描画処理
//==========================================================
void MapRenderer::Draw()
{
    if (!m_pMap) return;

    Engine::Renderer::GetInstance().SetDepthEnable(true);

    // --- カメラの参照を取得 ---
    Camera& camera = GetMainCamera();

    XMMATRIX ProjectionMatrix =
        XMMatrixPerspectiveFovLH(XMConvertToRadians(camera.fov),
            (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT,
            camera.nearclip, camera.farclip
        );
    Engine::Renderer::GetInstance().SetProjectionMatrix(ProjectionMatrix);

    XMVECTOR eyev = XMLoadFloat3(&camera.Atposition);
    XMVECTOR pos = XMLoadFloat3(&camera.position);
    XMVECTOR up = XMLoadFloat3(&camera.Upvector);
    XMMATRIX ViewMatrix = XMMatrixLookAtLH(pos, eyev, up);
    Engine::Renderer::GetInstance().SetViewMatrix(ViewMatrix);

    if (m_Texture) {
        Engine::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);
    } else {
        ID3D11ShaderResourceView* whiteTex = Engine::Renderer::GetInstance().GetWhiteTexture();
        if (whiteTex) {
            Engine::GetDeviceContext()->PSSetShaderResources(0, 1, &whiteTex);
        }
    }

    // --- 頂点・インデックスバッファをセット ---
    UINT stride = sizeof(Engine::Vertex3D);
    UINT offset = 0;
    Engine::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Engine::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    Engine::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Engine::MaterialData material;
    ZeroMemory(&material, sizeof(material));
    material.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    material.emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    {
        ID3D11DeviceContext* ctx = Engine::Renderer::GetInstance().GetContext();
        ID3D11Buffer* buf = Engine::Renderer::GetInstance().GetMaterialBuffer();
        if (ctx && buf) {
            ctx->UpdateSubresource(buf, 0, nullptr, &material, 0, 0);
        }
    }

    // --- マテリアル設定 ---
    ZeroMemory(&material, sizeof(material));
    material.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    material.emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    {
        ID3D11DeviceContext* ctx = Engine::Renderer::GetInstance().GetContext();
        ID3D11Buffer* buf = Engine::Renderer::GetInstance().GetMaterialBuffer();
        if (ctx && buf) {
            ctx->UpdateSubresource(buf, 0, nullptr, &material, 0, 0);
            ctx->VSSetConstantBuffers(1, 1, &buf);  // 頂点シェーダー用
            ctx->PSSetConstantBuffers(1, 1, &buf);  // ピクセルシェーダー用
        }
    }

    // --- 全ブロックを描画 ---
    for (int y = 0; y < m_pMap->GetHeight(); y++) {
        for (int z = 0; z < m_pMap->GetDepth(); z++) {
            for (int x = 0; x < m_pMap->GetWidth(); x++) {
                if (m_pMap->GetBlock(x, y, z) == 1) {
                    XMFLOAT3 worldPos = GetWorldPosition(x, y, z);
                    DrawBox(worldPos.x, worldPos.y, worldPos.z);
                }
            }
        }
    }
}

//==========================================================
// 単一のボックス描画
//==========================================================
void MapRenderer::DrawBox(float x, float y, float z)
{
    XMMATRIX TranslationMatrix = XMMatrixTranslation(x, y, z);
    XMMATRIX ScalingMatrix = XMMatrixScaling(BOX_SIZE, BOX_SIZE, BOX_SIZE);
    XMMATRIX WorldMatrix = ScalingMatrix * TranslationMatrix;

    Engine::Renderer::GetInstance().SetWorldMatrix(WorldMatrix);

    // --- インデックスバッファを使って描画 ---
    Engine::GetDeviceContext()->DrawIndexed(CUBE_INDEX_COUNT, 0, 0);
}

//==========================================================
// ワールド座標の計算
//==========================================================
XMFLOAT3 MapRenderer::GetWorldPosition(int mapX, int mapY, int mapZ) const
{
    float offsetX = -(m_pMap->GetWidth() - 1) * BOX_SIZE * 0.5f;
    float offsetY = -(m_pMap->GetHeight() - 1) * BOX_SIZE * 0.5f;
    float offsetZ = -(m_pMap->GetDepth() - 1) * BOX_SIZE * 0.5f;

    return XMFLOAT3(
        mapX * BOX_SIZE + offsetX,
        mapY * BOX_SIZE + offsetY,
        mapZ * BOX_SIZE + offsetZ
    );
}

} // namespace Game
