//==============================================================================
//  File   : udp_network.h
//  Brief  : UDP送受信ラッパークラス（WinSock2使用）
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/11
//------------------------------------------------------------------------------
// ゲーム通信ポートと探索用ポートの2つを使い分ける
//==============================================================================
#pragma once

#include "network_common.h"  // ポート定数やパケット構造体
#include <winsock2.h>        // WinSock2 API
#include <ws2tcpip.h>        // inet_pton, inet_ntop など
#include <string>
#include <vector>
#include <random>            // ランダムポート選択用

// WinSock2ライブラリを自動リンク
#pragma comment(lib, "Ws2_32.lib")

//============================================================
// UdpNetwork クラス
// 1つのUDPソケットをラップし、初期化・送信・受信・クローズを提供する。
// NetworkManagerが「ゲーム通信用」と「探索用」の2インスタンスを持つ。
//============================================================
class UdpNetwork {
public:
    UdpNetwork();
    ~UdpNetwork();

    //----------------------------------------------------------
    // 初期化系
    //----------------------------------------------------------

    // 指定ポートでソケットを作成しbindする（通常のゲーム通信用）
    // bind_port=0 の場合はOSが自動でポートを割り当てる
    bool initialize(int bind_port = NET_PORT);

    // ブロードキャスト送信が可能なソケットとして初期化する（探索用）
    // initialize() を呼んだ後に SO_BROADCAST オプションを設定する
    bool initialize_broadcast(int bind_port = DISCOVERY_PORT);

    // ファイアウォール回避用：ランダムな空きポートで初期化する
    // DYNAMIC_PORT_RANGES テーブルから空きポートを探す
    bool initialize_dynamic_port();

    //----------------------------------------------------------
    // 送信系
    //----------------------------------------------------------

    // 指定IPアドレスとポートにデータを送信する
    // 戻り値: 送信バイト数がlenと一致したらtrue
    bool send_to(const std::string& ip, int port, const void* data, int len);

    // 255.255.255.255 へブロードキャスト送信する（LAN内全体に届く）
    // ホスト探索のDISCOVERパケット送信に使用
    bool send_broadcast(int port, const void* data, int len);

    //----------------------------------------------------------
    // 受信系
    //----------------------------------------------------------

    // select()でソケットを監視し、データがあれば受信する
    // timeout_ms: 待機時間（ミリ秒）。0なら即座にチェックして戻る
    // 戻り値: 受信バイト数（0=データなし、負=エラー）
    int poll_recv(char* buffer, int bufferSize,
        std::string& from_ip, int& from_port,
        int timeout_ms = 0);

    //----------------------------------------------------------
    // 終了処理
    //----------------------------------------------------------

    // ソケットを閉じてWinSockをクリーンアップする
    void close_socket();

    //----------------------------------------------------------
    // 状態確認
    //----------------------------------------------------------

    // ソケットが有効（初期化済みで閉じていない）か判定する
    bool is_valid() const;

    //----------------------------------------------------------
    // ユーティリティ（静的メソッド）
    //----------------------------------------------------------

    // 指定ポートが使用可能か確認する（bindテストで判定）
    static bool is_port_available(int port);

    // ランダムに空きポートを1つ探して返す（見つからなければ-1）
    static int get_random_available_port();

    // 指定数だけ空きポートを探してリストで返す
    static std::vector<int> get_available_port_list(int count = 10);

    // このマシンのローカルIPアドレスを文字列で返す（例: "192.168.1.10"）
    static std::string get_local_ip();

    //----------------------------------------------------------
    // アクセサ
    //----------------------------------------------------------

    // 現在bindしているポート番号を取得する
    int get_current_port() const { return current_port; }

private:
    SOCKET sock = INVALID_SOCKET;   // WinSockソケットハンドル
    bool is_broadcast_socket = false; // ブロードキャスト対応ソケットかどうか
    int current_port = 0;            // 現在bindしているポート番号

    // ランダムポート生成用の乱数エンジン（静的メンバ、全インスタンス共通）
    static std::random_device rd;
    static std::mt19937 gen;
};
