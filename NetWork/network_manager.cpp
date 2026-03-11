//==============================================================================
//  File   : network_manager.cpp
//  Brief  : NetworkManagerクラスの実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
// ホスト/クライアントの起動、パケット処理、
// フレーム同期、ワーカースレッド管理を行う
//==============================================================================
#include "pch.h"
#include "network_manager.h"
#include "Game/Objects/game_object.h"  // Game::GameObject
#include "Game/Objects/camera.h"       // Game::GetLocalPlayerGameObject
#include "Game/Objects/bullet.h"       // Game::Bullet
#include "Game/Objects/player.h"              // ← 追加
#include "Game/Managers/bullet_manager.h" // Game::BulletManager
#include "Game/Managers/player_manager.h"     // ← 追加
#include "main.h"
#include "Engine/Graphics/primitive.h" // Box頂点データ, GetPolygonTexture
#include <cstring>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <condition_variable>

//============================================================
// グローバル変数
//============================================================

// NetworkManagerのグローバルインスタンス（extern宣言はnetwork_manager.hにある）
NetworkManager g_network;

//============================================================
// コンストラクタ / デストラクタ
//============================================================

NetworkManager::NetworkManager() {
    // チャンネルスキャンの初期タイムスタンプを設定
    m_lastChannelScan = std::chrono::steady_clock::now();
}

NetworkManager::~NetworkManager() {
    // ワーカースレッドを停止してからソケットを閉じる
    stop_worker();
    m_net.close_socket();
    m_discovery.close_socket();
}

// 自分のプレイヤーIDを返す（クライアント側で使用）
uint32_t NetworkManager::getMyPlayerId() const {
    return m_myPlayerId;
}

//============================================================
// start_as_host - ホストとして起動する
// 1. ファイアウォール例外を登録（試みる）
// 2. ソケットをフォールバック付きで初期化
// 3. ホストのプレイヤーIDは1（クライアントには2以降を割り当て）
// 4. ワーカースレッドを開始
//============================================================
bool NetworkManager::start_as_host() {
    add_firewall_exception();
    if (!initialize_with_fallback()) {
        return false;
    }
    m_isHost = true;
    // ホスト自身はID=1。クライアントにはID=2から割り当てる
    m_nextPlayerId = 2;
    // 受信用ワーカースレッドを開始
    start_worker();
    return true;
}

//============================================================
// start_as_client - クライアントとして起動する
// 1. 動的ポートでソケットを初期化（固定ポートが使えない環境対応）
// 2. 探索用ソケットはDISCOVERY_PORT+1で初期化
// 3. ワーカースレッドを開始
//============================================================
bool NetworkManager::start_as_client() {
    add_firewall_exception();
    // まず動的ポートを試し、ダメならポート0（OS任せ）で初期化
    if (!m_net.initialize_dynamic_port()) {
        if (!m_net.initialize(0)) {
            return false;
        }
    }
    // 探索用ソケット（ホストの探索応答を受信する）
    if (!m_discovery.initialize_broadcast(DISCOVERY_PORT + 1)) {
        return false;
    }
    m_isHost = false;
    start_worker();
    return true;
}

//============================================================
// discover_and_join - ホストを探して参加する（ブロッキング処理）
// 全チャンネルに順番にDISCOVERをブロードキャストし、
// 応答があったらJOINパケットを送信する
//============================================================
bool NetworkManager::discover_and_join(std::string& out_host_ip) {
    // まずチャンネル使用状況をスキャン（30秒間隔制限あり）
    scan_channel_usage();

    // 各チャンネルのポートに対してDISCOVERを送信
    for (int channelIdx = 0; channelIdx < NUM_CHANNELS; channelIdx++) {
        int discoveryPort = PORT_RANGES[channelIdx][1];  // そのチャンネルの探索ポート

        // DISCOVERパケットをブロードキャスト送信
        uint8_t discover_pkt = PKT_DISCOVER;
        m_discovery.send_broadcast(discoveryPort, &discover_pkt, 1);

        char buf[MAX_UDP_PACKET];
        std::string from_ip;
        int from_port;

        // 1秒間、200msごとに応答を待つ
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count() < 1000) {
            int r = m_discovery.poll_recv(buf, sizeof(buf), from_ip, from_port, 200);
            if (r > 0) {
                uint8_t t = (uint8_t)buf[0];
                if (t == PKT_DISCOVER_REPLY) {
                    // ホストが見つかった
                    out_host_ip = from_ip;
                    m_hostIp = out_host_ip;
                    m_hostPort = PORT_RANGES[channelIdx][0];  // ゲーム通信ポート
                    m_currentChannel = channelIdx;

                    // JOINパケットをホストのゲーム通信ポートに送信
                    uint8_t join_pkt = PKT_JOIN;
                    m_net.send_to(out_host_ip, m_hostPort, &join_pkt, 1);
                    return true;
                }
            }
        }
    }
    // 全チャンネルで見つからなかった
    return false;
}

//============================================================
// update - メインスレッドから毎フレーム呼ばれる
// ワーカースレッドがキューに積んだパケットを最大N個取り出して処理する
// 1フレームに処理しすぎるとゲームが止まるので上限を設ける
//============================================================
void NetworkManager::update(float dt, Game::GameObject* localPlayer,
    std::vector<std::shared_ptr<Game::GameObject>>& worldObjects) {
    size_t processed = 0;
    while (processed < m_maxPacketsPerFrame) {
        RecvPacket pkt;
        {
            // キューからパケットを1つ取り出す（ロック範囲を最小にする）
            std::lock_guard<std::mutex> lk(m_recvMutex);
            if (m_recvQueue.empty()) break;
            pkt = std::move(m_recvQueue.front());
            m_recvQueue.pop_front();
        }
        // パケットの種別に応じて処理する
        process_received(pkt.data.data(), pkt.len, pkt.from_ip, pkt.from_port,
            localPlayer, worldObjects);
        ++processed;
    }
}

//============================================================
// process_received - 受信パケットを種別ごとに処理する
// ホストとクライアントで処理が異なる
//============================================================
void NetworkManager::process_received(const char* buf, int len,
    const std::string& from_ip, int from_port,
    Game::GameObject* localPlayer,
    std::vector<std::shared_ptr<Game::GameObject>>& worldObjects) {
    if (len <= 0) return;

    // パケットの先頭1バイトで種別を判定
    uint8_t t = (uint8_t)buf[0];

    if (m_isHost) {
        //============ ホスト側の処理 ============

        if (t == PKT_JOIN) {
            // クライアントからの参加リクエスト
            host_handle_join(from_ip, from_port, worldObjects);

        } else if (t == PKT_INPUT) {
            // クライアントからの入力データ
            if (len >= (int)sizeof(PacketInput)) {
                PacketInput pi;
                memcpy(&pi, buf, sizeof(PacketInput));
                host_handle_input(pi, worldObjects);

                // そのクライアントの最終通信時刻を更新
                std::lock_guard<std::mutex> lk(m_mutex);
                for (auto& client : m_clients) {
                    if (client.playerId == pi.playerId) {
                        client.lastSeen = std::chrono::steady_clock::now();
                        break;
                    }
                }
            }

        } else if (t == PKT_PING) {
            // 生存確認パケット（現状は何もしない）

        } else if (t == PKT_STATE) {
            // クライアントが自分の状態を送ってきた
            if (len >= (int)sizeof(PacketStateHeader)) {
                PacketStateHeader hdr;
                memcpy(&hdr, buf, sizeof(hdr));
                const char* p = buf + sizeof(hdr);
                size_t expected = sizeof(hdr) + (size_t)hdr.objectCount * sizeof(ObjectState);

                if (len >= (int)expected) {
                    const ObjectState* entries = (const ObjectState*)p;

                    for (uint32_t i = 0; i < hdr.objectCount; ++i) {
                        const ObjectState& os = entries[i];

                        bool applied = false;
                        for (const auto& go : worldObjects) {
                            if (go->getId() == os.id) {
                                // isAliveフラグも渡す
                                go->setNetworkTarget(
                                    { os.posX, os.posY, os.posZ },
                                    { os.rotX, os.rotY, os.rotZ },
                                    os.isAlive != 0);
                                applied = true;
                                break;
                            }
                        }

                        // クライアントの最終通信時刻を更新
                        std::lock_guard<std::mutex> lk(m_mutex);
                        for (auto& client : m_clients) {
                            if (client.playerId == os.id) {
                                client.lastSeen = std::chrono::steady_clock::now();
                                break;
                            }
                        }
                    }
                }
            }
        } else if (t == PKT_BULLET) {
            // クライアントからの弾発射通知 → ローカルで弾を生成 + 他クライアントに転送
            if (len >= (int)sizeof(PacketBullet)) {
                PacketBullet pb;
                memcpy(&pb, buf, sizeof(PacketBullet));

                // ホスト側で弾を生成
                auto b = std::make_unique<Game::Bullet>();
                b->Initialize(GetPolygonTexture(),
                    { pb.posX, pb.posY, pb.posZ },
                    { pb.dirX, pb.dirY, pb.dirZ },
                    (int)pb.ownerPlayerId);
                Game::BulletManager::GetInstance().Add(std::move(b));

                // 他の全クライアントに転送（送信元以外）
                std::lock_guard<std::mutex> lk(m_mutex);
                for (const auto& c : m_clients) {
                    if (c.ip == from_ip && c.port == from_port) continue;
                    m_net.send_to(c.ip, c.port, &pb, (int)sizeof(PacketBullet));
                }
            }
        }

    } else {
        //============ クライアント側の処理 ============

        if (t == PKT_JOIN_ACK) {
            // ホストから参加承認を受信（自分のプレイヤーIDが入っている）
            if (len >= 1 + 4) {
                uint32_t pid_net;
                memcpy(&pid_net, buf + 1, 4);
                m_myPlayerId = ntohl(pid_net);  // ネットワークバイト順→ホストバイト順に変換
            }

        } else if (t == PKT_STATE) {
            // ホストからゲーム状態を受信
            if (len >= (int)sizeof(PacketStateHeader)) {
                PacketStateHeader hdr;
                memcpy(&hdr, buf, sizeof(hdr));
                const char* p = buf + sizeof(hdr);
                size_t expected = sizeof(hdr) + (size_t)hdr.objectCount * sizeof(ObjectState);
                if (len >= (int)expected) {
                    client_handle_state(hdr, (const ObjectState*)p, localPlayer, worldObjects);
                }
            }

        } else if (t == PKT_BULLET) {
            // ホストから弾の発射通知を受信 → ローカルで弾を生成
            if (len >= (int)sizeof(PacketBullet)) {
                PacketBullet pb;
                memcpy(&pb, buf, sizeof(PacketBullet));

                // 自分が撃った弾は既にローカルで生成済みなのでスキップ
                if (pb.ownerPlayerId != m_myPlayerId) {
                    auto b = std::make_unique<Game::Bullet>();
                    b->Initialize(GetPolygonTexture(),
                        { pb.posX, pb.posY, pb.posZ },
                        { pb.dirX, pb.dirY, pb.dirZ },
                        (int)pb.ownerPlayerId);
                    Game::BulletManager::GetInstance().Add(std::move(b));
                }
            }
        }
    }
}

//============================================================
// host_handle_join - ホスト: 新しいクライアントの参加処理
// 1. 重複チェック（同じIP:Portなら無視）
// 2. 空いているプレイヤーIDを割り当て
// 3. クライアント情報をリストに追加
// 4. GameObjectを作成（既存なら再利用）
// 5. JOIN_ACKを返送
//============================================================
void NetworkManager::host_handle_join(const std::string& from_ip, int from_port,
    std::vector<std::shared_ptr<Game::GameObject>>& worldObjects) {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            for (auto& c : m_clients) {
                if (c.ip == from_ip && c.port == from_port) return;
            }
        }

        // クライアントにはID=2を割り当て（Player2に憑依）
        uint32_t assignedId = 2;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            ClientInfo ci;
            ci.ip = from_ip;
            ci.port = from_port;
            ci.playerId = assignedId;
            ci.lastSeen = std::chrono::steady_clock::now();
            m_clients.push_back(ci);
        }

        // ★ GameObjectの新規生成を削除（Player2は既にPlayerManagerが持っている）

        // JOIN_ACK返送
        uint8_t reply[1 + 4];
        reply[0] = PKT_JOIN_ACK;
        uint32_t pid_net = htonl(assignedId);
        memcpy(reply + 1, &pid_net, 4);
        m_net.send_to(from_ip, from_port, reply, (int)(1 + 4));
}

//============================================================
// host_handle_input - ホスト: クライアントの入力を適用する
// 対応するGameObjectの位置に移動量を加算する
//============================================================
void NetworkManager::host_handle_input(const PacketInput& pi,
    std::vector<std::shared_ptr<Game::GameObject>>& worldObjects) {
    for (const auto& go : worldObjects) {
        if (go->getId() == pi.playerId) {
            auto pos = go->getPosition();
            pos.x += pi.moveX;
            pos.y += pi.moveY;
            pos.z += pi.moveZ;
            go->setPosition(pos);
            return;
        }
    }
}

//============================================================
// client_handle_state - クライアント: ホストから受信した状態を適用
// 自分自身のIDはスキップ（ローカルの操作を優先するため）
// 既存オブジェクトがあれば補間ターゲットを設定、なければ新規作成
//============================================================
void NetworkManager::client_handle_state(const PacketStateHeader& hdr,
    const ObjectState* entries,
    Game::GameObject* localPlayer,
    std::vector<std::shared_ptr<Game::GameObject>>& worldObjects) {

    for (uint32_t i = 0; i < hdr.objectCount; ++i) {
        const ObjectState& os = entries[i];

        // 自分自身のプレイヤーIDならスキップ
        if (os.id == m_myPlayerId) {
            continue;
        }

        // 既存のGameObjectを探して補間ターゲットを設定
        bool applied = false;
        for (const auto& go : worldObjects) {
            if (go->getId() == os.id) {
                // isAliveフラグも渡す
                go->setNetworkTarget(
                    { os.posX, os.posY, os.posZ },
                    { os.rotX, os.rotY, os.rotZ },
                    os.isAlive != 0);
                applied = true;
                break;
            }
        }

        // 見つからなければ新しいGameObjectを生成
        if (!applied) {
        }
    }
}

//============================================================
// send_input - クライアント: ホストに入力データを送信する
//============================================================
void NetworkManager::send_input(const PacketInput& input) {
    // ホスト自身は送信不要
    if (m_isHost) return;
    // ホストIPが設定されていなければ送信しない
    if (m_hostIp.empty()) return;
    m_net.send_to(m_hostIp, m_hostPort, &input, (int)sizeof(PacketInput));
}

//============================================================
// send_bullet - 弾の発射情報を送信する
// ホスト: 全クライアントへ送信
// クライアント: ホストへ送信
//============================================================
void NetworkManager::send_bullet(const PacketBullet& pb) {
    if (m_isHost) {
        // ホスト: 全クライアントに弾情報を送信
        std::lock_guard<std::mutex> lk(m_mutex);
        for (const auto& c : m_clients) {
            m_net.send_to(c.ip, c.port, &pb, (int)sizeof(PacketBullet));
        }
    } else {
        // クライアント: ホストに弾情報を送信
        if (!m_hostIp.empty()) {
            m_net.send_to(m_hostIp, m_hostPort, &pb, (int)sizeof(PacketBullet));
        }
    }
}

//============================================================
// send_state_to_clients_round_robin
// ホスト: 1回の呼び出しにつき1クライアントだけに状態を送信する
// 全クライアントに毎回送ると負荷が高いため、順番に1人ずつ送る
//============================================================
void NetworkManager::send_state_to_clients_round_robin(
    std::vector<std::shared_ptr<Game::GameObject>>* worldObjectsPtr) {

    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_clients.empty() || worldObjectsPtr == nullptr) return;

    // 今回送信するクライアントをインデックスで選ぶ（ラウンドロビン）
    size_t idx = m_stateSendIndex % m_clients.size();
    ClientInfo& c = m_clients[idx];

    // パケットヘッダーを作成（オブジェクト1個分）
    PacketStateHeader header;
    header.type = PKT_STATE;
    header.seq = m_seq++;
    header.objectCount = 1;

    // 送信バッファを確保
    std::vector<char> sendbuf;
    sendbuf.resize(sizeof(header) + sizeof(ObjectState));
    memcpy(sendbuf.data(), &header, sizeof(header));

    // 該当クライアントのGameObjectから位置・回転を取得
    ObjectState os = {};
    os.id = c.playerId;
    bool found = false;
    for (const auto& go : *worldObjectsPtr) {
        if (go && go->getId() == os.id) {
            auto p = go->getPosition();
            os.posX = p.x; os.posY = p.y; os.posZ = p.z;
            auto rrot = go->getRotation();
            os.rotX = rrot.x; os.rotY = rrot.y; os.rotZ = rrot.z;
            found = true;
            break;
        }
    }
    // 見つからなければゼロ位置を送信
    if (!found) {
        os.posX = os.posY = os.posZ = 0.0f;
        os.rotX = os.rotY = os.rotZ = 0.0f;
    }
    memcpy(sendbuf.data() + sizeof(header), &os, sizeof(ObjectState));

    // 送信
    m_net.send_to(c.ip, c.port, sendbuf.data(), static_cast<int>(sendbuf.size()));

    // 次回は次のクライアントに送信する
    m_stateSendIndex = (m_stateSendIndex + 1) % (m_clients.empty() ? 1 : m_clients.size());
}

//============================================================
// FrameSync - フレーム同期
// メインループからNフレームごとに呼ばれる（例: 10フレームごと=6Hz）
// プレイヤーID 1と2の位置・回転を送受信する
//============================================================
void NetworkManager::FrameSync(Game::GameObject* localPlayer,
    std::vector<std::shared_ptr<Game::GameObject>>& worldObjects) {
    if (!localPlayer) {
        return;
    }

    // PlayerManagerへの参照を取得
    auto& playerMgr = Game::PlayerManager::GetInstance();

    // ラムダ: 指定IDリストのObjectStateを構築する
    auto build_states_for_ids = [&](const std::vector<int>& ids) {
        std::vector<ObjectState> states;
        for (int id : ids) {
            ObjectState os = {};
            os.id = static_cast<uint32_t>(id);
            bool found = false;

            // PlayerManagerからプレイヤー情報を取得
            Game::Player* player = playerMgr.GetPlayer(id);
            if (player) {
                auto p = player->GetPosition();
                auto r = player->GetRotation();
                os.posX = p.x; os.posY = p.y; os.posZ = p.z;
                os.rotX = r.x; os.rotY = r.y; os.rotZ = r.z;
                os.isAlive = player->IsAlive() ? 1 : 0;  // 生存フラグを設定
                found = true;
            }

            // Playerが見つからない場合はGameObjectから取得
            if (!found) {
                if (localPlayer && localPlayer->getId() == (uint32_t)id) {
                    auto p = localPlayer->getPosition();
                    auto r = localPlayer->getRotation();
                    os.posX = p.x; os.posY = p.y; os.posZ = p.z;
                    os.rotX = r.x; os.rotY = r.y; os.rotZ = r.z;
                    os.isAlive = 1;  // localPlayerは生存と仮定
                    found = true;
                } else {
                    for (const auto& go : worldObjects) {
                        if (!go) continue;
                        if (go->getId() == (uint32_t)id) {
                            auto p = go->getPosition();
                            auto r = go->getRotation();
                            os.posX = p.x; os.posY = p.y; os.posZ = p.z;
                            os.rotX = r.x; os.rotY = r.y; os.rotZ = r.z;
                            os.isAlive = 1;  // デフォルトは生存
                            found = true;
                            break;
                        }
                    }
                }
            }

            if (!found) {
                os.posX = os.posY = os.posZ = 0.0f;
                os.rotX = os.rotY = os.rotZ = 0.0f;
                os.isAlive = 0;
            }
            states.push_back(os);
        }
        return states;
        };

    // 同期対象のプレイヤーID
    std::vector<int> targetIds = { 1, 2 };

    if (m_isHost) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_clients.empty()) {
            return;
        }

        auto states = build_states_for_ids(targetIds);

        PacketStateHeader header;
        header.type = PKT_STATE;
        header.seq = m_seq++;
        header.objectCount = (uint32_t)states.size();

        std::vector<char> buf(sizeof(header) + states.size() * sizeof(ObjectState));
        memcpy(buf.data(), &header, sizeof(header));
        memcpy(buf.data() + sizeof(header), states.data(), states.size() * sizeof(ObjectState));

        for (const auto& c : m_clients) {
            m_net.send_to(c.ip, c.port, buf.data(), (int)buf.size());
        }

    } else {
        if (m_hostIp.empty()) {
            return;
        }
        if (m_myPlayerId == 0) {
            return;
        }

        auto states = build_states_for_ids(targetIds);

        PacketStateHeader header;
        header.type = PKT_STATE;
        header.seq = m_seq++;
        header.objectCount = (uint32_t)states.size();

        std::vector<char> buf(sizeof(header) + states.size() * sizeof(ObjectState));
        memcpy(buf.data(), &header, sizeof(header));
        memcpy(buf.data() + sizeof(header), states.data(), states.size() * sizeof(ObjectState));

        m_net.send_to(m_hostIp, m_hostPort, buf.data(), (int)buf.size());
    }
}

//============================================================
// scan_channel_usage - チャンネル使用状況をスキャンする
// 負荷軽減のため30秒間隔でしか実行しない
//============================================================
void NetworkManager::scan_channel_usage() {
    auto now = std::chrono::steady_clock::now();
    // 前回スキャンから30秒経っていなければスキップ
    if (std::chrono::duration_cast<std::chrono::seconds>(now - m_lastChannelScan).count() < 30) {
        return;
    }
    m_lastChannelScan = now;
}

//============================================================
// find_least_crowded_channel - 最もユーザーが少ないチャンネルを返す
//============================================================
int NetworkManager::find_least_crowded_channel() {
    if (m_channelInfo.empty()) return 0;  // 情報がなければチャンネル0
    int bestChannel = 0;
    uint32_t minUsers = UINT32_MAX;
    for (const auto& info : m_channelInfo) {
        if (info.userCount < minUsers) {
            minUsers = info.userCount;
            bestChannel = static_cast<int>(info.channelId);
        }
    }
    return bestChannel;
}

//============================================================
// switch_to_channel - 指定チャンネルに切り替える
// 現在のソケットを閉じて新しいポートで再初期化する
//============================================================
bool NetworkManager::switch_to_channel(int channelId) {
    if (channelId < 0 || channelId >= NUM_CHANNELS) return false;

    // 現在のソケットを閉じる
    m_net.close_socket();
    m_discovery.close_socket();

    // 新しいチャンネルのポートで初期化
    int newNetPort = PORT_RANGES[channelId][0];
    int newDiscoveryPort = PORT_RANGES[channelId][1];
    if (!m_net.initialize(newNetPort) || !m_discovery.initialize_broadcast(newDiscoveryPort)) {
        return false;
    }
    m_currentChannel = channelId;
    return true;
}

//============================================================
// try_dynamic_ports - 両ソケットをランダムポートで初期化する
//============================================================
bool NetworkManager::try_dynamic_ports() {
    return m_net.initialize_dynamic_port() && m_discovery.initialize_dynamic_port();
}

//============================================================
// check_existing_firewall_rule - ファイアウォールルールの存在確認
// COM列挙はコストが高いため、常にfalseを返して簡略化している
//============================================================
bool NetworkManager::check_existing_firewall_rule() {
    return false;
}

//============================================================
// add_firewall_exception - ファイアウォール例外の登録
// 管理者権限があるか確認するが、実際のルール追加は行わない
// （学校環境などでは権限がないことが多いため）
//============================================================
bool NetworkManager::add_firewall_exception() {
    // 管理者権限チェック
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        CheckTokenMembership(NULL, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
    }
    if (!isAdmin) {
        return true;
    }
    // 管理者であっても実際のルール追加はスキップ（パフォーマンス優先）
    return true;
}

//============================================================
// handle_channel_scan - チャンネルスキャン要求への応答
// 自分のチャンネル情報を要求元に返す
//============================================================
void NetworkManager::handle_channel_scan(const std::string& from_ip, int from_port) {
    ChannelInfo info;
    info.type = PKT_CHANNEL_INFO;
    info.channelId = static_cast<uint32_t>(m_currentChannel);
    info.userCount = static_cast<uint32_t>(m_clients.size());
    info.basePort = static_cast<uint32_t>(m_net.get_current_port());
    info.discoveryPort = static_cast<uint32_t>(m_discovery.get_current_port());
    m_discovery.send_to(from_ip, from_port, &info, sizeof(info));
}

//============================================================
// handle_channel_info - 受信したチャンネル情報をリストに保存する
// 同じチャンネルIDが既にあれば上書き、なければ追加
//============================================================
void NetworkManager::handle_channel_info(const ChannelInfo& info) {
    bool found = false;
    for (auto& existing : m_channelInfo) {
        if (existing.channelId == info.channelId) {
            existing = info;  // 既存エントリを更新
            found = true;
            break;
        }
    }
    if (!found) m_channelInfo.push_back(info);  // 新規追加
}

//============================================================
// start_worker - 受信ポーリング用のワーカースレッドを開始する
//
// このスレッドは以下を繰り返す:
// 1. ゲーム通信ソケットをポーリング → パケットをキューに追加
// 2. 探索ソケットをポーリング → DISCOVERには即座に応答、それ以外はキューへ
// 3. ホストの場合、定期的にラウンドロビンで最小限の状態を送信
// 4. 5ms待機してCPU負荷を抑える
//============================================================
void NetworkManager::start_worker() {
    // 既に動作中なら何もしない（exchange=trueを返す場合は既にtrue）
    if (m_workerRunning.exchange(true)) return;

    m_worker = std::thread([this]() {
        // ワーカー内のローカル変数
        std::vector<std::shared_ptr<Game::GameObject>> worldObjectsSnapshot;
        auto lastStateSend = std::chrono::steady_clock::now();

        while (m_workerRunning.load()) {

            // --- 1. ゲーム通信ソケットのポーリング ---
            char buf[MAX_UDP_PACKET];
            std::string from_ip;
            int from_port = 0;

            // 50msタイムアウトで受信を試みる
            int r = m_net.poll_recv(buf, sizeof(buf), from_ip, from_port, 50);
            if (r > 0) {
                // 受信データをキューに積む（メインスレッドで処理する）
                RecvPacket pkt;
                pkt.data.assign(buf, buf + r);
                pkt.len = r;
                pkt.from_ip = from_ip;
                pkt.from_port = from_port;
                pkt.isDiscovery = false;
                push_recv_packet(std::move(pkt));
            }

            // --- 2. 探索ソケットのポーリング ---
            int dr = m_discovery.poll_recv(buf, sizeof(buf), from_ip, from_port, 10);
            if (dr > 0) {
                uint8_t dt = (uint8_t)buf[0];
                if (m_isHost && dt == PKT_DISCOVER) {
                    // ホストの場合、DISCOVER要求には即座に応答する（軽量処理）
                    uint8_t reply = PKT_DISCOVER_REPLY;
                    m_discovery.send_to(from_ip, from_port, &reply, 1);
                } else {
                    // それ以外のパケット（チャンネル情報など）はキューへ
                    RecvPacket pkt;
                    pkt.data.assign(buf, buf + dr);
                    pkt.len = dr;
                    pkt.from_ip = from_ip;
                    pkt.from_port = from_port;
                    pkt.isDiscovery = true;
                    push_recv_packet(std::move(pkt));
                }
            }

            // --- 3. ホスト: 定期的な最小限状態送信 ---
            // m_stateInterval（200ms）ごとに1クライアントに最小パケットを送る
            // ネットワーク接続を維持するためのキープアライブ的な役割
            auto now = std::chrono::steady_clock::now();
            if (!m_isHost) {
                // クライアントは何もしない
            } else if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastStateSend) >= m_stateInterval) {
                // ワーカースレッドからworldObjectsに安全にアクセスできないため、
                // 位置はゼロで埋めた最小パケットを送る（正確な位置はFrameSyncで送る）
                std::lock_guard<std::mutex> lk(m_mutex);
                if (!m_clients.empty()) {
                    size_t idx = m_stateSendIndex % m_clients.size();
                    ClientInfo& c = m_clients[idx];

                    // 最小限の状態パケットを組み立て
                    PacketStateHeader header;
                    header.type = PKT_STATE;
                    header.seq = m_seq++;
                    header.objectCount = 1;
                    ObjectState os = {};
                    os.id = c.playerId;
                    os.posX = os.posY = os.posZ = 0.0f;
                    os.rotX = os.rotY = os.rotZ = 0.0f;

                    std::vector<char> sendbuf;
                    sendbuf.resize(sizeof(header) + sizeof(os));
                    memcpy(sendbuf.data(), &header, sizeof(header));
                    memcpy(sendbuf.data() + sizeof(header), &os, sizeof(os));
                    m_net.send_to(c.ip, c.port, sendbuf.data(), static_cast<int>(sendbuf.size()));

                    // 次のクライアントに進む
                    m_stateSendIndex = (m_stateSendIndex + 1) %
                        (m_clients.empty() ? 1 : m_clients.size());
                }
                lastStateSend = now;
            }

            // --- 4. CPU負荷軽減のためスリープ ---
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        // ワーカースレッド終了
        });
}

//============================================================
// stop_worker - ワーカースレッドを停止してjoinする
//============================================================
void NetworkManager::stop_worker() {
    // フラグをfalseに設定（既にfalseなら何もしない）
    if (!m_workerRunning.exchange(false)) return;
    // スレッドの終了を待つ
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

//============================================================
// push_recv_packet - ワーカースレッドからキューにパケットを追加する
// キューが最大サイズ（1024）を超えたら古いパケットを捨てる
//============================================================
void NetworkManager::push_recv_packet(RecvPacket&& pkt) {
    std::lock_guard<std::mutex> lk(m_recvMutex);
    const size_t MAX_QUEUE = 1024;
    if (m_recvQueue.size() >= MAX_QUEUE) {
        // キューが満杯 → 最も古いパケットを削除
        m_recvQueue.pop_front();
    }
    m_recvQueue.push_back(std::move(pkt));
}

//============================================================
// initialize_with_fallback - フォールバック付きソケット初期化
// 1. デフォルトポート（チャンネル0）を試す
// 2. ダメなら他のチャンネルを順に試す
// 3. 全部ダメならランダム動的ポートを試す
//============================================================
bool NetworkManager::initialize_with_fallback() {
    // まずデフォルトポートで試す
    if (m_net.initialize(NET_PORT) && m_discovery.initialize_broadcast(DISCOVERY_PORT)) {
        return true;
    }
    // 代替チャンネルを順に試す
    for (int i = 1; i < NUM_CHANNELS; i++) {
        int netPort = PORT_RANGES[i][0];
        int discoveryPort = PORT_RANGES[i][1];
        if (m_net.initialize(netPort) && m_discovery.initialize_broadcast(discoveryPort)) {
            m_currentChannel = i;
            return true;
        }
    }
    // 最後の手段: ランダムポート
    if (try_dynamic_ports()) return true;
    return false;
}

//============================================================
// send_state_to_all - ホスト: 全クライアントに全オブジェクトの状態を送信
// ホスト自身のプレイヤー（ID=1）も含めて送信する
//============================================================
void NetworkManager::send_state_to_all(
    std::vector<std::shared_ptr<Game::GameObject>>& worldObjects) {

    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_clients.empty()) {
        return;
    }

    // 送信する状態リストを構築
    std::vector<ObjectState> states;

    // --- ホストのローカルプレイヤー（ID=1）を追加 ---
    Game::GameObject* localPlayer = nullptr;
    localPlayer = Game::GetLocalPlayerGameObject();

    if (localPlayer && localPlayer->getId() == 1) {
        ObjectState os = {};
        os.id = 1;
        auto p = localPlayer->getPosition();
        auto r = localPlayer->getRotation();
        os.posX = p.x; os.posY = p.y; os.posZ = p.z;
        os.rotX = r.x; os.rotY = r.y; os.rotZ = r.z;
        states.push_back(os);
    }

    // --- 各クライアントのGameObjectを追加 ---
    for (const auto& c : m_clients) {
        ObjectState os = {};
        os.id = c.playerId;
        bool found = false;
        for (const auto& go : worldObjects) {
            if (go && go->getId() == os.id) {
                auto p = go->getPosition();
                auto r = go->getRotation();
                os.posX = p.x; os.posY = p.y; os.posZ = p.z;
                os.rotX = r.x; os.rotY = r.y; os.rotZ = r.z;
                found = true;
                break;
            }
        }
        // worldObjectsに見つからなければゼロ位置で送信
        if (!found) {
            os.posX = os.posY = os.posZ = 0.0f;
            os.rotX = os.rotY = os.rotZ = 0.0f;
        }
        states.push_back(os);
    }

    if (states.empty()) {
        return;
    }

    // --- パケットを組み立てて全クライアントに送信 ---
    PacketStateHeader header;
    header.type = PKT_STATE;
    header.seq = m_seq++;
    header.objectCount = (uint32_t)states.size();

    std::vector<char> buf(sizeof(header) + states.size() * sizeof(ObjectState));
    memcpy(buf.data(), &header, sizeof(header));
    memcpy(buf.data() + sizeof(header), states.data(), states.size() * sizeof(ObjectState));

    for (const auto& c : m_clients) {
        m_net.send_to(c.ip, c.port, buf.data(), (int)buf.size());
    }
}
