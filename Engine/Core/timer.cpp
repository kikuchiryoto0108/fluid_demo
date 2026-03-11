//==============================================================================
//  File   : timer.cpp
//  Brief  : システムタイマー - 高精度パフォーマンスカウンタ実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include <Windows.h>

//==========================================================
// グローバル変数
//==========================================================
static bool     g_bTimerStopped = true;  // ストップフラグ
static LONGLONG g_TicksPerSec = 0;     // 1秒間の計測精度
static LONGLONG g_StopTime = 0;     // ストップした時間
static LONGLONG g_LastElapsedTime = 0;     // 最後に記録した更新時間
static LONGLONG g_BaseTime = 0;     // 基本時間

//==========================================================
// 内部関数プロトタイプ
//==========================================================
static LARGE_INTEGER GetAdjustedCurrentTime(void);

//==========================================================
// SystemTimer_Initialize - システムタイマーの初期化
//==========================================================
void SystemTimer_Initialize(void) {
    g_bTimerStopped = true;
    g_TicksPerSec = 0;
    g_StopTime = 0;
    g_LastElapsedTime = 0;
    g_BaseTime = 0;

    // 高解像度パフォーマンスカウンタ周期の取得
    LARGE_INTEGER ticksPerSec = { 0 };
    QueryPerformanceFrequency(&ticksPerSec);
    g_TicksPerSec = ticksPerSec.QuadPart;
}

//==========================================================
// SystemTimer_Reset - システムタイマーのリセット
//==========================================================
void SystemTimer_Reset(void) {
    LARGE_INTEGER time = GetAdjustedCurrentTime();

    g_BaseTime = g_LastElapsedTime = time.QuadPart;
    g_StopTime = 0;
    g_bTimerStopped = false;
}

//==========================================================
// SystemTimer_Start - システムタイマーのスタート
//==========================================================
void SystemTimer_Start(void) {
    // 現在の時間を取得
    LARGE_INTEGER time = { 0 };
    QueryPerformanceCounter(&time);

    // 今まで計測がストップしていたら
    if (g_bTimerStopped) {
        // 止まっていた時間を差し引いて基本時間を更新
        g_BaseTime += time.QuadPart - g_StopTime;
    }

    g_StopTime = 0;
    g_LastElapsedTime = time.QuadPart;
    g_bTimerStopped = false;
}

//==========================================================
// SystemTimer_Stop - システムタイマーのストップ
//==========================================================
void SystemTimer_Stop(void) {
    if (g_bTimerStopped) return;

    LARGE_INTEGER time = { 0 };
    QueryPerformanceCounter(&time);

    g_LastElapsedTime = g_StopTime = time.QuadPart;  // 停止時間を記録
    g_bTimerStopped = true;
}

//==========================================================
// SystemTimer_Advance - システムタイマーを0.1秒進める
//==========================================================
void SystemTimer_Advance(void) {
    g_StopTime += g_TicksPerSec / 10;
}

//==========================================================
// SystemTimer_GetTime - 計測時間の取得
//==========================================================
double SystemTimer_GetTime(void) {
    LARGE_INTEGER time = GetAdjustedCurrentTime();

    return (double)(time.QuadPart - g_BaseTime) / (double)g_TicksPerSec;
}

//==========================================================
// SystemTimer_GetAbsoluteTime - 現在の時間を取得
//==========================================================
double SystemTimer_GetAbsoluteTime(void) {
    LARGE_INTEGER time = { 0 };
    QueryPerformanceCounter(&time);

    return time.QuadPart / (double)g_TicksPerSec;
}

//==========================================================
// SystemTimer_GetElapsedTime - 経過時間の取得
//
// 注意: タイマーが正確であることを保証するために、更新時間を
// ゼロにクランプする。elapsed_timeは、プロセッサが省電力モード
// に入るか、何らかの形で別のプロセッサにシャッフルされると、
// 正の範囲外になる可能性がある。
//==========================================================
float SystemTimer_GetElapsedTime(void) {
    LARGE_INTEGER time = GetAdjustedCurrentTime();

    double elapsed_time = (float)((double)(time.QuadPart - g_LastElapsedTime) / (double)g_TicksPerSec);
    g_LastElapsedTime = time.QuadPart;

    if (elapsed_time < 0.0f) {
        elapsed_time = 0.0f;
    }

    return (float)elapsed_time;
}

//==========================================================
// SystemTimer_IsStoped - システムタイマーが止まっているか？
//==========================================================
bool SystemTimer_IsStoped(void) {
    return g_bTimerStopped;
}

//==========================================================
// LimitThreadAffinityToCurrentProc - スレッドを1プロセッサに制限
//==========================================================
void LimitThreadAffinityToCurrentProc(void) {
    HANDLE hCurrentProcess = GetCurrentProcess();

    // プロセスのアフィニティマスクを取得
    DWORD_PTR dwProcessAffinityMask = 0;
    DWORD_PTR dwSystemAffinityMask = 0;

    if (GetProcessAffinityMask(hCurrentProcess, &dwProcessAffinityMask, &dwSystemAffinityMask) != 0 && dwProcessAffinityMask) {
        // プロセスが実行可能な最下位プロセッサを見つける
        DWORD_PTR dwAffinityMask = (dwProcessAffinityMask & ((~dwProcessAffinityMask) + 1));

        // スレッドが常に実行されるプロセッサとして設定
        HANDLE hCurrentThread = GetCurrentThread();
        if (INVALID_HANDLE_VALUE != hCurrentThread) {
            SetThreadAffinityMask(hCurrentThread, dwAffinityMask);
            CloseHandle(hCurrentThread);
        }
    }

    CloseHandle(hCurrentProcess);
}

//==========================================================
// GetAdjustedCurrentTime - 停止時間または現在時間を取得
//==========================================================
LARGE_INTEGER GetAdjustedCurrentTime(void) {
    LARGE_INTEGER time;
    if (g_StopTime != 0) {
        time.QuadPart = g_StopTime;
    } else {
        QueryPerformanceCounter(&time);
    }

    return time;
}
