#include "pch.h"
#include "sph_fluid.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Engine/Collision/map_collision.h"

#pragma comment(lib, "d3dcompiler.lib")

#ifndef PI
#define PI 3.14159265358979323846f
#endif

namespace Engine {

    // ============================================================
    // コンストラクタ・デストラクタ
    // ============================================================
    SPHFluid::SPHFluid() {}
    SPHFluid::~SPHFluid() { Shutdown(); }

    // ============================================================
    // Initialize — 流体システム全体の初期化
    // 
    // やること：
    //   1. デバイスとコンテキストを保存
    //   2. 画面サイズを取得（描画で使う）
    //   3. SPH物理パラメータの初期値を設定
    //
    // ※ GPU資源（シェーダー、レンダーターゲット等）は
    //    ここではまだ作らない（遅延初期化）
    // ============================================================
    bool SPHFluid::Initialize(ID3D11Device* device, int maxParticles) {
        m_device = device;
        m_maxParticles = (uint32_t)maxParticles;

        // パーティクル配列のメモリを先に確保しておく
        // （後からpush_backするたびにメモリ再確保が走るのを防ぐ）
        m_particles.reserve(m_maxParticles);

        // デバイスからコンテキスト（GPUに命令を送る窓口）を取得
        device->GetImmediateContext(&m_context);

        // --- 画面サイズの取得 ---
        // スワップチェーンから実際の画面サイズを取得しようとする
        // 取れなかったらデフォルト値(1280x720)を使う
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory> factory;
        device->QueryInterface(__uuidof(IDXGIDevice), (void**)dxgiDevice.GetAddressOf());

        m_screenWidth = 1280;
        m_screenHeight = 720;

        if (m_context) {
            // 現在のレンダーターゲット（描画先）からテクスチャサイズを取得
            ComPtr<ID3D11RenderTargetView> rtv;
            m_context->OMGetRenderTargets(1, rtv.GetAddressOf(), nullptr);
            if (rtv) {
                ComPtr<ID3D11Resource> res;
                rtv->GetResource(res.GetAddressOf());
                ComPtr<ID3D11Texture2D> tex;
                if (SUCCEEDED(res.As(&tex))) {
                    D3D11_TEXTURE2D_DESC desc;
                    tex->GetDesc(&desc);
                    m_screenWidth = desc.Width;
                    m_screenHeight = desc.Height;
                }
            }
        }
        // アスペクト比（画面の横÷縦。プロジェクション行列で使う）
        m_aspect = (float)m_screenWidth / (float)m_screenHeight;

        // --- SPH物理パラメータのデフォルト値 ---
        // smoothingRadius: どこまで遠い粒を考慮するか（影響範囲）
        // restDensity:     水の通常の密度。これより密集→押し出す、疎→引き寄せる
        // gasConstant:     密度差を圧力に変換する係数。大きいほど圧力が強い
        // viscosity:       粘性。大きいほどドロドロ
        // gravity:         重力。Y軸下向き -9.81
        m_params.smoothingRadius = 1.0f;
        m_params.restDensity = 1000.0f;
        m_params.gasConstant = 2.0f;
        m_params.viscosity = 3.5f;
        m_params.gravity = { 0.0f, -9.81f, 0.0f };
        m_params.deltaTime = 1.0f / 60.0f;
        m_params.boundaryMin = m_boundaryMin;
        m_params.boundaryMax = m_boundaryMax;
        m_params.particleCount = 0;

        m_initialized = true;
        m_ssInitialized = false; // GPU資源はまだ作ってない

        OutputDebugStringA("SPHFluid: Initialize success!\n");
        char buf[128];
        sprintf_s(buf, "SPHFluid: screen=%ux%u maxParticles=%u\n", m_screenWidth, m_screenHeight, m_maxParticles);
        OutputDebugStringA(buf);
        return true;
    }

    // ============================================================
    // InitializeScreenSpace — GPU資源の遅延初期化
    //
    // 流体を使わないシーンでは無駄なGPUメモリを食わないため
    //
    // 作るもの：
    //   1. ビルボード用の四角形メッシュ
    //   2. 5種類のシェーダー（深度/厚み/ブラー/最終合成）
    //   3. レンダーターゲット（中間画像を描く先）
    //   4. ブレンドステート等の描画設定
    //   5. 定数バッファ（シェーダーにパラメータを送る箱）
    //   6. サンプラー（テクスチャの読み方設定）
    // ============================================================
    bool SPHFluid::InitializeScreenSpace() {
        if (m_ssInitialized) return true;  // 既に初期化済みなら何もしない
        if (!m_device || !m_context) return false;

        // 1つでも失敗したらfalseを返す
        if (!CreateBillboardResources()) { OutputDebugStringA("SPHFluid: CreateBillboardResources FAILED\n"); return false; }
        if (!InitializeShaders()) { OutputDebugStringA("SPHFluid: InitializeShaders FAILED\n"); return false; }
        if (!CreateRenderTargets()) { OutputDebugStringA("SPHFluid: CreateRenderTargets FAILED\n"); return false; }
        if (!CreateStates()) { OutputDebugStringA("SPHFluid: CreateStates FAILED\n"); return false; }
        if (!CreateConstantBuffers()) { OutputDebugStringA("SPHFluid: CreateConstantBuffers FAILED\n"); return false; }
        if (!CreateSamplers()) { OutputDebugStringA("SPHFluid: CreateSamplers FAILED\n"); return false; }

        m_ssInitialized = true;
        OutputDebugStringA("SPHFluid: Screen-space resources created!\n");
        return true;
    }

    // ============================================================
    // 終了処理 — GPU資源とパーティクルを全部解放
    // ============================================================
    void SPHFluid::Finalize() { Shutdown(); }

    void SPHFluid::Shutdown() {
        // レンダーターゲット（中間画像）を全部削除
        delete m_depthRT;      m_depthRT = nullptr;      // 深度用
        delete m_blurRT1;      m_blurRT1 = nullptr;      // ブラー用1
        delete m_blurRT2;      m_blurRT2 = nullptr;      // ブラー用2
        delete m_thicknessRT;  m_thicknessRT = nullptr;  // 厚み用
        delete m_thickBlurRT1; m_thickBlurRT1 = nullptr; // 厚みブラー用1
        delete m_thickBlurRT2; m_thickBlurRT2 = nullptr; // 厚みブラー用2

        m_particles.clear();
        m_particleCount = 0;
        m_initialized = false;
        m_ssInitialized = false;
    }

    // ============================================================
    // CompileShaderFromFile — シェーダーファイルを読んでコンパイル
    //
    // シェーダー = GPUに「どう描画するか」を教えるプログラム
    // HLSLというGPU用の言語で書かれたファイルを読み込んで
    // GPUが理解できるバイトコードに変換する
    //
    // 工夫:
    //   - BOM（ファイル先頭の不要な印）を自動スキップ
    //   - UTF-16ファイルを検出してエラーメッセージを出す
    //   - コンパイルエラーの詳細をデバッグ出力する
    // ============================================================
    static HRESULT CompileShaderFromFile(const wchar_t* filename,
        const char* entryPoint,   // シェーダー内の関数名
        const char* target,       // "vs_5_0"=頂点シェーダー, "ps_5_0"=ピクセルシェーダー
        ID3DBlob** blob) {        // コンパイル結果の出力先

        // カレントディレクトリを出力（シェーダーが見つからない時のデバッグ用）
        static bool logged = false;
        if (!logged) {
            wchar_t cwd[512];
            GetCurrentDirectoryW(512, cwd);
            char buf[1024];
            sprintf_s(buf, "SPHFluid: Current directory: %ls\n", cwd);
            OutputDebugStringA(buf);
            logged = true;
        }

        // --- ファイルをバイナリモードで読み込む ---
        FILE* f = nullptr;
        _wfopen_s(&f, filename, L"rb");
        if (!f) {
            char buf[256];
            sprintf_s(buf, "SPHFluid: Cannot open shader file: %ls\n", filename);
            OutputDebugStringA(buf);
            return E_FAIL;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<char> data(size);
        fread(data.data(), 1, size, f);
        fclose(f);

        // デバッグ用: ファイルサイズと先頭60文字を出力
        {
            char buf[256];
            sprintf_s(buf, "SPHFluid: Loaded %ls (%ld bytes) entry=%s\n", filename, size, entryPoint);
            OutputDebugStringA(buf);
            char preview[64] = {};
            int previewLen = (size < 60) ? (int)size : 60;
            memcpy(preview, data.data(), previewLen);
            for (int i = 0; i < previewLen; i++) {
                if (preview[i] == '\n' || preview[i] == '\r') preview[i] = ' ';
            }
            preview[previewLen] = 0;
            sprintf_s(buf, "SPHFluid: Preview: [%s]\n", preview);
            OutputDebugStringA(buf);
        }

        // --- BOM（Byte Order Mark）のスキップ ---
        // テキストエディタによってはファイル先頭に付く不要なバイト列
        // これがあるとD3DCompileが「文法エラー」と言ってくる
        const char* src = data.data();
        SIZE_T srcSize = (SIZE_T)size;

        // UTF-8 BOM: EF BB BF の3バイト
        if (srcSize >= 3 &&
            (unsigned char)src[0] == 0xEF &&
            (unsigned char)src[1] == 0xBB &&
            (unsigned char)src[2] == 0xBF) {
            src += 3;
            srcSize -= 3;
            OutputDebugStringA("SPHFluid: BOM detected and skipped\n");
        }
        // UTF-16 BOM: FF FE → これはHLSLコンパイラが読めないのでエラー
        if (srcSize >= 2 &&
            (unsigned char)src[0] == 0xFF &&
            (unsigned char)src[1] == 0xFE) {
            OutputDebugStringA("SPHFluid: Shader file is UTF-16! Save as UTF-8!\n");
            return E_FAIL;
        }

        // --- コンパイルオプション ---
        DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
        // デバッグビルドではシェーダーデバッグ情報を含める
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        // --- 実際のコンパイル ---
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3DCompile(src, srcSize, nullptr, nullptr, nullptr,
            entryPoint, target, flags, 0, blob, errorBlob.GetAddressOf());
        if (FAILED(hr) && errorBlob) {
            // コンパイルエラーの詳細を出力
            char buf[2048];
            sprintf_s(buf, "SPHFluid shader error [%s]: %s\n", entryPoint,
                (char*)errorBlob->GetBufferPointer());
            OutputDebugStringA(buf);
        }
        return hr;
    }

    // ============================================================
    // CreateBillboardResources — ビルボード用のメッシュを作る
    //
    // ビルボード = カメラに常に正対する板ポリゴン
    // パーティクル1個につき1枚のビルボードを描画する
    //
    // 作るもの:
    //   1. 四角形の頂点バッファ（4頂点: 左下,左上,右上,右下）
    //   2. インデックスバッファ（4頂点→2三角形の描画順序）
    //   3. インスタンスバッファ（パーティクルの位置を流し込む）
    //
    // インスタンシング:
    //   四角形は1つだけ作り、パーティクルの数だけ「使い回す」
    //   → 四角形メッシュ × パーティクル位置 = 大量の板を1回で描画
    //   → ドローコール1回で全パーティクル描画（超高速）
    // ============================================================
    bool SPHFluid::CreateBillboardResources() {
        // 四角形の4頂点（-1〜+1の範囲。シェーダーでサイズ調整する）
        XMFLOAT2 quadVerts[4] = { {-1,-1}, {-1,1}, {1,1}, {1,-1} };

        D3D11_BUFFER_DESC vbd = {};
        vbd.ByteWidth = sizeof(quadVerts);
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.Usage = D3D11_USAGE_DEFAULT;
        D3D11_SUBRESOURCE_DATA sd = { quadVerts };
        if (FAILED(m_device->CreateBuffer(&vbd, &sd, &m_pBillboardVB))) return false;

        // 三角形の描画順序（0-1-2で三角形1、0-2-3で三角形2）
        UINT idx[6] = { 0,1,2, 0,2,3 };
        D3D11_BUFFER_DESC ibd = {};
        ibd.ByteWidth = sizeof(idx);
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibd.Usage = D3D11_USAGE_DEFAULT;
        D3D11_SUBRESOURCE_DATA id = { idx };
        if (FAILED(m_device->CreateBuffer(&ibd, &id, &m_pBillboardIB))) return false;

        // インスタンスバッファ（毎フレームCPUから書き換えるのでDYNAMIC）
        // 各パーティクルのワールド座標(XMFLOAT3)を格納する
        D3D11_BUFFER_DESC inst = {};
        inst.ByteWidth = sizeof(XMFLOAT3) * m_maxParticles;
        inst.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        inst.Usage = D3D11_USAGE_DYNAMIC;        // CPUから毎フレーム書き換え可能
        inst.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_device->CreateBuffer(&inst, nullptr, &m_pInstanceBuffer))) return false;

        return true;
    }

    // ============================================================
    // InitializeShaders — 全シェーダーのコンパイルとGPUオブジェクト作成
    //
    // 5パス分のシェーダーを用意する:
    //   パス1: fluid_depth.hlsl    → 深度描画（VS + PS）
    //   パス2-3: fluid_blur.hlsl   → ブラー（VS + PS×2種）
    //   パス4: fluid_thickness.hlsl → 厚み描画（PS）
    //   パス5: fluid_final.hlsl    → 最終合成（PS）
    //
    // 入力レイアウト:
    //   slot0: POSITION (float2) — 四角形の頂点座標
    //   slot1: INST_POS (float3) — パーティクルのワールド座標（インスタンス）
    // ============================================================
    bool SPHFluid::InitializeShaders() {
        ID3DBlob* blob = nullptr;
        HRESULT hr;

        // === 深度パスの頂点シェーダー ===
        hr = CompileShaderFromFile(L"fluid_depth.hlsl", "VS_Depth", "vs_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile VS_Depth\n"); return false; }

        // 入力レイアウト: GPUに「頂点データの並び方」を教える
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            // slot0: 四角形の2D座標（全パーティクル共通）
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            // slot1: パーティクルの3D位置（パーティクルごとに異なる）
            { "INST_POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
        hr = m_device->CreateInputLayout(layout, 2, blob->GetBufferPointer(), blob->GetBufferSize(), m_pBillboardLayout.GetAddressOf());
        m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pDepthVS.GetAddressOf());
        blob->Release(); blob = nullptr;
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to create input layout\n"); return false; }

        // === 深度パスのピクセルシェーダー ===
        hr = CompileShaderFromFile(L"fluid_depth.hlsl", "PS_Depth", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_Depth\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pDepthPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        // === 厚みパスのピクセルシェーダー ===
        hr = CompileShaderFromFile(L"fluid_thickness.hlsl", "PS_Thickness", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_Thickness\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pThicknessPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        // === ブラーパスの頂点シェーダー ===
        // VS_Quad: SV_VertexID（0,1,2）から全画面三角形を生成
        // 頂点バッファ不要で画面全体を覆う三角形を作る賢いテクニック
        hr = CompileShaderFromFile(L"fluid_blur.hlsl", "VS_Quad", "vs_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile VS_Quad\n"); return false; }
        m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pQuadVS.GetAddressOf());
        blob->Release(); blob = nullptr;

        // === バイラテラルブラー（エッジ保持型。水面用） ===
        hr = CompileShaderFromFile(L"fluid_blur.hlsl", "PS_BilateralBlur", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_BilateralBlur\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pBilateralBlurPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        // === ガウシアンブラー（単純ぼかし。予備用） ===
        hr = CompileShaderFromFile(L"fluid_blur.hlsl", "PS_GaussianBlur", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_GaussianBlur\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pGaussianBlurPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        // === 最終合成パス（法線復元→ライティング→色決定） ===
        hr = CompileShaderFromFile(L"fluid_final.hlsl", "PS_Final", "ps_5_0", &blob);
        if (FAILED(hr)) { OutputDebugStringA("SPHFluid: Failed to compile PS_Final\n"); return false; }
        m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pFinalPS.GetAddressOf());
        blob->Release(); blob = nullptr;

        OutputDebugStringA("SPHFluid: All shaders compiled successfully!\n");
        return true;
    }

    // ============================================================
    // CreateRenderTargets — 中間画像の描画先を作る
    //
    // レンダーターゲット = 画面に直接描くのではなく、
    //   テクスチャに描いて後で使う「一時的なキャンバス」
    //
    // 作るもの:
    //   depthRT:     深度パスの結果（R32_FLOAT = 32bit浮動小数1ch）
    //   blurRT1/2:   ブラーのピンポン用（交互に読み書き）
    //   thicknessRT: 厚みパスの結果（半解像度・R16_FLOAT = 16bit）
    //   thickBlurRT: 厚みブラー用（現在未使用だが拡張用に確保）
    //
    // なぜR32_FLOATか:
    //   深度値は0.0〜1.0の精密な浮動小数。RGBA8だと精度が足りない
    // なぜ厚みは半解像度か:
    //   厚みは色の濃淡に使うだけで高精度が不要。GPU負荷を1/4に削減
    // ============================================================
    bool SPHFluid::CreateRenderTargets() {
        // 既存のRTがあれば先に削除（画面サイズ変更時の再作成対応）
        if (m_depthRT) { delete m_depthRT;     m_depthRT = nullptr; }
        if (m_blurRT1) { delete m_blurRT1;     m_blurRT1 = nullptr; }
        if (m_blurRT2) { delete m_blurRT2;     m_blurRT2 = nullptr; }
        if (m_thicknessRT) { delete m_thicknessRT; m_thicknessRT = nullptr; }
        if (m_thickBlurRT1) { delete m_thickBlurRT1; m_thickBlurRT1 = nullptr; }
        if (m_thickBlurRT2) { delete m_thickBlurRT2; m_thickBlurRT2 = nullptr; }

        UINT w = m_screenWidth;
        UINT h = m_screenHeight;
        UINT halfW = w / 2;  // 厚み用は半分の解像度
        UINT halfH = h / 2;

        // 深度用（フル解像度）
        m_depthRT = new RenderTarget();
        if (!m_depthRT->Create(m_device, w, h, DXGI_FORMAT_R32_FLOAT, false)) {
            OutputDebugStringA("SPHFluid: Failed to create depthRT\n");
            return false;
        }

        // ブラー用×2（フル解像度。水平と垂直で交互に使う）
        m_blurRT1 = new RenderTarget();
        m_blurRT2 = new RenderTarget();
        if (!m_blurRT1->Create(m_device, w, h, DXGI_FORMAT_R32_FLOAT, false) ||
            !m_blurRT2->Create(m_device, w, h, DXGI_FORMAT_R32_FLOAT, false)) {
            OutputDebugStringA("SPHFluid: Failed to create blurRTs\n");
            return false;
        }

        // 厚み用（半解像度。精度より速度優先）
        m_thicknessRT = new RenderTarget();
        if (!m_thicknessRT->Create(m_device, halfW, halfH, DXGI_FORMAT_R16_FLOAT, false)) {
            OutputDebugStringA("SPHFluid: Failed to create thicknessRT\n");
            return false;
        }

        // 厚みブラー用（将来の拡張用に確保）
        m_thickBlurRT1 = new RenderTarget();
        m_thickBlurRT2 = new RenderTarget();
        if (!m_thickBlurRT1->Create(m_device, halfW, halfH, DXGI_FORMAT_R16_FLOAT, false) ||
            !m_thickBlurRT2->Create(m_device, halfW, halfH, DXGI_FORMAT_R16_FLOAT, false)) {
            OutputDebugStringA("SPHFluid: Failed to create thickBlurRTs\n");
            return false;
        }

        OutputDebugStringA("SPHFluid: All render targets created\n");
        return true;
    }

    // ============================================================
    // CreateStates — GPU描画の「設定」をまとめて作る
    //
    // ブレンドステート: ピクセルの混ぜ方
    //   - アルファブレンド: 半透明描画（最終合成で使う）
    //   - 加算ブレンド:     値を足し算（厚みパスで使う）
    //   - MINブレンド:      小さい方を残す（深度パスで使う）
    //
    // 深度ステンシル: 深度テスト無効
    //   流体は独自のレンダーターゲットに描くので
    //   通常の深度バッファは使わない
    //
    // ラスタライザ: 両面描画・カリングなし
    //   ビルボードは裏表関係なく描きたいため
    // ============================================================
    bool SPHFluid::CreateStates() {
        HRESULT hr;

        // --- アルファブレンド（最終パスで水を半透明に重ねる） ---
        // SrcAlpha × 新しいピクセル + (1-SrcAlpha) × 背景 = 半透明合成
        {
            D3D11_BLEND_DESC d = {};
            d.AlphaToCoverageEnable = FALSE;
            d.IndependentBlendEnable = FALSE;
            d.RenderTarget[0].BlendEnable = TRUE;
            d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
            d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            hr = m_device->CreateBlendState(&d, &m_pAlphaBlendState);
            if (FAILED(hr)) return false;
        }

        // --- 加算ブレンド（厚みパスで粒の厚みを足し算する） ---
        // 新しい値 + 既存の値 = 累積（粒が重なるほど厚くなる）
        {
            D3D11_BLEND_DESC d = {};
            d.RenderTarget[0].BlendEnable = TRUE;
            d.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            d.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
            d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
            d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            hr = m_device->CreateBlendState(&d, &m_pAdditiveBlendState);
            if (FAILED(hr)) return false;
        }

        // --- MINブレンド（深度パスで一番手前の深度だけ残す） ---
        // min(新しい深度, 既存の深度) → 描画順に関係なく最前面を取得
        // これがないと後から描いた粒が手前の粒を上書きしてしまう
        {
            D3D11_BLEND_DESC d = {};
            d.RenderTarget[0].BlendEnable = TRUE;
            d.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            d.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
            d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;  // ← ここがミソ
            d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
            d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MIN;
            d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            hr = m_device->CreateBlendState(&d, &m_pMinBlendState);
            if (FAILED(hr)) return false;
        }

        // --- 深度テスト無効 ---
        // 流体は専用のレンダーターゲットに描くので、通常の深度バッファは不要
        {
            D3D11_DEPTH_STENCIL_DESC d = {};
            d.DepthEnable = FALSE;
            d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            hr = m_device->CreateDepthStencilState(&d, &m_pDepthDisabledState);
            if (FAILED(hr)) return false;
        }

        // --- カリングなし ---
        // ビルボードは裏表なく描画したい
        {
            D3D11_RASTERIZER_DESC d = {};
            d.FillMode = D3D11_FILL_SOLID;
            d.CullMode = D3D11_CULL_NONE;   // 裏面も描画
            d.DepthClipEnable = TRUE;
            hr = m_device->CreateRasterizerState(&d, &m_pNoCullRS);
            if (FAILED(hr)) return false;
        }
        return true;
    }

    // ============================================================
    // CreateConstantBuffers — シェーダーにパラメータを送る「箱」を作る
    //
    // 定数バッファ = CPUからGPUシェーダーにデータを渡す入れ物
    //   CBCamera: ビュー行列、プロジェクション行列、粒サイズ等
    //   CBBlur:   ブラー方向、ブラー強さ等
    //   CBFinal:  逆プロジェクション行列、水の色、光の方向等
    //
    // DYNAMIC + CPU_ACCESS_WRITE: 毎フレームCPUから書き換え可能
    // サイズは16バイト境界にアラインメント（GPUの要求）
    // ============================================================
    bool SPHFluid::CreateConstantBuffers() {
        auto makeCB = [&](UINT size, ComPtr<ID3D11Buffer>& buf) -> bool {
            D3D11_BUFFER_DESC d = {};
            d.ByteWidth = (size + 15) & ~15;  // 16バイト境界に切り上げ
            d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            d.Usage = D3D11_USAGE_DYNAMIC;
            d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            return SUCCEEDED(m_device->CreateBuffer(&d, nullptr, buf.GetAddressOf()));
            };
        if (!makeCB(sizeof(CBCamera), m_pCameraCB)) return false;
        if (!makeCB(sizeof(CBBlur), m_pBlurCB))     return false;
        if (!makeCB(sizeof(CBFinal), m_pFinalCB))    return false;
        return true;
    }

    // ============================================================
    // CreateSamplers — テクスチャの読み方設定を作る
    //
    // サンプラー = テクスチャからピクセルを読むときの補間方法
    //   PointSampler:  最近傍（ぼかしなし。深度テクスチャ用）
    //   LinearSampler: バイリニア補間（滑らかに読む。厚みテクスチャ用）
    //
    // CLAMP: テクスチャの端を超えたら端の色を繰り返す
    // ============================================================
    bool SPHFluid::CreateSamplers() {
        {
            D3D11_SAMPLER_DESC d = {};
            d.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;  // ぼかしなし
            d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            if (FAILED(m_device->CreateSamplerState(&d, &m_pPointSampler))) return false;
        }
        {
            D3D11_SAMPLER_DESC d = {};
            d.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;  // 滑らか補間
            d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            if (FAILED(m_device->CreateSamplerState(&d, &m_pLinearSampler))) return false;
        }
        return true;
    }

    // ============================================================
    // UpdateInstanceBuffer — パーティクル位置をGPUに送る
    //
    // 毎フレーム、CPUで計算した全パーティクルの位置(XMFLOAT3)を
    // GPUのインスタンスバッファにコピーする
    //
    // MAP_WRITE_DISCARD: 「前のデータは捨てていいから書かせて」
    //   GPUが前フレームのデータをまだ使っていても安全に書ける
    // ============================================================
    void SPHFluid::UpdateInstanceBuffer(ID3D11DeviceContext* ctx) {
        if (m_particleCount == 0) return;
        D3D11_MAPPED_SUBRESOURCE mp;
        if (FAILED(ctx->Map(m_pInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) return;
        XMFLOAT3* dst = (XMFLOAT3*)mp.pData;
        for (uint32_t i = 0; i < m_particleCount; i++)
            dst[i] = m_particles[i].position;
        ctx->Unmap(m_pInstanceBuffer.Get(), 0);
    }

    // ============================================================
    // SpawnParticles — 球状にランダム配置でパーティクルを生成
    //
    // center: 生成中心位置
    // count:  生成したい数
    // radius: 生成範囲の半径
    //
    // 球座標(θ,φ,r)でランダムな位置を作る
    // 初速度はゼロ（その場に置くだけ）
    // ============================================================
    void SPHFluid::SpawnParticles(const XMFLOAT3& c, uint32_t count, float radius) {
        // 上限を超えないように実際の生成数を決定
        uint32_t n = std::min(count, m_maxParticles - m_particleCount);
        char buf[128];
        sprintf_s(buf, "SPHFluid::SpawnParticles requested=%u actual=%u current=%u\n", count, n, m_particleCount);
        OutputDebugStringA(buf);

        for (uint32_t i = 0; i < n; ++i) {
            SPHParticle p;
            // 球座標でランダムな位置を計算
            float th = (float)rand() / RAND_MAX * 2.0f * PI;   // 水平角 0〜2π
            float ph = (float)rand() / RAND_MAX * PI;           // 垂直角 0〜π
            float r = (float)rand() / RAND_MAX * radius;        // 半径 0〜radius
            p.position = {
                c.x + r * sinf(ph) * cosf(th),
                c.y + r * sinf(ph) * sinf(th),
                c.z + r * cosf(ph)
            };
            p.velocity = { 0,0,0 };
            p.acceleration = { 0,0,0 };
            p.density = m_params.restDensity;
            p.pressure = 0;
            p.mass = 1.0f;
            p.lifetime = m_particleLifetime;
            m_particles.push_back(p);
            m_particleCount++;
        }
        m_params.particleCount = m_particleCount;
    }

    // ============================================================
    // SpawnParticleWithVelocity — 初速度付きで1個生成
    //
    // 水鉄砲から発射するときに使う
    // pos: 発射位置、vel: 発射速度（方向と速さ）
    // ============================================================
    void SPHFluid::SpawnParticleWithVelocity(const XMFLOAT3& pos, const XMFLOAT3& vel) {
        if (m_particleCount >= m_maxParticles) {
            static int log = 0;
            if (log++ < 3) OutputDebugStringA("SPHFluid: SpawnParticle REJECTED (max reached)\n");
            return;
        }
        SPHParticle p;
        p.position = pos;
        p.velocity = vel;
        p.acceleration = { 0,0,0 };
        p.density = m_params.restDensity;
        p.pressure = 0;
        p.mass = 1.0f;
        p.lifetime = m_particleLifetime;
        m_particles.push_back(p);
        m_particleCount++;
        m_params.particleCount = m_particleCount;

        static int log2 = 0;
        if (log2++ < 5) {
            char buf[256];
            sprintf_s(buf, "SPHFluid: Spawned! cnt=%u pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f)\n",
                m_particleCount, pos.x, pos.y, pos.z, vel.x, vel.y, vel.z);
            OutputDebugStringA(buf);
        }
    }

    // ============================================================
    // SPHカーネル関数群
    //
    // カーネル関数 = 「距離rの粒がどれくらい影響するか」を決める重み関数
    // h（スムージング半径）以内の粒だけ影響し、それ以外は0
    //
    // 3種類使い分ける理由:
    //   Poly6:     密度計算用。滑らかで安定。
    //   Spiky:     圧力勾配用。r=0でも勾配が残る（粒が重なっても押し出せる）
    //   Viscosity: 粘性用。ラプラシアン（2階微分）が常に正で安定。
    // ============================================================

    // Poly6カーネル: 315/(64πh⁹) × (h²-r²)³
    // 距離0で最大、距離hでゼロ。密度計算に使う
    float SPHFluid::Poly6Kernel(float r, float h) {
        if (r >= h) return 0;  // 影響範囲外
        float d = h * h - r * r;
        return 315.0f / (64.0f * PI * powf(h, 9)) * d * d * d;
    }

    // Spikyカーネルの勾配: -45/(πh⁶) × (h-r)² × (diff/r)
    // 圧力による「どっちの方向にどれだけ押すか」を計算
    // diffは2粒間のベクトル、rはその長さ
    XMFLOAT3 SPHFluid::SpikyGradient(const XMFLOAT3& diff, float r, float h) {
        if (r >= h || r < 1e-6f) return { 0,0,0 };  // 範囲外 or 距離ゼロ（ゼロ除算防止）
        float c = -45.0f / (PI * powf(h, 6)) * powf(h - r, 2) / r;
        return { c * diff.x, c * diff.y, c * diff.z };
    }

    // Viscosityカーネルのラプラシアン: 45/(πh⁶) × (h-r)
    // 粘性力の計算に使う。距離が近いほど大きい
    float SPHFluid::ViscosityLaplacian(float r, float h) {
        if (r >= h) return 0;
        return 45.0f / (PI * powf(h, 6)) * (h - r);
    }

    // ============================================================
    // EnforceBoundary — 境界の壁で跳ね返す
    //
    // パーティクルが設定した境界の外に出たら
    // 壁の位置に戻して、速度を反転×減衰(0.3)
    // 壁にぶつかると速度が30%になって跳ね返るイメージ
    // ============================================================
    void SPHFluid::EnforceBoundary(SPHParticle& p) {
        float b = 0.3f;  // 反発係数（1.0=完全弾性、0.0=完全吸収）
        if (p.position.x < m_boundaryMin.x) { p.position.x = m_boundaryMin.x; p.velocity.x *= -b; }
        if (p.position.x > m_boundaryMax.x) { p.position.x = m_boundaryMax.x; p.velocity.x *= -b; }
        if (p.position.y < m_boundaryMin.y) { p.position.y = m_boundaryMin.y; p.velocity.y *= -b; }
        if (p.position.y > m_boundaryMax.y) { p.position.y = m_boundaryMax.y; p.velocity.y *= -b; }
        if (p.position.z < m_boundaryMin.z) { p.position.z = m_boundaryMin.z; p.velocity.z *= -b; }
        if (p.position.z > m_boundaryMax.z) { p.position.z = m_boundaryMax.z; p.velocity.z *= -b; }
    }

    // ============================================================
    // CollideWithMap — マップブロック（壁・床）との衝突
    //
    // 各パーティクルを小さなBoxCollider（箱型の当たり判定）に見立てて
    // MapCollisionシステムで近くのブロックと衝突判定する
    //
    // プレイヤーの衝突判定と同じ仕組みを流用している
    // → 実装コストを抑えつつマップとの整合性を維持
    //
    // 流れ:
    //   1. パーティクル位置に小さなBoxColliderを作る
    //   2. 空間分割グリッドから近くのブロックを取得
    //   3. めり込み量を計算して押し戻す
    //   4. めり込んだ軸の速度を反転×減衰（跳ね返り）
    // ============================================================
    void SPHFluid::CollideWithMap() {
        float bounce = 0.3f;              // 跳ね返り係数
        float particleRadius = m_particleScale;  // パーティクルの大きさ

        for (uint32_t i = 0; i < m_particleCount; i++) {
            XMFLOAT3& pos = m_particles[i].position;
            XMFLOAT3& vel = m_particles[i].velocity;

            // パーティクルを小さな箱として衝突判定
            Engine::BoxCollider particleBox(pos, { particleRadius, particleRadius, particleRadius });

            // 近くのブロックを空間分割グリッドから取得
            auto& mapCol = Engine::MapCollision::GetInstance();
            auto nearbyBlocks = mapCol.GetNearbyBlocks(pos, 1.5f);

            for (auto* block : nearbyBlocks) {
                XMFLOAT3 pen;  // めり込みベクトル（どれだけ食い込んでるか）
                if (particleBox.ComputePenetration(block, pen)) {
                    // めり込み分だけ押し戻す
                    pos.x += pen.x;
                    pos.y += pen.y;
                    pos.z += pen.z;

                    // めり込んだ軸の速度を反転×減衰
                    if (pen.x != 0.0f) { vel.x *= -bounce; }
                    if (pen.y != 0.0f) { vel.y *= -bounce; }
                    if (pen.z != 0.0f) { vel.z *= -bounce; }

                    // 押し戻し後の位置でコライダーを更新（連続衝突対応）
                    particleBox.SetCenter(pos);
                }
            }
        }
    }

    // ============================================================
    // CollideWithPlayers — プレイヤーとの衝突
    //
    // プレイヤーの位置・サイズ情報をAABB（軸平行境界ボックス）として受け取り
    // パーティクルがその中に入っていたら最小軸で押し出す
    //
    // CollideWithMapと似ているが、こちらはBoxColliderクラスを使わず
    // 直接AABB判定を行っている（プレイヤーの数が少ないため単純実装）
    // ============================================================
    void SPHFluid::CollideWithPlayers() {
        if (m_playerBoxes.empty()) return;

        float bounce = 0.2f;
        float particleRadius = m_particleScale;

        for (uint32_t i = 0; i < m_particleCount; i++) {
            XMFLOAT3& pos = m_particles[i].position;
            XMFLOAT3& vel = m_particles[i].velocity;

            for (const auto& pbox : m_playerBoxes) {
                float halfX = pbox.halfSize.x;
                float halfY = pbox.halfSize.y;
                float halfZ = pbox.halfSize.z;

                // パーティクルとプレイヤー中心の差
                float dx = pos.x - pbox.center.x;
                float dy = pos.y - pbox.center.y;
                float dz = pos.z - pbox.center.z;

                // AABB内にいるかチェック（パーティクル半径も考慮）
                if (fabsf(dx) < halfX + particleRadius &&
                    fabsf(dy) < halfY + particleRadius &&
                    fabsf(dz) < halfZ + particleRadius) {

                    // 各軸のめり込み量を計算
                    float overlapX = (halfX + particleRadius) - fabsf(dx);
                    float overlapY = (halfY + particleRadius) - fabsf(dy);
                    float overlapZ = (halfZ + particleRadius) - fabsf(dz);

                    // 最小のめり込み軸で押し出す（最も自然な方向）
                    if (overlapX <= overlapY && overlapX <= overlapZ) {
                        pos.x += (dx > 0 ? overlapX : -overlapX);
                        vel.x *= -bounce;
                    } else if (overlapY <= overlapZ) {
                        pos.y += (dy > 0 ? overlapY : -overlapY);
                        vel.y *= -bounce;
                    } else {
                        pos.z += (dz > 0 ? overlapZ : -overlapZ);
                        vel.z *= -bounce;
                    }
                }
            }
        }
    }

    // ============================================================
    // SimulateCPU — SPH流体物理の本体
    //
    // 1フレーム分の物理シミュレーションを実行する
    //
    // 大きな流れ:
    //   1. サブステップに分割（すり抜け防止）
    //   2. [SPH] 密度を計算（各粒の周りにどれだけ粒があるか）
    //   3. [SPH] 力を計算（圧力+粘性+凝集力）
    //   4. 積分（力→加速度→速度→位置）
    //   5. 境界チェック＆衝突判定
    //
    // パフォーマンス工夫:
    //   - 200個超のパーティクルはSPH計算をスキップ（O(n²)対策）
    //   - SPH計算は最初のサブステップだけ（重い処理の削減）
    //   - 速度・加速度にクランプで数値爆発を防止
    // ============================================================
    void SPHFluid::SimulateCPU(float dt) {
        float h = m_params.smoothingRadius;  // スムージング半径
        uint32_t n = m_particleCount;
        float damping = 0.98f;  // 毎フレーム速度を2%減衰（空気抵抗的な）

        // --- サブステップ ---
        // 1フレームを2回に分けて計算する
        // 理由: 高速パーティクルが1フレームで壁を飛び越えるのを防ぐ
        // 2回に分けると1回の移動量が半分になる
        const int subSteps = 2;
        float subDt = dt / (float)subSteps;

        // SPH計算の上限パーティクル数
        // O(n²)なので200個でも200×200=40,000回の計算
        // これ以上増やすとフレームレートが死ぬ
        const uint32_t MAX_SIM = 200;

        for (int step = 0; step < subSteps; step++) {

            // --- SPH計算（密度と力）---
            // 条件: パーティクル数が2以上かつMAX_SIM以下
            //        & 最初のサブステップだけ（計算量削減）
            if (n <= MAX_SIM && n >= 2) {
                if (step == 0) {

                    // ========== 密度計算 ==========
                    // 各パーティクルの「周囲にどれだけ粒があるか」を数える
                    // Poly6カーネルで距離に応じた重みを付けて足し合わせる
                    for (uint32_t i = 0; i < n; i++) {
                        m_particles[i].density = 0;
                        for (uint32_t j = 0; j < n; j++) {
                            // i番目とj番目の粒の距離を計算
                            float dx = m_particles[i].position.x - m_particles[j].position.x;
                            float dy = m_particles[i].position.y - m_particles[j].position.y;
                            float dz = m_particles[i].position.z - m_particles[j].position.z;
                            float r = sqrtf(dx * dx + dy * dy + dz * dz);
                            // 距離rに応じた重みを密度に加算
                            m_particles[i].density += m_particles[j].mass * Poly6Kernel(r, h);
                        }
                        // 密度が小さすぎるとゼロ除算の危険→下限を設定
                        if (m_particles[i].density < 1.0f) m_particles[i].density = m_params.restDensity;

                        // 圧力 = k × (実際の密度 - 通常の密度)
                        // 密集→正の圧力（押し出す）、疎→負の圧力（引き寄せる）
                        m_particles[i].pressure = m_params.gasConstant * (m_particles[i].density - m_params.restDensity);
                    }

                    // ========== 力の計算 ==========
                    // 3つの力を合算する:
                    //   fP: 圧力（密集→押し出し、疎→引き込み）
                    //   fV: 粘性（隣と速度を揃える）
                    //   fCohesion: 凝集力（水のまとまり感）
                    for (uint32_t i = 0; i < n; i++) {
                        XMFLOAT3 fP = { 0,0,0 }, fV = { 0,0,0 };
                        XMFLOAT3 fCohesion = { 0,0,0 };

                        for (uint32_t j = 0; j < n; j++) {
                            if (i == j) continue;  // 自分自身はスキップ
                            float dx = m_particles[i].position.x - m_particles[j].position.x;
                            float dy = m_particles[i].position.y - m_particles[j].position.y;
                            float dz = m_particles[i].position.z - m_particles[j].position.z;
                            float r = sqrtf(dx * dx + dy * dy + dz * dz);

                            if (r >= h || r < 1e-6f) continue;  // 影響範囲外 or 重なりすぎ

                            // --- 圧力 ---
                            // 2粒の圧力の平均 × Spikyカーネル勾配 → 押し出し方向と力
                            float pA = (m_particles[i].pressure + m_particles[j].pressure) * 0.5f;
                            XMFLOAT3 g = SpikyGradient({ dx,dy,dz }, r, h);
                            float mR = m_particles[j].mass / m_particles[j].density;
                            fP.x += -mR * pA * g.x;
                            fP.y += -mR * pA * g.y;
                            fP.z += -mR * pA * g.z;

                            // --- 粘性 ---
                            // 速度差 × Viscosityラプラシアン → 速度を揃える力
                            float lap = ViscosityLaplacian(r, h);
                            fV.x += mR * (m_particles[j].velocity.x - m_particles[i].velocity.x) * lap;
                            fV.y += mR * (m_particles[j].velocity.y - m_particles[i].velocity.y) * lap;
                            fV.z += mR * (m_particles[j].velocity.z - m_particles[i].velocity.z) * lap;

                            // --- 凝集力 ---
                            // 近くの粒を引き寄せる（表面張力の代わり）
                            // これがないと水がどんどん薄く広がってしまう
                            float cohesionStrength = 5.0f;
                            float cohesion = Poly6Kernel(r, h) * cohesionStrength;
                            fCohesion.x -= dx / r * cohesion;  // 相手の方向に引っ張る
                            fCohesion.y -= dy / r * cohesion;
                            fCohesion.z -= dz / r * cohesion;
                        }

                        // 粘性にviscosityパラメータを掛ける
                        fV.x *= m_params.viscosity;
                        fV.y *= m_params.viscosity;
                        fV.z *= m_params.viscosity;

                        // 力の合計 ÷ 密度 + 重力 = 加速度
                        // （ニュートンの第二法則: F/m = a、ここではm≒密度）
                        float inv = 1.0f / m_particles[i].density;
                        if (!isfinite(inv)) inv = 1.0f / m_params.restDensity;

                        m_particles[i].acceleration.x = (fP.x + fV.x + fCohesion.x) * inv + m_params.gravity.x;
                        m_particles[i].acceleration.y = (fP.y + fV.y + fCohesion.y) * inv + m_params.gravity.y;
                        m_particles[i].acceleration.z = (fP.z + fV.z + fCohesion.z) * inv + m_params.gravity.z;

                        // 加速度クランプ（異常な力の発生を防ぐ安全装置）
                        float maxAcc = 500.0f;
                        m_particles[i].acceleration.x = fmaxf(-maxAcc, fminf(maxAcc, m_particles[i].acceleration.x));
                        m_particles[i].acceleration.y = fmaxf(-maxAcc, fminf(maxAcc, m_particles[i].acceleration.y));
                        m_particles[i].acceleration.z = fmaxf(-maxAcc, fminf(maxAcc, m_particles[i].acceleration.z));
                    }
                }
            }

            // ========== 積分（位置・速度の更新） ==========
            // 全パーティクル共通の処理
            float maxVel = 50.0f;  // 速度の上限（すり抜け防止）

            for (uint32_t i = 0; i < n; i++) {
                // パーティクル数がMAX_SIM超 or 2未満 → 重力だけ適用
                // それ以外 → SPHで計算した加速度を適用
                if (n > MAX_SIM || n < 2) {
                    m_particles[i].velocity.x += m_params.gravity.x * subDt;
                    m_particles[i].velocity.y += m_params.gravity.y * subDt;
                    m_particles[i].velocity.z += m_params.gravity.z * subDt;
                } else {
                    // 加速度 × 時間 = 速度の変化量（物理の基本公式）
                    m_particles[i].velocity.x += m_particles[i].acceleration.x * subDt;
                    m_particles[i].velocity.y += m_particles[i].acceleration.y * subDt;
                    m_particles[i].velocity.z += m_particles[i].acceleration.z * subDt;
                }

                // ダンピング（空気抵抗的な減衰。0.98=毎フレーム2%減速）
                m_particles[i].velocity.x *= damping;
                m_particles[i].velocity.y *= damping;
                m_particles[i].velocity.z *= damping;

                // 速度クランプ（累積による暴走を防ぐ安全装置）
                m_particles[i].velocity.x = fmaxf(-maxVel, fminf(maxVel, m_particles[i].velocity.x));
                m_particles[i].velocity.y = fmaxf(-maxVel, fminf(maxVel, m_particles[i].velocity.y));
                m_particles[i].velocity.z = fmaxf(-maxVel, fminf(maxVel, m_particles[i].velocity.z));

                // 速度 × 時間 = 位置の変化量
                m_particles[i].position.x += m_particles[i].velocity.x * subDt;
                m_particles[i].position.y += m_particles[i].velocity.y * subDt;
                m_particles[i].position.z += m_particles[i].velocity.z * subDt;

                // NaN（非数値）検出 → 全リセット
                // SPHは粒子が重なると数値が発散することがある
                if (!isfinite(m_particles[i].position.x) || !isfinite(m_particles[i].position.y) || !isfinite(m_particles[i].position.z)) {
                    m_particles[i].position = { 0, 0, 0 };
                    m_particles[i].velocity = { 0, 0, 0 };
                    m_particles[i].acceleration = { 0, 0, 0 };
                }

                // 境界の壁で跳ね返す
                EnforceBoundary(m_particles[i]);
            }

            // --- 各サブステップでコリジョン実行 ---
            // サブステップごとにやらないと、移動→衝突→移動→衝突 の間で
            // 壁をすり抜ける可能性がある
            if (m_mapCollisionEnabled) {
                CollideWithMap();
            }
            if (m_playerCollisionEnabled) {
                CollideWithPlayers();
            }

        } // end subSteps
    }

    // ============================================================
    // Update — 毎フレーム呼ばれる更新処理
    //
    // やること:
    //   1. 寿命切れパーティクルを削除
    //   2. SPH物理シミュレーションを実行
    //
    // 寿命管理:
    //   各パーティクルのlifetimeを毎フレーム減らし、
    //   0以下になったら配列の最後の要素と入れ替えて削除
    //   （swap-and-pop: vectorの高速削除テクニック）
    // ============================================================
    void SPHFluid::Update(ID3D11DeviceContext* ctx, float dt) {
        if (m_particleCount == 0) return;

        // 寿命管理: 期限切れパーティクルを削除
        for (size_t i = 0; i < m_particles.size(); ) {
            m_particles[i].lifetime -= dt;
            if (m_particles[i].lifetime <= 0) {
                // swap-and-pop: 最後の要素と入れ替えてpop_back
                // erase()より圧倒的に高速（O(1) vs O(n)）
                m_particles[i] = m_particles.back();
                m_particles.pop_back();
                m_particleCount--;
            } else {
                ++i;
            }
        }
        m_params.particleCount = m_particleCount;
        if (m_particleCount == 0) return;

        // SPH物理シミュレーション実行
        SimulateCPU(dt);
    }

    // ============================================================
    // Draw — 描画のエントリーポイント
    //
    // スクリーンスペースが有効 → DrawScreenSpace（5パス描画）
    // 無効 or 初期化失敗      → DrawParticles（フォールバック）
    // ============================================================
    void SPHFluid::Draw(ID3D11DeviceContext* ctx) {
        // デバッグログ（最初の10フレームと、以降は600フレームに1回）
        static int drawLog = 0;
        if (drawLog < 10 || drawLog % 600 == 0) {
            char buf[256];
            sprintf_s(buf, "SPHFluid::Draw init=%d cnt=%u ss=%d ssInit=%d\n",
                m_initialized ? 1 : 0, m_particleCount, m_screenSpaceEnabled ? 1 : 0, m_ssInitialized ? 1 : 0);
            OutputDebugStringA(buf);
        }
        drawLog++;

        if (!m_initialized || m_particleCount == 0) return;

        // GPU資源の遅延初期化（最初のDraw時だけ実行）
        if (m_screenSpaceEnabled && !m_ssInitialized) {
            if (!InitializeScreenSpace()) {
                OutputDebugStringA("SPHFluid: SS init FAILED! Disabling.\n");
                m_screenSpaceEnabled = false;
            }
        }

        // スクリーンスペース描画 or フォールバック
        if (m_screenSpaceEnabled && m_ssInitialized && m_pDepthVS && m_depthRT) {
            static int ssLog = 0;
            if (ssLog < 5) {
                char buf[512];
                sprintf_s(buf, "SPHFluid::DrawSS cnt=%u p0=(%.1f,%.1f,%.1f) cam=(%.1f,%.1f,%.1f)->at(%.1f,%.1f,%.1f) fov=%.2f\n",
                    m_particleCount,
                    m_particles[0].position.x, m_particles[0].position.y, m_particles[0].position.z,
                    m_camPos.x, m_camPos.y, m_camPos.z,
                    m_camAt.x, m_camAt.y, m_camAt.z, m_fov);
                OutputDebugStringA(buf);
                ssLog++;
            }
            DrawScreenSpace(ctx);
        } else {
            DrawParticles(ctx);
        }
    }

    // ============================================================
    // DrawScreenSpace — スクリーンスペース流体レンダリング本体
    //
    // 5つのパスで「粒の集まり」→「滑らかな水面」に変換する
    //
    // パス1: 深度描画     → 各粒の「カメラからの距離」をテクスチャに記録
    // パス2: 水平ブラー1  → 深度をぼかして粒同士をつなげる（水平方向）
    // パス3: 垂直ブラー1  → 同上（垂直方向）
    // パス2b: 水平ブラー2 → もう一度ぼかす（到達範囲を広げる）
    // パス3b: 垂直ブラー2 → 同上
    // パス4: 厚み描画     → 粒の重なり具合を記録（色の濃淡に使う）
    // パス5: 最終合成     → 深度と厚みから法線・色・透明度を計算して画面に描画
    //
    // 重要な工夫:
    //   - 描画前に全GPUステートを保存し、描画後に復帰
    //     → 既存のレンダリングパイプラインに一切影響を与えない
    // ============================================================
    void SPHFluid::DrawScreenSpace(ID3D11DeviceContext* ctx) {

        // ==========================================
        // ステート保存（描画後に元に戻すため）
        // ==========================================
        // 流体描画は独自のシェーダー・ブレンド・レンダーターゲットを使うので
        // 描画前に「今の設定」を全部保存して、描画後に元に戻す
        // これをやらないとゲームの他の描画が壊れる

        ComPtr<ID3D11RenderTargetView> prevRTV;
        ComPtr<ID3D11DepthStencilView> prevDSV;
        ctx->OMGetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.GetAddressOf());

        ComPtr<ID3D11BlendState> prevBlend; FLOAT prevBF[4]; UINT prevSM;
        ctx->OMGetBlendState(prevBlend.GetAddressOf(), prevBF, &prevSM);

        ComPtr<ID3D11DepthStencilState> prevDS; UINT prevSR;
        ctx->OMGetDepthStencilState(prevDS.GetAddressOf(), &prevSR);

        ComPtr<ID3D11RasterizerState> prevRS;
        ctx->RSGetState(prevRS.GetAddressOf());

        D3D11_VIEWPORT prevVP; UINT numVP = 1;
        ctx->RSGetViewports(&numVP, &prevVP);

        ComPtr<ID3D11VertexShader> prevVS;
        ComPtr<ID3D11PixelShader>  prevPS;
        ComPtr<ID3D11InputLayout>  prevIL;
        D3D11_PRIMITIVE_TOPOLOGY   prevTopo;
        ctx->VSGetShader(prevVS.GetAddressOf(), nullptr, nullptr);
        ctx->PSGetShader(prevPS.GetAddressOf(), nullptr, nullptr);
        ctx->IAGetInputLayout(prevIL.GetAddressOf());
        ctx->IAGetPrimitiveTopology(&prevTopo);

        ComPtr<ID3D11Buffer> prevVSCB0, prevVSCB1, prevPSCB0;
        ctx->VSGetConstantBuffers(0, 1, prevVSCB0.GetAddressOf());
        ctx->VSGetConstantBuffers(1, 1, prevVSCB1.GetAddressOf());
        ctx->PSGetConstantBuffers(0, 1, prevPSCB0.GetAddressOf());

        ComPtr<ID3D11SamplerState> prevSamp0, prevSamp1;
        ctx->PSGetSamplers(0, 1, prevSamp0.GetAddressOf());
        ctx->PSGetSamplers(1, 1, prevSamp1.GetAddressOf());

        ComPtr<ID3D11ShaderResourceView> prevSRV0, prevSRV1, prevSRV2;
        ctx->PSGetShaderResources(0, 1, prevSRV0.GetAddressOf());
        ctx->PSGetShaderResources(1, 1, prevSRV1.GetAddressOf());
        ctx->PSGetShaderResources(2, 1, prevSRV2.GetAddressOf());

        ComPtr<ID3D11Buffer> prevVB0, prevVB1, prevIB;
        UINT prevStride0, prevStride1, prevOff0, prevOff1;
        DXGI_FORMAT prevIBFmt; UINT prevIBOff;
        ctx->IAGetVertexBuffers(0, 1, prevVB0.GetAddressOf(), &prevStride0, &prevOff0);
        ctx->IAGetVertexBuffers(1, 1, prevVB1.GetAddressOf(), &prevStride1, &prevOff1);
        ctx->IAGetIndexBuffer(prevIB.GetAddressOf(), &prevIBFmt, &prevIBOff);

        // ==========================================
        // 行列の準備
        // ==========================================
        // View行列: カメラの位置と向きから「ワールド→カメラ」の変換
        // Proj行列: カメラ視野から「カメラ→画面」の変換
        XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&m_camPos), XMLoadFloat3(&m_camAt), XMLoadFloat3(&m_camUp));
        XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearZ, m_farZ);

        // パーティクル位置をGPUに送る
        UpdateInstanceBuffer(ctx);

        // ビューポート設定（描画領域のサイズ）
        D3D11_VIEWPORT fullVP = {};
        fullVP.Width = (float)m_screenWidth;
        fullVP.Height = (float)m_screenHeight;
        fullVP.MaxDepth = 1.0f;

        D3D11_VIEWPORT halfVP = {};  // 厚みパス用（半解像度）
        halfVP.Width = (float)(m_screenWidth / 2);
        halfVP.Height = (float)(m_screenHeight / 2);
        halfVP.MaxDepth = 1.0f;

        // 全パスで共通の設定
        ctx->RSSetState(m_pNoCullRS.Get());                    // 裏面も描画
        ctx->OMSetDepthStencilState(m_pDepthDisabledState.Get(), 0);  // 深度テストなし

        ID3D11ShaderResourceView* nullSRV = nullptr;
        ID3D11RenderTargetView* nullRTV = nullptr;

        // ==========================================
        // パス1: 深度描画
        // ==========================================
        // 各パーティクルの「カメラからの距離」をテクスチャに描く
        // ビルボード（カメラに正対する板）として描き、
        // ピクセルシェーダーで球体の膨らみを加えた深度を出力
        //
        // MINブレンド: 同じ画面位置に複数の粒が重なったら
        //   一番手前（カメラに近い）の深度だけ残す
        //   → 描画順に関係なく正確な水面の最前面が得られる
        {
            // RTを1.0（最遠値）でクリア（MINブレンドなので初期値は最大）
            float clr[4] = { 1.0f, 0, 0, 0 };
            ctx->ClearRenderTargetView(m_depthRT->GetRTV(), clr);
            ID3D11RenderTargetView* rtv = m_depthRT->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);
            ctx->OMSetBlendState(m_pMinBlendState.Get(), nullptr, 0xFFFFFFFF);
            ctx->RSSetViewports(1, &fullVP);

            // カメラ情報をシェーダーに送る
            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pCameraCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBCamera* cb = (CBCamera*)mp.pData;
                // Transpose: DirectXMath(行優先) → HLSL(列優先) の変換
                XMStoreFloat4x4(&cb->View, XMMatrixTranspose(view));
                XMStoreFloat4x4(&cb->Proj, XMMatrixTranspose(proj));
                // 描画時の粒サイズ（物理サイズより大きくして隣の粒と重なりを作る）
                cb->PointRadius = m_particleScale * 6.0f;
                cb->ScreenSize = { (float)m_screenWidth, (float)m_screenHeight };
                cb->_pad0 = m_farZ;
                ctx->Unmap(m_pCameraCB.Get(), 0);
            }
            ctx->VSSetConstantBuffers(0, 1, m_pCameraCB.GetAddressOf());
            ctx->PSSetConstantBuffers(0, 1, m_pCameraCB.GetAddressOf());

            // 頂点バッファの設定
            // slot0: 四角形（全パーティクル共通の板ポリゴン）
            // slot1: インスタンスバッファ（各パーティクルのワールド座標）
            UINT strides[2] = { sizeof(XMFLOAT2), sizeof(XMFLOAT3) };
            UINT offsets[2] = { 0, 0 };
            ID3D11Buffer* vbs[2] = { m_pBillboardVB.Get(), m_pInstanceBuffer.Get() };
            ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
            ctx->IASetIndexBuffer(m_pBillboardIB.Get(), DXGI_FORMAT_R32_UINT, 0);
            ctx->IASetInputLayout(m_pBillboardLayout.Get());
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(m_pDepthVS.Get(), nullptr, 0);
            ctx->PSSetShader(m_pDepthPS.Get(), nullptr, 0);

            // DrawIndexedInstanced: 6頂点の四角形 × パーティクル数 を1回で描画
            // インスタンシングにより、四角形メッシュは1つだけで全粒を描画できる
            ctx->DrawIndexedInstanced(6, m_particleCount, 0, 0, 0);

            // RTをアンバインド（次のパスでSRVとして読むため）
            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
        }

        // ==========================================
        // パス2: 水平ブラー（1回目）
        // ==========================================
        // 深度テクスチャを左右方向にぼかす
        // バイラテラルブラー: 深度差が大きい境界はぼかさない（エッジ保持）
        //
        // なぜ水平と垂直を分けるか:
        //   N×Nのブラーを直接やるとN²回のテクスチャ読み。
        //   水平N回 + 垂直N回 = 2N回で同じ結果。圧倒的に高速。
        {
            ctx->PSSetShaderResources(0, 1, &nullSRV);  // 前のSRVをクリア

            float clr[4] = { 1.0f, 0, 0, 0 };
            ctx->ClearRenderTargetView(m_blurRT1->GetRTV(), clr);

            ID3D11RenderTargetView* rtv = m_blurRT1->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);
            ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);  // ブレンドなし（上書き）
            ctx->RSSetViewports(1, &fullVP);

            // ブラーパラメータをシェーダーに送る
            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pBlurCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBBlur* cb = (CBBlur*)mp.pData;
                cb->BlurDir = { 2.0f / m_screenWidth, 0.0f };  // 水平方向（2ピクセル飛ばし）
                cb->BlurScale = 0.5f;           // 空間重みの広がり
                cb->BlurDepthFalloff = 100.0f;   // 深度差による重み減衰の強さ
                cb->FilterRadius = 5;
                cb->_pad1 = { 0, 0, 0 };
                ctx->Unmap(m_pBlurCB.Get(), 0);
            }
            ctx->PSSetConstantBuffers(0, 1, m_pBlurCB.GetAddressOf());

            // パス1の深度テクスチャを入力として読む
            ID3D11ShaderResourceView* srv = m_depthRT->GetSRV();
            ctx->PSSetShaderResources(0, 1, &srv);
            ctx->PSSetSamplers(0, 1, m_pPointSampler.GetAddressOf());

            // VS_Quad: 3頂点で画面全体を覆う三角形を生成（頂点バッファ不要）
            ctx->IASetInputLayout(nullptr);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(m_pQuadVS.Get(), nullptr, 0);
            ctx->PSSetShader(m_pBilateralBlurPS.Get(), nullptr, 0);
            ctx->Draw(3, 0);  // 3頂点 = 1三角形で画面全体をカバー

            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
            ctx->PSSetShaderResources(0, 1, &nullSRV);
        }

        // ==========================================
        // パス3: 垂直ブラー（1回目）
        // ==========================================
        // パス2の結果を上下方向にぼかす
        // パス2→パス3で「水平→垂直」の分離型ブラーが完成
        {
            float clr[4] = { 1.0f, 0, 0, 0 };
            ctx->ClearRenderTargetView(m_blurRT2->GetRTV(), clr);

            ID3D11RenderTargetView* rtv = m_blurRT2->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);

            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pBlurCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBBlur* cb = (CBBlur*)mp.pData;
                cb->BlurDir = { 0.0f, 2.0f / m_screenHeight };  // 垂直方向
                cb->BlurScale = 0.5f;
                cb->BlurDepthFalloff = 100.0f;
                cb->FilterRadius = 5;
                cb->_pad1 = { 0, 0, 0 };
                ctx->Unmap(m_pBlurCB.Get(), 0);
            }

            // パス2の結果を入力
            ID3D11ShaderResourceView* srv = m_blurRT1->GetSRV();
            ctx->PSSetShaderResources(0, 1, &srv);

            ctx->Draw(3, 0);

            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
            ctx->PSSetShaderResources(0, 1, &nullSRV);
        }

        // ==========================================
        // パス2b: 水平ブラー（2回目）
        // ==========================================
        // 1往復のブラーだと粒同士の隙間が埋まりきらないので
        // もう1往復追加して到達範囲を広げる
        // → 粒々感がさらに消えて滑らかな水面になる
        {
            float clr[4] = { 1.0f, 0, 0, 0 };
            ctx->ClearRenderTargetView(m_blurRT1->GetRTV(), clr);
            ID3D11RenderTargetView* rtv = m_blurRT1->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);

            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pBlurCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBBlur* cb = (CBBlur*)mp.pData;
                cb->BlurDir = { 2.0f / m_screenWidth, 0.0f };
                cb->BlurScale = 0.5f;
                cb->BlurDepthFalloff = 100.0f;
                cb->FilterRadius = 5;
                cb->_pad1 = { 0, 0, 0 };
                ctx->Unmap(m_pBlurCB.Get(), 0);
            }

            // パス3の結果を入力（ピンポン: RT1→RT2→RT1→RT2）
            ID3D11ShaderResourceView* srv = m_blurRT2->GetSRV();
            ctx->PSSetShaderResources(0, 1, &srv);
            ctx->Draw(3, 0);

            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
            ctx->PSSetShaderResources(0, 1, &nullSRV);
        }

        // ==========================================
        // パス3b: 垂直ブラー（2回目）
        // ==========================================
        // これでブラー合計4パス完了。結果はblurRT2に入る
        {
            float clr[4] = { 1.0f, 0, 0, 0 };
            ctx->ClearRenderTargetView(m_blurRT2->GetRTV(), clr);
            ID3D11RenderTargetView* rtv = m_blurRT2->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);

            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pBlurCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBBlur* cb = (CBBlur*)mp.pData;
                cb->BlurDir = { 0.0f, 2.0f / m_screenHeight };
                cb->BlurScale = 0.5f;
                cb->BlurDepthFalloff = 100.0f;
                cb->FilterRadius = 5;
                cb->_pad1 = { 0, 0, 0 };
                ctx->Unmap(m_pBlurCB.Get(), 0);
            }

            ID3D11ShaderResourceView* srv = m_blurRT1->GetSRV();
            ctx->PSSetShaderResources(0, 1, &srv);
            ctx->Draw(3, 0);

            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
            ctx->PSSetShaderResources(0, 1, &nullSRV);
        }

        // ==========================================
        // パス4: 厚み描画
        // ==========================================
        // 各パーティクルの「厚み」を加算ブレンドで描画
        // 粒が重なるほど厚み値が大きくなる
        // → 最終パスで「厚い水は濃い青」「薄い水は水色」と色を変える
        //
        // 半解像度: 厚みは精密な値が不要なのでGPU負荷を1/4に削減
        // 加算ブレンド: 新しい値 + 既存の値 = 累積厚み
        {
            float clr[4] = { 0, 0, 0, 0 };  // 厚みゼロでクリア
            ctx->ClearRenderTargetView(m_thicknessRT->GetRTV(), clr);

            ID3D11RenderTargetView* rtv = m_thicknessRT->GetRTV();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);
            ctx->OMSetBlendState(m_pAdditiveBlendState.Get(), nullptr, 0xFFFFFFFF);
            ctx->RSSetViewports(1, &halfVP);  // 半解像度

            ctx->VSSetConstantBuffers(0, 1, m_pCameraCB.GetAddressOf());

            // パス1と同じビルボード描画（シェーダーだけ違う）
            UINT strides[2] = { sizeof(XMFLOAT2), sizeof(XMFLOAT3) };
            UINT offsets[2] = { 0, 0 };
            ID3D11Buffer* vbs[2] = { m_pBillboardVB.Get(), m_pInstanceBuffer.Get() };
            ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
            ctx->IASetIndexBuffer(m_pBillboardIB.Get(), DXGI_FORMAT_R32_UINT, 0);
            ctx->IASetInputLayout(m_pBillboardLayout.Get());
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(m_pDepthVS.Get(), nullptr, 0);     // 頂点シェーダーは深度パスと共通
            ctx->PSSetShader(m_pThicknessPS.Get(), nullptr, 0);  // ピクセルシェーダーだけ違う
            ctx->DrawIndexedInstanced(6, m_particleCount, 0, 0, 0);

            ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
            ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
        }

        // ==========================================
        // パス5: 最終合成
        // ==========================================
        // ブラー済み深度と厚みから水の色を計算して画面に描画
        //
        // シェーダー(fluid_final.hlsl)がやること:
        //   1. 深度からビュー空間位置を復元（逆プロジェクション）
        //   2. 隣接ピクセルとの差分から法線を計算
        //   3. フレネル反射（浅い角度ほど反射が強い）
        //   4. スペキュラハイライト（キラキラ）
        //   5. 厚みに応じた色の補間（薄い=水色、厚い=深い青）
        //   6. アルファブレンドで背景と合成
        //
        // シーン深度テクスチャ(t2)をバインドし、壁の奥の水を棄却する
        {
            // 描画先を元のバックバッファに戻す
            ID3D11RenderTargetView* rtv = prevRTV.Get();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);
            ctx->OMSetBlendState(m_pAlphaBlendState.Get(), nullptr, 0xFFFFFFFF);
            ctx->OMSetDepthStencilState(m_pDepthDisabledState.Get(), 0);
            ctx->RSSetViewports(1, &fullVP);

            // 最終合成パラメータをシェーダーに送る
            {
                D3D11_MAPPED_SUBRESOURCE mp;
                ctx->Map(m_pFinalCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
                CBFinal* cb = (CBFinal*)mp.pData;
                // 逆プロジェクション行列（深度→ビュー空間位置の復元に使う）
                XMStoreFloat4x4(&cb->InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, proj)));
                cb->TexelSize = { 1.0f / m_screenWidth, 1.0f / m_screenHeight };
                cb->WaterAlpha = 0.8f;
                cb->FresnelPower = 4.0f;
                cb->WaterColor = { m_particleColor.x, m_particleColor.y, m_particleColor.z };
                cb->_pad2 = m_nearZ;
                cb->WaterDeepColor = { 0.05f, 0.2f, 0.5f };
                cb->_pad3 = m_farZ;
                cb->LightDir = { 0.3f, 1.0f, 0.5f };
                cb->SpecPower = 64.0f;
                cb->AbsorptionCoeff = { 0.4f, 0.08f, 0.04f };
                cb->RefractScale = 0.02f;
                ctx->Unmap(m_pFinalCB.Get(), 0);
            }
            ctx->PSSetConstantBuffers(0, 1, m_pFinalCB.GetAddressOf());

            // 入力テクスチャ:
            //   t0: ブラー済み深度（blurRT2 = 4パスブラー後の結果）
            //   t1: 厚みテクスチャ
            //   t2: シーン深度テクスチャ（壁の奥の水を棄却するため）
            ID3D11ShaderResourceView* sceneSRV = m_sceneDepthSRV.Get();
            ID3D11ShaderResourceView* srvs[3] = {
                m_blurRT2->GetSRV(),
                m_thicknessRT->GetSRV(),
                sceneSRV  // シーン深度
            };
            ctx->PSSetShaderResources(0, 3, srvs);
            ID3D11SamplerState* samps[2] = { m_pPointSampler.Get(), m_pLinearSampler.Get() };
            ctx->PSSetSamplers(0, 2, samps);

            ctx->IASetInputLayout(nullptr);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(m_pQuadVS.Get(), nullptr, 0);
            ctx->PSSetShader(m_pFinalPS.Get(), nullptr, 0);
            ctx->Draw(3, 0);

            // テクスチャをアンバインド
            ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
            ctx->PSSetShaderResources(0, 3, nullSRVs);
        }

        // ==========================================
        // ステート復帰（保存した設定を全部元に戻す）
        // ==========================================
        ctx->OMSetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.Get());
        ctx->OMSetBlendState(prevBlend.Get(), prevBF, prevSM);
        ctx->OMSetDepthStencilState(prevDS.Get(), prevSR);
        ctx->RSSetState(prevRS.Get());
        ctx->RSSetViewports(1, &prevVP);
        ctx->IASetInputLayout(prevIL.Get());
        ctx->IASetPrimitiveTopology(prevTopo);
        ctx->VSSetShader(prevVS.Get(), nullptr, 0);
        ctx->PSSetShader(prevPS.Get(), nullptr, 0);

        ID3D11Buffer* vb0 = prevVB0.Get();
        ctx->IASetVertexBuffers(0, 1, &vb0, &prevStride0, &prevOff0);
        ID3D11Buffer* vb1 = prevVB1.Get();
        ctx->IASetVertexBuffers(1, 1, &vb1, &prevStride1, &prevOff1);
        ctx->IASetIndexBuffer(prevIB.Get(), prevIBFmt, prevIBOff);

        ctx->VSSetConstantBuffers(0, 1, prevVSCB0.GetAddressOf());
        ctx->VSSetConstantBuffers(1, 1, prevVSCB1.GetAddressOf());
        ctx->PSSetConstantBuffers(0, 1, prevPSCB0.GetAddressOf());
        ctx->PSSetSamplers(0, 1, prevSamp0.GetAddressOf());
        ctx->PSSetSamplers(1, 1, prevSamp1.GetAddressOf());
        ID3D11ShaderResourceView* rSRV[3] = { prevSRV0.Get(), prevSRV1.Get(), prevSRV2.Get() };
        ctx->PSSetShaderResources(0, 3, rSRV);

        // デバッグログ（5秒に1回くらい）
        static int frameCount = 0;
        frameCount++;
        if (frameCount % 300 == 0 && m_particleCount > 0) {
            char buf[256];
            sprintf_s(buf, "SPHFluid: Frame %d, particles=%u, scale=%.3f, radius=%.3f\n",
                frameCount, m_particleCount, m_particleScale, m_particleScale * 6.0f);
            OutputDebugStringA(buf);
        }
    }

    // ============================================================
    // DrawParticles — フォールバック描画（スクリーンスペースが使えない場合）
    // 現在は何もしない（将来的にはシンプルな点描画などを実装予定）
    // ============================================================
    void SPHFluid::DrawParticles(ID3D11DeviceContext* ctx) {
        (void)ctx;
        static int log = 0;
        if (log++ < 3) OutputDebugStringA("SPHFluid::DrawParticles fallback\n");
    }

} // namespace Engine
