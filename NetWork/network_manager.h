//==============================================================================
//  File   : network_manager.h
//  Brief  : ネットワークマネージャー
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/11
//------------------------------------------------------------------------------
// ホスト/クライアント両対応の軽量ネットワークマネージャー
// ワーカースレッドで受信し、メインスレッドで処理するモデル
//==============================================================================
/*********************************************************************
 * \file   network_manager.h
 * \brief  
 *         ワーカースレッドで受信し、メインスレッドで処理するモデル
 *
 * \author Ryoto Kikuchi
 * \date   2026/2/24
 *********************************************************************/
#pragma once

#include "udp_network.h"       // UDPソケットラッパー
#include "network_common.h"    // パケット構造体・ポート定数
#include <vector>
#include <unordered_map>
#include <memory>              // std::shared_ptr
#include <mutex>               // std::mutex, std::lock_guard
#include <chrono>              // 時刻計測
#include <thread>              // std::thread（ワーカースレッド）
#include <condition_variable>
#include <deque>               // 受信キュー
#include <atomic>              // std::atomic（スレッド間フラグ）

// GameObjectの前方宣言（ヘッダーの相互依存を避ける）
namespace Game { class GameObject; }

//============================================================
// NetworkManager クラス
//
// 役割:
//   - ホストモード: クライアントの参加管理、状態の配信
//   - クライアントモード: ホスト探索、参加、入力送信、状態受信
//   - 共通: ワーカースレッドでソケットをポーリングし、
//           受信パケットをキューに積んでメインスレッドで処理
//============================================================
class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    //----------------------------------------------------------
    // 起動・接続
    //----------------------------------------------------------

    // ホストとして起動する（ソケット初期化→ワーカースレッド開始）
    bool start_as_host();

    // クライアントとして起動する（動的ポートで初期化→ワーカースレッド開始）
    bool start_as_client();

    // クライアント用: ブロードキャストでホストを探してJOINを送る（ブロッキング）
    // 成功時、out_host_ipにホストのIPアドレスが入る
    bool discover_and_join(std::string& out_host_ip);

    //----------------------------------------------------------
    // 毎フレーム処理
    //----------------------------------------------------------

    // メインスレッドから毎フレーム呼ぶ。キューに溜まったパケットを処理する
    // dt: デルタタイム（現状未使用だが将来の補間用に渡す）
    void update(float dt, Game::GameObject* localPlayer,
        std::vector<std::shared_ptr<Game::GameObject>>& worldObjects);

    // クライアントの入力をホストへ送信する
    void send_input(const PacketInput& input);

    // 現在ホストモードかどうかを返す
    bool is_host() const { return m_isHost; }

    // サーバーから割り当てられた自分のプレイヤーIDを取得する
    uint32_t getMyPlayerId() const;

    // フレーム同期: Nフレームごとに呼び出し、位置情報を送受信する
    // （例: 60FPSで10フレームごと = 6Hz）
    void FrameSync(Game::GameObject* localPlayer,
        std::vector<std::shared_ptr<Game::GameObject>>& worldObjects);

    //----------------------------------------------------------
    // チャンネル管理（ポートが塞がっている場合の代替手段）
    //----------------------------------------------------------

    // 各チャンネルの使用状況をスキャンする（30秒間隔で実行）
    void scan_channel_usage();

    // 最もユーザーが少ないチャンネル番号を返す
    int find_least_crowded_channel();

    // 指定チャンネルに切り替える（ソケットを再初期化する）
    bool switch_to_channel(int channelId);

    //----------------------------------------------------------
    // 動的ポート管理
    //----------------------------------------------------------

    // ランダムな空きポートでゲーム通信・探索の両ソケットを初期化する
    bool try_dynamic_ports();

    //----------------------------------------------------------
    // ファイアウォール例外登録
    //----------------------------------------------------------

    // 管理者権限があればファイアウォール例外を追加する（実際にはスキップ）
    bool add_firewall_exception();


    // 弾の発射情報を送信する（ホスト: 全クライアントへ、クライアント: ホストへ）
    void send_bullet(const PacketBullet& pb);

private:
    //----------------------------------------------------------
    // ソケット
    //----------------------------------------------------------
    UdpNetwork m_net;        // ゲーム通信用ソケット（NET_PORT）
    UdpNetwork m_discovery;  // ホスト探索用ソケット（DISCOVERY_PORT）
    bool m_isHost = false;   // trueならホスト、falseならクライアント

    //----------------------------------------------------------
    // スレッド安全用ミューテックス
    //----------------------------------------------------------
    std::mutex m_mutex;  // m_clients, m_seq などの保護用

    //----------------------------------------------------------
    // ホスト側のデータ
    //----------------------------------------------------------

    // 接続中のクライアント情報
    struct ClientInfo {
        std::string ip;       // クライアントのIPアドレス
        int port;             // クライアントのポート番号
        uint32_t playerId;    // 割り当てたプレイヤーID
        std::chrono::steady_clock::time_point lastSeen;  // 最終通信時刻
    };
    std::vector<ClientInfo> m_clients;   // 接続中クライアントのリスト
    uint32_t m_nextPlayerId = 1;         // 次に割り当てるプレイヤーID
    uint32_t m_seq = 0;                  // パケットのシーケンス番号（送信ごとにインクリメント）

    //----------------------------------------------------------
    // クライアント側のデータ
    //----------------------------------------------------------
    std::string m_hostIp;              // 接続先ホストのIPアドレス
    int m_hostPort = NET_PORT;         // 接続先ホストのポート番号
    uint32_t m_myPlayerId = 0;         // サーバーから割り当てられた自分のID（0=未参加）

    //----------------------------------------------------------
    // チャンネル管理
    //----------------------------------------------------------
    int m_currentChannel = 0;          // 現在使用中のチャンネル番号
    std::vector<ChannelInfo> m_channelInfo;  // スキャン結果のチャンネル情報一覧
    std::chrono::steady_clock::time_point m_lastChannelScan;  // 最後にスキャンした時刻

    //----------------------------------------------------------
    // 受信パケットキュー
    // ワーカースレッドが受信してキューに積み、
    // メインスレッドのupdate()で取り出して処理する
    //----------------------------------------------------------
    struct RecvPacket {
        std::vector<char> data;  // 受信データ本体
        int len;                 // データ長
        std::string from_ip;     // 送信元IPアドレス
        int from_port;           // 送信元ポート番号
        bool isDiscovery;        // 探索ソケットからの受信かどうか
    };
    std::deque<RecvPacket> m_recvQueue;  // 受信パケットのキュー（先入れ先出し）
    std::mutex m_recvMutex;              // キュー操作用ミューテックス
    std::condition_variable m_recvCv;    // キュー通知用（将来のブロッキング受信用）

    //----------------------------------------------------------
    // ワーカースレッド
    //----------------------------------------------------------
    std::thread m_worker;                // 受信ポーリング用のバックグラウンドスレッド
    std::atomic<bool> m_workerRunning{ false };  // スレッド動作フラグ

    //----------------------------------------------------------
    // パフォーマンス・調整パラメータ
    //----------------------------------------------------------
    size_t m_maxPacketsPerFrame = 8;    // 1フレームで処理する最大パケット数
    std::chrono::milliseconds m_stateInterval{ 200 };  // ホストがSTATEを送信する間隔
    size_t m_stateSendIndex = 0;        // ラウンドロビン送信のインデックス

    //----------------------------------------------------------
    // パケット処理（private関数）
    //----------------------------------------------------------

    // 受信パケットを種別に応じて振り分ける（メインスレッドで呼ばれる）
    void process_received(const char* buf, int len,
        const std::string& from_ip, int from_port,
        Game::GameObject* localPlayer,
        std::vector<std::shared_ptr<Game::GameObject>>& worldObjects);

    // ホスト: JOINパケットを受信した時の処理（ID割り当て・ACK送信）
    void host_handle_join(const std::string& from_ip, int from_port,
        std::vector<std::shared_ptr<Game::GameObject>>& worldObjects);

    // ホスト: INPUTパケットを受信した時の処理（プレイヤー移動を適用）
    void host_handle_input(const PacketInput& pi,
        std::vector<std::shared_ptr<Game::GameObject>>& worldObjects);

    // クライアント: STATEパケットを受信した時の処理（他プレイヤーの位置を更新）
    void client_handle_state(const PacketStateHeader& hdr, const ObjectState* entries,
        Game::GameObject* localPlayer,
        std::vector<std::shared_ptr<Game::GameObject>>& worldObjects);

    // ホスト: 全クライアントに全オブジェクトの状態を送信する
    void send_state_to_all(std::vector<std::shared_ptr<Game::GameObject>>& worldObjects);

    //----------------------------------------------------------
    // チャンネル関連（private関数）
    //----------------------------------------------------------

    // チャンネルスキャン要求に対して自分のチャンネル情報を返す
    void handle_channel_scan(const std::string& from_ip, int from_port);

    // 受信したチャンネル情報をリストに追加・更新する
    void handle_channel_info(const ChannelInfo& info);

    // ポートのフォールバック付き初期化（チャンネル0→1→…→動的ポート）
    bool initialize_with_fallback();

    //----------------------------------------------------------
    // ラウンドロビン送信（ワーカースレッドから使用）
    //----------------------------------------------------------

    // 1フレームにつき1クライアントだけに状態を送信して負荷を分散する
    void send_state_to_clients_round_robin(
        std::vector<std::shared_ptr<Game::GameObject>>* worldObjectsPtr);

    //----------------------------------------------------------
    // ワーカースレッド制御
    //----------------------------------------------------------

    // ワーカースレッドを開始する（既に動作中なら何もしない）
    void start_worker();

    // ワーカースレッドを停止してjoinする
    void stop_worker();

    // ワーカースレッドからキューにパケットを追加する（容量制限付き）
    void push_recv_packet(RecvPacket&& pkt);

    //----------------------------------------------------------
    // ファイアウォール補助
    //----------------------------------------------------------

    // 既存のファイアウォールルールがあるか確認する（現状は常にfalse）
    bool check_existing_firewall_rule();

    //----------------------------------------------------------
    // デバッグ設定
    //----------------------------------------------------------
    bool m_verboseLogs = false;  // trueにすると詳細ログを出力する
};

// グローバルインスタンス宣言（実体はnetwork_manager.cppで定義）
extern NetworkManager g_network;
