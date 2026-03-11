//==============================================================================
//  File   : renderer.h
//  Brief  : レンダラー - シングルトン描画システム
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>

namespace Engine {
    using namespace DirectX;
    using Microsoft::WRL::ComPtr;

    //=====================================================
    // Renderer - シングルトン描画システム
    // 3D描画と2D UI描画の両方を担当する
    //=====================================================
    class Renderer {
    public:
        static Renderer& GetInstance();

        // --- 初期化・終了 ---
        bool Initialize(HINSTANCE hInstance, HWND hWnd, bool windowed);
        void Finalize();

        // --- フレーム操作 ---
        void Clear(float r = 0.4f, float g = 0.2f, float b = 0.2f, float a = 1.0f);
        void Present();

        // --- 深度設定 ---
        void SetDepthEnable(bool enable);

        // --- 行列設定 ---
        void SetWorldMatrix(const XMMATRIX& matrix);
        void SetViewMatrix(const XMMATRIX& matrix);
        void SetProjectionMatrix(const XMMATRIX& matrix);

        // --- 2D/3D切り替え ---
        void SetupFor2D();
        void SetupFor3D();

        // --- アクセサ ---
        ID3D11Device* GetDevice()         const { return m_pDevice.Get(); }
        ID3D11DeviceContext* GetContext()         const { return m_pContext.Get(); }
        ID3D11Buffer* GetMaterialBuffer() const { return m_pMaterialBuffer.Get(); }
        uint32_t              GetScreenWidth()    const { return m_screenWidth; }
        uint32_t              GetScreenHeight()   const { return m_screenHeight; }
        IDXGISwapChain* GetSwapChain()      const { return m_pSwapChain.Get(); }

        //=====================================================
        // 2D UI描画関数
        //=====================================================

        // 画面上に色付き矩形を描画する（HPバーの背景・残量に使用）
        // x, y: 画面左上からの座標（ピクセル）
        // width, height: 矩形のサイズ（ピクセル）
        // color: RGBA色（0.0～1.0）
        void DrawRect2D(float x, float y, float width, float height, const XMFLOAT4& color);

        // 画面上にASCIIテキストを描画する（ビットマップフォント方式）
        // x, y: 描画位置（ピクセル）
        // text: 表示文字列（ASCII 32～126対応）
        // color: 文字色（RGBA、0.0～1.0）
        void DrawText2D(float x, float y, const char* text, const XMFLOAT4& color);

		// whiteテクスチャを取得する（矩形描画でテクスチャ乗算を無効化するため）
        ID3D11ShaderResourceView* GetWhiteTexture() const { return m_pWhiteSRV.Get(); }

    private:
        Renderer() = default;
        ~Renderer() = default;
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // WVP定数バッファを更新する
        void UpdateConstantBuffer();

        // --- 2D UI用リソースの初期化・終了 ---
        bool InitUI();
        void FinalizeUI();

        // ビットマップフォントテクスチャをCPUで生成してGPUに転送する
        bool CreateFontTexture();

        // 1x1白テクスチャを生成する（矩形描画のテクスチャ乗算を無効化するため）
        bool CreateWhiteTexture();

        //=====================================================
        // D3D11コアオブジェクト
        //=====================================================
        ComPtr<ID3D11Device>            m_pDevice;
        ComPtr<ID3D11DeviceContext>      m_pContext;
        ComPtr<IDXGISwapChain>          m_pSwapChain;
        ComPtr<ID3D11RenderTargetView>  m_pRenderTargetView;
        ComPtr<ID3D11DepthStencilView>  m_pDepthStencilView;

        // --- シェーダー・入力レイアウト ---
        ComPtr<ID3D11VertexShader>  m_pVertexShader;
        ComPtr<ID3D11PixelShader>   m_pPixelShader;
        ComPtr<ID3D11InputLayout>   m_pInputLayout;

        // --- 定数バッファ ---
        ComPtr<ID3D11Buffer>  m_pConstantBuffer;   // WVP行列用
        ComPtr<ID3D11Buffer>  m_pMaterialBuffer;   // マテリアル用

        // --- 深度ステンシルステート ---
        ComPtr<ID3D11DepthStencilState> m_pDepthStateEnable;
        ComPtr<ID3D11DepthStencilState> m_pDepthStateDisable;

        // --- 行列 ---
        XMMATRIX m_worldMatrix = XMMatrixIdentity();
        XMMATRIX m_viewMatrix = XMMatrixIdentity();
        XMMATRIX m_projectionMatrix = XMMatrixIdentity();

        // --- 画面サイズ ---
        uint32_t m_screenWidth = 1920;
        uint32_t m_screenHeight = 1080;

        //=====================================================
        // 2D UI描画用リソース
        //=====================================================

        // 矩形描画用の動的頂点バッファ（6頂点 = 三角形2枚で四角形）
        ComPtr<ID3D11Buffer> m_pUIVertexBuffer;

        // テキスト描画用の動的頂点バッファ（最大128文字 × 6頂点）
        ComPtr<ID3D11Buffer> m_pTextVertexBuffer;

        // ビットマップフォントのテクスチャ（128×72ピクセル）
        ComPtr<ID3D11ShaderResourceView> m_pFontSRV;

        // 1x1白テクスチャ（矩形描画でテクスチャ乗算を無効化するため）
        ComPtr<ID3D11ShaderResourceView> m_pWhiteSRV;

        // --- フォント定数 ---
        static constexpr int MAX_TEXT_CHARS = 128;   // 1回の描画で使える最大文字数
        static constexpr int FONT_CHAR_W = 8;     // 1文字の幅（ピクセル）
        static constexpr int FONT_CHAR_H = 12;    // 1文字の高さ（ピクセル）
        static constexpr int FONT_COLS = 16;    // テクスチャ上の1行あたりの文字数
        static constexpr int FONT_ROWS = 6;     // テクスチャ上の行数
        static constexpr int FONT_TEX_W = FONT_CHAR_W * FONT_COLS;  // 128px
        static constexpr int FONT_TEX_H = FONT_CHAR_H * FONT_ROWS;  // 72px

        // UI初期化済みフラグ
        bool m_uiInitialized = false;
    };

    // グローバルアクセス関数
    inline ID3D11Device* GetDevice() { return Renderer::GetInstance().GetDevice(); }
    inline ID3D11DeviceContext* GetDeviceContext() { return Renderer::GetInstance().GetContext(); }
}
