/*********************************************************************
 * \file   debug_log.h
 * \brief  デバッグログユーティリティ
 *         OutputDebugStringA/Wを使用したデバッグ出力ヘルパー
 *         Debug/Release両方のビルドで動作
 *
 * \author Ryoto Kikuchi
 * \date   2025
 *********************************************************************/
#pragma once

#include <Windows.h>
#include <cstdio>
#include <cstdarg>

namespace Engine {

    //==========================================================
    // DebugLog - デバッグ出力ヘルパー（ANSI版）
    // 最大バッファサイズを超える場合は切り詰めて出力
    //==========================================================
    inline void DebugLog(const char* format, ...) {
        constexpr size_t BUFFER_SIZE = 2048;
        char buffer[BUFFER_SIZE];

        va_list args;
        va_start(args, format);
        int result = vsnprintf(buffer, BUFFER_SIZE, format, args);
        va_end(args);

        // バッファオーバーフローが発生した場合、終端を明示的に設定
        if (result < 0 || result >= static_cast<int>(BUFFER_SIZE)) {
            buffer[BUFFER_SIZE - 1] = '\0';
        }

        OutputDebugStringA(buffer);
    }

    //==========================================================
    // DebugLogW - デバッグ出力ヘルパー（ワイド文字版）
    //==========================================================
    inline void DebugLogW(const wchar_t* format, ...) {
        constexpr size_t BUFFER_SIZE = 2048;
        wchar_t buffer[BUFFER_SIZE];

        va_list args;
        va_start(args, format);
        int result = _vsnwprintf_s(buffer, BUFFER_SIZE, _TRUNCATE, format, args);
        va_end(args);

        // バッファオーバーフローが発生した場合、終端を明示的に設定
        if (result < 0) {
            buffer[BUFFER_SIZE - 1] = L'\0';
        }

        OutputDebugStringW(buffer);
    }

} // namespace Engine

//==========================================================
// 便利なマクロ（名前空間なしでも使用可能）
//==========================================================
#define DEBUG_LOG   Engine::DebugLog
#define DEBUG_LOG_W Engine::DebugLogW
