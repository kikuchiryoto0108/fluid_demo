//==============================================================================
//  File   : udp_network.cpp
//  Brief  : udp_networkクラスの実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
// WinSock2を使ったUDPソケットの初期化・送受信・ポート管理
//==============================================================================
#include "pch.h"
#include "udp_network.h"
#include <iostream>
#include <cstring>

//============================================================
// 静的メンバの初期化（ランダムポート選択用の乱数エンジン）
//============================================================
std::random_device UdpNetwork::rd;
std::mt19937 UdpNetwork::gen(UdpNetwork::rd());

//============================================================
// コンストラクタ / デストラクタ
//============================================================

UdpNetwork::UdpNetwork() {}

// デストラクタでソケットを確実に閉じる
UdpNetwork::~UdpNetwork() { close_socket(); }

// ソケットが有効かどうかを返す
bool UdpNetwork::is_valid() const { return sock != INVALID_SOCKET; }

//============================================================
// initialize - 通常のUDPソケットを作成してbindする
//============================================================
bool UdpNetwork::initialize(int bind_port) {
    // WinSock2の初期化（バージョン2.2を要求）
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    // UDPソケットを作成
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket() failed\n";
        WSACleanup();
        return false;
    }

    // ソケットを指定ポートにバインドする
    // INADDR_ANY = すべてのネットワークインターフェースで待ち受ける
    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)bind_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed on port " << bind_port << "\n";
        closesocket(sock);
        sock = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    // バインド成功。現在のポート番号を記録する
    current_port = bind_port;

    // ブロッキングモードのまま使用する（poll_recvではselect()で制御）
    // mode=0 でブロッキングに設定
    u_long mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);

    is_broadcast_socket = false;
    return true;
}

//============================================================
// initialize_broadcast - ブロードキャスト対応ソケットとして初期化
// ホスト探索で255.255.255.255へ送信するために必要
//============================================================
bool UdpNetwork::initialize_broadcast(int bind_port) {
    // まず通常のソケットとして初期化する
    if (!initialize(bind_port)) return false;

    // SO_BROADCASTオプションを有効にする
    // これがないと255.255.255.255への送信が失敗する
    BOOL bOpt = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&bOpt, sizeof(bOpt)) == SOCKET_ERROR) {
        std::cerr << "setsockopt SO_BROADCAST failed\n";
        return false;
    }
    is_broadcast_socket = true;
    return true;
}

//============================================================
// initialize_dynamic_port - ランダムな空きポートで初期化する
// 固定ポートがファイアウォールでブロックされている場合のフォールバック
//============================================================
bool UdpNetwork::initialize_dynamic_port() {
    // 空いているポートをランダムに探す
    int port = get_random_available_port();
    if (port < 0) {
        std::cerr << "No available dynamic port found\n";
        return false;
    }

    std::cout << "Using dynamic port: " << port << std::endl;
    return initialize(port);
}

//============================================================
// send_to - 指定アドレスにUDPパケットを送信する
//============================================================
bool UdpNetwork::send_to(const std::string& ip, int port, const void* data, int len) {
    if (sock == INVALID_SOCKET) return false;

    // 送信先アドレスを設定
    sockaddr_in dest;
    ZeroMemory(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons((unsigned short)port);
    inet_pton(AF_INET, ip.c_str(), &dest.sin_addr);  // 文字列IPをバイナリに変換

    // sendtoでUDPパケットを送信
    int sent = sendto(sock, (const char*)data, len, 0, (sockaddr*)&dest, sizeof(dest));
    return sent == len;  // 送信バイト数が一致すれば成功
}

//============================================================
// send_broadcast - LAN内全体にブロードキャスト送信する
// ホスト探索のDISCOVERパケットで使用
//============================================================
bool UdpNetwork::send_broadcast(int port, const void* data, int len) {
    if (sock == INVALID_SOCKET) return false;
    // 255.255.255.255 = サブネット内の全ホストに届く特別なアドレス
    return send_to("255.255.255.255", port, data, len);
}

//============================================================
// poll_recv - select()を使って受信データがあるか確認し、あれば受信する
// timeout_ms: 待機ミリ秒（0=即座に戻る、負=無限待ち）
// 戻り値: 受信バイト数。0=データなし
//============================================================
int UdpNetwork::poll_recv(char* buffer, int bufferSize,
    std::string& from_ip, int& from_port,
    int timeout_ms) {
    if (sock == INVALID_SOCKET) return -1;

    // select()用のファイルディスクリプタセットを初期化
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    // タイムアウト値を設定
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    // select()でソケットにデータが入っているか確認
    int sel = select((int)sock + 1, &readfds, nullptr, nullptr,
        (timeout_ms >= 0) ? &tv : nullptr);
    if (sel <= 0) return 0;  // タイムアウトまたはエラー

    // データが入っているので受信する
    sockaddr_in from;
    int fromlen = sizeof(from);
    int ret = recvfrom(sock, buffer, bufferSize, 0, (sockaddr*)&from, &fromlen);
    if (ret <= 0) return 0;

    // 送信元のIPアドレスとポート番号を取り出す
    char ipbuf[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, &from.sin_addr, ipbuf, sizeof(ipbuf));  // バイナリIPを文字列に変換
    from_ip = ipbuf;
    from_port = ntohs(from.sin_port);

    return ret;  // 受信したバイト数を返す
}

//============================================================
// close_socket - ソケットを閉じてWinSockリソースを解放する
//============================================================
void UdpNetwork::close_socket() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    WSACleanup();  // WinSockの参照カウントを減らす
}

//============================================================
// is_port_available - 指定ポートが使用可能か確認する
// 実際にbindを試みて成功するかで判定する
//============================================================
bool UdpNetwork::is_port_available(int port) {
    // bindテスト用にWinSockを初期化
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // テスト用の一時ソケットを作成
    SOCKET testSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (testSocket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    // 指定ポートへのbindを試みる
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bool available = (bind(testSocket, (sockaddr*)&addr, sizeof(addr)) == 0);

    // テスト用ソケットを閉じてクリーンアップ
    closesocket(testSocket);
    WSACleanup();
    return available;
}

//============================================================
// get_random_available_port - ランダムに空きポートを1つ探す
// DYNAMIC_PORT_RANGES テーブルの範囲から試す
// 各範囲で最大100回ランダム試行する
//============================================================
int UdpNetwork::get_random_available_port() {
    for (int rangeIdx = 0; rangeIdx < NUM_DYNAMIC_RANGES; rangeIdx++) {
        int min_port = DYNAMIC_PORT_RANGES[rangeIdx][0];
        int max_port = DYNAMIC_PORT_RANGES[rangeIdx][1];

        // この範囲内でランダムなポート番号を生成する分布
        std::uniform_int_distribution<> dis(min_port, max_port);

        for (int attempts = 0; attempts < 100; attempts++) {
            int port = dis(gen);
            if (is_port_available(port)) {
                return port;  // 空きポートが見つかった
            }
        }
    }
    return -1;  // 全範囲で見つからなかった
}

//============================================================
// get_available_port_list - 複数の空きポートを探してリストで返す
//============================================================
std::vector<int> UdpNetwork::get_available_port_list(int count) {
    std::vector<int> ports;
    for (int i = 0; i < count; i++) {
        int port = get_random_available_port();
        if (port > 0) {
            ports.push_back(port);
        }
    }
    return ports;
}

//============================================================
// get_local_ip - このマシンのローカルIPアドレスを取得する
// gethostname() と getaddrinfo() で最初に見つかったIPv4アドレスを返す
//============================================================
std::string UdpNetwork::get_local_ip() {
    // まずホスト名を取得
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        return "127.0.0.1";  // 失敗したらlocalhostを返す
    }

    // ホスト名からIPアドレスを解決
    struct addrinfo hints = {}, * result;
    hints.ai_family = AF_INET;       // IPv4のみ
    hints.ai_socktype = SOCK_DGRAM;  // UDP

    if (getaddrinfo(hostname, NULL, &hints, &result) != 0) {
        return "127.0.0.1";
    }

    // バイナリアドレスを文字列に変換
    char ip_str[INET_ADDRSTRLEN];
    struct sockaddr_in* addr_in = (struct sockaddr_in*)result->ai_addr;
    inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, INET_ADDRSTRLEN);

    freeaddrinfo(result);  // 確保されたメモリを解放
    return std::string(ip_str);
}
