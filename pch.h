//==============================================================================
//  File   : pch.h
//  Brief  : プリコンパイル済みヘッダー - 共通インクルードファイル
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/11
//------------------------------------------------------------------------------
//
//==============================================================================
#ifndef PCH_H
#define PCH_H

//==========================================================
// Windowsヘッダー（Winsockを最初にインクルード）
//==========================================================
// WIN32_LEAN_AND_MEAN: 使用頻度の低いAPIを除外してコンパイル時間を短縮
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// NOMINMAX: Windows.hのmin/maxマクロを無効化（std::min/maxとの競合回避）
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>   // Winsock2 API（ネットワーク通信）
#include <ws2tcpip.h>   // TCP/IPユーティリティ関数
#include <windows.h>    // Windows API基本ヘッダー

//==========================================================
// DirectXヘッダー
//==========================================================
#include <d3d11.h>          // Direct3D 11 API
#include <d3dcompiler.h>    // シェーダーコンパイラ
#include <DirectXMath.h>    // SIMD数学ライブラリ
#include <wrl/client.h>     // ComPtr スマートポインタ

//==========================================================
// C/C++標準ライブラリ
//==========================================================
// --- 基本型・ユーティリティ ---
#include <cstdint>      // 固定幅整数型（int32_t, uint64_tなど）
#include <cstring>      // C文字列操作関数
#include <cmath>        // 数学関数

// --- コンテナ・文字列 ---
#include <string>       // std::string
#include <vector>       // std::vector
#include <unordered_map> // std::unordered_map（ハッシュマップ）

// --- メモリ管理・アルゴリズム ---
#include <memory>       // std::shared_ptr, std::unique_ptr
#include <algorithm>    // std::sort, std::findなど
#include <functional>   // std::function, std::bind

// --- マルチスレッド ---
#include <mutex>        // std::mutex（排他制御）
#include <atomic>       // std::atomic（アトミック操作）
#include <thread>       // std::thread（スレッド管理）

#endif // PCH_H
