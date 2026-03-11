//==============================================================================
//  File   : timer.h
//  Brief  : システムタイマー - 高精度パフォーマンスカウンタ
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#ifndef SYSTEM_TIMER_H_
#define SYSTEM_TIMER_H_

//==========================================================
// システムタイマー関数
//==========================================================

// --- 初期化・制御 ---
void SystemTimer_Initialize(void);  // システムタイマーの初期化
void SystemTimer_Reset(void);       // システムタイマーのリセット
void SystemTimer_Start(void);       // システムタイマーのスタート
void SystemTimer_Stop(void);        // システムタイマーのストップ
void SystemTimer_Advance(void);     // システムタイマーを0.1秒進める

// --- 時間取得 ---
double SystemTimer_GetTime(void);         // 計測時間の取得
double SystemTimer_GetAbsoluteTime(void); // 現在の時間を取得
float  SystemTimer_GetElapsedTime(void);  // 経過時間の取得

// --- 状態確認 ---
bool SystemTimer_IsStoped(void);  // システムタイマーが止まっているか？

// --- スレッド制御 ---
void LimitThreadAffinityToCurrentProc(void);  // 現在のスレッドを1のプロセッサに制限

#endif // SYSTEM_TIMER_H_
