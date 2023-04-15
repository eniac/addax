#include "addax-lib.h"
#include "net.hpp"

vector<int> adv_fds;
vector<string> shares_vec;
volatile int recv_share_num = 0;
vector<int> new_adv_fds;

void recv_share(int fd) {
    string response;
    bool recv_flag = recv_response(fd, response);
    assert(recv_flag);
    char a = *(response.substr(0, 1).c_str());
    int idx = a;
    cout << fd << " recv id " << idx << endl;
    shares_vec[idx] = response.substr(1);
    new_adv_fds[idx] = fd;
}

void send_and_recv_share(int id, int parallel_num, string& msg, int winner_id) {
    for (int i = 0; i < adv_fds.size(); i++) {
        if (i % parallel_num != id || (winner_id != -1 && i == winner_id))
            continue;
        string response;
        int fd = adv_fds[i];
        assert(fd > 0);
        assert(sendMsg(fd, msg));
        bool recv_flag = recv_response(fd, response);
        assert(recv_flag);
        char a = *(response.substr(0, 1).c_str());
        int idx = a;
        cout << fd << " recv id " << idx << " size: " << response.size()
             << endl;
        if (winner_id != -1 && idx > winner_id) {
            shares_vec[idx - 1] = response.substr(1);
        } else {
            shares_vec[idx] = response.substr(1);
        }
        assert(idx == i);
    }
}

void ask_adv_bid(int winner_id, int bid) {
    int magic_num = 0xFFFFFFF;
    string query = string((char*)&magic_num, sizeof(int));
    query.append(string((char*)&bid, sizeof(int)));
    sendMsg(adv_fds[winner_id], query);
    string response;
    bool recv_flag = recv_response(adv_fds[winner_id], response);
    assert(recv_flag);
    assert(response.size() == sizeof(int));
    int resp = *((int*)response.c_str());
    assert(bid == resp);
}

void listen_for_client(int* client_fd, string ip_addr) {
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    int sock_opt;
    assert(setsockopt(listen_fd, SOL_SOCKET,
                      TCP_NODELAY | SO_REUSEADDR | SO_REUSEPORT, &sock_opt,
                      sizeof(sock_opt)) >= 0);
    struct sockaddr_in servaddr = string_to_struct_addr(ip_addr);
    assert(bind(listen_fd, (sockaddr*)&servaddr, sizeof(servaddr)) >= 0);
    assert(listen(listen_fd, 100) >= 0);
    cout << "start listening on " << ip_addr << endl;
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    int fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
    *client_fd = fd;
    cout << "conn with client\n";
}

int main(int argc, char* argv[]) {
    Crypto env = Crypto();
    BIGNUM* p = env.getP();
    int bit_len = env.bitLen();
    BN_CTX* ctx = env.getCtx();
    int order_bit_len = env.orderBitLen();

    int ad_num = 20;
    bool parallel = true;
    int parallel_num = 8;
    string ip_another_committee = "0.0.0.0:6667";
    string ip_addr = "0.0.0.0:6666";

    int round = 2;
    int base_bucket = 100;
    // sockets to receive from advertisers, assume we know number of advertisers
    // in advance
    int port = 6666;

    // whether to run first price baseline
    // false default (second price auction by default)
    bool first_price = false;

    int opt;
    while ((opt = getopt(argc, argv, "a:b:l:n:c:p:o:r:q")) != -1) {
        if (opt == 'a') {
            ad_num = atoi(optarg);
        } else if (opt == 'b') {
            BUCKET_NUM = atoi(optarg);
        } else if (opt == 'l') {
            LAMBDA = atoi(optarg);
        } else if (opt == 'p') {
            ip_another_committee = string(optarg);
        } else if (opt == 'n') {
            parallel_num = atoi(optarg);
        } else if (opt == 'o') {
            ip_addr = string(optarg);
        } else if (opt == 'r') {
            round = atoi(optarg);
        } else if (opt == 'q') {
            first_price = true;
        }
    }

    if (round == 2) {
        base_bucket = 100;
    } else if (round == 4) {
        base_bucket = 10;
    } else {
        assert(0);
    }

    DEBUG << "simulating " << ad_num << " advertisers\n";
    DEBUG << "simulating " << BUCKET_NUM << " buckets\n";
    DEBUG << "lambda: " << LAMBDA << endl;
    DEBUG << "parallel: " << parallel << endl;
    DEBUG << "parallel num: " << parallel_num << endl;
    DEBUG << "publisher ip: " << ip_another_committee << endl;
    DEBUG << "running as first price? " << first_price << endl;

    adv_fds = vector<int>(ad_num, 0);
    shares_vec = vector<string>(ad_num);
    int total_bucket = BUCKET_NUM;
    int interval = total_bucket / base_bucket;
    int start_bucket = 0;

    BUCKET_NUM = base_bucket;

    // potential can change it two thread of sending
    // send two shares

    Committee c = Committee(base_bucket, LAMBDA, parallel, parallel_num);

    system_clock::time_point starttime, endtime, start, end;
    system_clock::time_point start_total, end_total;

    vector<string> listen_ips;
    int curr_port = port;
    for (int i = 0; i < 3; i++) {
        listen_ips.push_back("0.0.0.0:" + to_string(curr_port));
        curr_port++;
    }

    int client_fd_write;
    int client_fd_read;
    vector<thread> t_vec;
    for (int i = 0; i < 2; i++) {
        if (i == 0) {
            thread t(&listen_for_client, &client_fd_write, listen_ips[i]);
            t_vec.push_back(move(t));
        } else if (i == 1) {
            thread t(&listen_for_client, &client_fd_read, listen_ips[i]);
            t_vec.push_back(move(t));
        }
    }

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    int sock_opt;
    assert(setsockopt(listen_fd, SOL_SOCKET,
                      TCP_NODELAY | SO_REUSEADDR | SO_REUSEPORT, &sock_opt,
                      sizeof(sock_opt)) >= 0);
    struct sockaddr_in servaddr = string_to_struct_addr(listen_ips[2]);
    assert(bind(listen_fd, (sockaddr*)&servaddr, sizeof(servaddr)) >= 0);
    assert(listen(listen_fd, 100) >= 0);
    cout << "start listening on " << listen_ips[2] << endl;

    // init new_adv_fds which records correct idx to fd
    new_adv_fds = vector<int>(ad_num, -1);

    int recv_adv_num = 0;
    while (recv_adv_num < ad_num) {
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
        starttime = system_clock::now();
        int comm_fd =
            accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
        // note: adv_fds does not have correct sequence of advertisers
        adv_fds[recv_adv_num] = comm_fd;
        thread t(&recv_share, comm_fd);
        t_vec.push_back(move(t));
        recv_adv_num++;
        cout << "connect with " << recv_adv_num << " adv\n";
    }

    cout << "start waiting for join\n";
    for (auto& th : t_vec) {
        th.join();
    }
    cout << "finish\n";

    // till now adv_fds should have correct sequence
    adv_fds = new_adv_fds;
    for (int i = 0; i < adv_fds.size(); i++) {
        cout << i << " fd is " << adv_fds[i] << endl;
    }

    vector<Committee> c_vec;
    vector<vector<BIGNUM*>> sum_vec;
    vector<int> decode_vec;
    vector<int> revealed_ids =
        generate_ids(1, total_bucket, base_bucket, start_bucket);

    c.initShares(ad_num);
    c.deserial_addShares_parallel_opt(shares_vec);

    vector<vector<BIGNUM*>> c_shares_opt = c.getShares_opt();
    vector<BIGNUM*> sum_s1 =
        sumBNVec_opt(c_shares_opt, env, parallel, parallel_num);
    string sum_s1_str = serializeShareVec_opt(sum_s1);

    // exchange hash
    string sum_s1_str_hash = sha256(sum_s1_str);
    thread t1_hash(&sendShare, client_fd_write, ref(sum_s1_str_hash));
    string sum_s2_str_hash;
    thread t2_hash(&recvShare, client_fd_read, ref(sum_s2_str_hash));
    t2_hash.join();
    t1_hash.join();

    // receive then send
    thread t1(&sendShare, client_fd_write, ref(sum_s1_str));
    string sum_s2_str;
    thread t2(&recvShare, client_fd_read, ref(sum_s2_str));
    t2.join();
    assert(sha256(sum_s2_str) == sum_s2_str_hash);
    vector<BIGNUM*> sum_s2 = c.bn_deserializeShare_opt(sum_s2_str);
    t1.join();

    endtime = system_clock::now();
    cout << "TIME: recv shares + deserialize: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    vector<vector<BIGNUM*>> share_input;
    share_input.emplace_back(sum_s1);
    share_input.emplace_back(sum_s2);
    vector<BIGNUM*> sum_vec_s = sumBNVec_opt(share_input, env);
    int decode_bid = decode_bit_vec_opt(sum_vec_s);
    start_bucket = decode_bid * interval + start_bucket;
    cout << "AND decode (using shares): " << decode_bid << endl;

    c_vec.emplace_back(c);
    sum_vec.emplace_back(sum_s1);
    decode_vec.emplace_back(start_bucket);
    shares_vec.clear();
    shares_vec = vector<string>(ad_num);

    for (int loop = 2; loop <= round; loop++) {
        interval = interval / base_bucket;
        vector<int> ids =
            generate_ids(loop, total_bucket, base_bucket, start_bucket);
        revealed_ids.insert(revealed_ids.end(), ids.begin(), ids.end());

        // send start buckt to all advertisers
        char start_str[8];
        memcpy(start_str, &start_bucket, 4);
        memcpy(start_str + 4, &loop, 4);
        string start_msg = string((const char*)start_str, 8);
        vector<thread> t_vec;
        for (int i = 0; i < ad_num; i++) {
            thread t(&send_and_recv_share, i, ad_num, ref(start_msg), -1);
            t_vec.push_back(move(t));
        }
        for (auto& th : t_vec) {
            th.join();
        }

        // repeat running
        Committee c1 = Committee(base_bucket, LAMBDA, parallel, parallel_num);
        c1.initShares(ad_num);
        c1.deserial_addShares_parallel_opt(shares_vec);

        vector<vector<BIGNUM*>> c1_share_opt = c1.getShares_opt();
        vector<BIGNUM*> sum_s1 =
            sumBNVec_opt(c1_share_opt, env, parallel, parallel_num);
        string sum_s1_str = serializeShareVec_opt(sum_s1);

        endtime = system_clock::now();
        cout << "TIME: deserialize shares + sum: "
             << duration_cast<std::chrono::duration<double>>(endtime -
                                                             starttime)
                    .count()
             << endl;

        starttime = system_clock::now();

        // exchange hash
        string sum_s1_str_hash = sha256(sum_s1_str);
        thread t1_hash(&sendShare, client_fd_write, ref(sum_s1_str_hash));
        string sum_s2_str_hash;
        thread t2_hash(&recvShare, client_fd_read, ref(sum_s2_str_hash));
        t2_hash.join();
        t1_hash.join();

        // send then receive
        thread t1(&sendShare, client_fd_write, ref(sum_s1_str));
        string sum_s2_str;
        thread t2(&recvShare, client_fd_read, ref(sum_s2_str));
        t2.join();
        assert(sha256(sum_s2_str) == sum_s2_str_hash);
        vector<BIGNUM*> sum_s2 = c1.bn_deserializeShare_opt(sum_s2_str);
        t1.join();

        endtime = system_clock::now();
        cout << "TIME: recv shares + deserialize: "
             << duration_cast<std::chrono::duration<double>>(endtime -
                                                             starttime)
                    .count()
             << endl;

        vector<vector<BIGNUM*>> share_input;
        share_input.emplace_back(sum_s1);
        share_input.emplace_back(sum_s2);
        vector<BIGNUM*> sum_vec_s = sumBNVec_opt(share_input, env);
        decode_bid = decode_bit_vec_opt(sum_vec_s);
        start_bucket = decode_bid * interval + start_bucket;
        cout << "AND decode (using shares): " << decode_bid << endl;

        c_vec.emplace_back(c1);
        sum_vec.emplace_back(sum_s1);
        shares_vec.clear();
        decode_vec.emplace_back(start_bucket);
        shares_vec = vector<string>(ad_num);
    }

    starttime = system_clock::now();
    // pre-defined seed
    vector<int> ids;
    for (int i = 0; i < ad_num; i++) {
        ids.push_back(i);
    }
    srand(100);

    // decide sequence
    vector<int> seq_ids;
    while (!ids.empty()) {
        int tmp_id = rand() % ids.size();
        seq_ids.push_back(ids[tmp_id]);
        cout << ids[tmp_id] << " ";
        ids.erase(ids.begin() + tmp_id);
    }
    cout << endl;

    for (int i = 0; i < revealed_ids.size(); i++) {
        cout << revealed_ids[i] << " ";
    }
    cout << endl;

    // serialize shares
    string all_bits_s1_str;
    for (int i = 0; i < seq_ids.size(); i++) {
        vector<BIGNUM*> s1;
        s1.emplace_back(c_vec[round - 1].revealBitShare_opt(
            seq_ids[i], decode_bid * LAMBDA + 0));
        s1.emplace_back(c_vec[round - 1].revealBitShare_opt(
            seq_ids[i], decode_bid * LAMBDA + 1));
        string s1_str = serializeBit(s1);
        all_bits_s1_str.append(formatMsg(s1_str));
    }

    // exchange hash
    string all_bits_s1_str_hash = sha256(all_bits_s1_str);
    thread t1_winner_hash(&sendShare, client_fd_write,
                          ref(all_bits_s1_str_hash));
    string all_bits_s2_str_hash;
    thread t2_winner_hash(&recvShare, client_fd_read,
                          ref(all_bits_s2_str_hash));
    t2_winner_hash.join();
    t1_winner_hash.join();

    thread t1_winner(&sendShare, client_fd_write, ref(all_bits_s1_str));
    string all_bits_s2_str;
    thread t2_winner(&recvShare, client_fd_read, ref(all_bits_s2_str));
    t2_winner.join();
    t1_winner.join();
    assert(sha256(all_bits_s2_str) == all_bits_s2_str_hash);

    int winner_id;
    for (int i = 0; i < seq_ids.size(); i++) {
        vector<BIGNUM*> s1;
        s1.emplace_back(c_vec[round - 1].revealBitShare_opt(
            seq_ids[i], decode_bid * LAMBDA));
        s1.emplace_back(c_vec[round - 1].revealBitShare_opt(
            seq_ids[i], decode_bid * LAMBDA + 1));
        winner_id = seq_ids[i];

        uint32_t size_s2_str = ntohl(
            *((uint32_t*)all_bits_s2_str.substr(0, sizeof(uint32_t)).c_str()));
        string s2_str = all_bits_s2_str.substr(sizeof(uint32_t), size_s2_str);
        all_bits_s2_str =
            all_bits_s2_str.substr(sizeof(uint32_t) + size_s2_str);

        vector<BIGNUM*> s2 = deserializeBit(s2_str);

        vector<BIGNUM*> s;
        for (int i = 0; i < LAMBDA; i++) {
            BIGNUM* v = BN_new();
            env.add_mod(v, s1[i], s2[i]);
            s.push_back(v);
        }
        int bit = decode_bit(s);
        if (bit == 1) {
            DEBUG << winner_id << " is the winner\n";
            break;
        }
    }

    ask_adv_bid(winner_id, start_bucket);

    endtime = system_clock::now();
    cout << "TIME: find winner: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    if (first_price) {
        cout << "You are running first price auction, stop here" << endl;
        return 0;
    }

    starttime = system_clock::now();
    // find second highest
    int second_price = 0;
    start_bucket = 0;
    interval = total_bucket;
    for (int loop = 1; loop <= round; loop++) {
        shares_vec.clear();
        shares_vec = vector<string>(ad_num - 1);
        interval = interval / base_bucket;
        vector<BIGNUM*> sum_s1_removed, sum_s2_removed;
        if (loop == 1) {
            cout << "enter here: using local share\n";
            BUCKET_NUM = round * base_bucket;
            start = system_clock::now();
            for (int i = 0; i < round; i++) {
                vector<BIGNUM*> removed_s1 =
                    c_vec[i].revealAdShare_opt(winner_id);
                vector<BIGNUM*> sum_s1_removed_round = sum_vec[i];
                subShare_opt(sum_s1_removed_round, removed_s1, env);
                sum_s1_removed.insert(sum_s1_removed.end(),
                                      sum_s1_removed_round.begin(),
                                      sum_s1_removed_round.end());
            }
        } else {
            char start_str[8];
            memcpy(start_str, &start_bucket, 4);
            memcpy(start_str + 4, &loop, 4);
            string start_msg = string((const char*)start_str, 8);
            vector<thread> t_vec;
            for (int i = 0; i < ad_num; i++) {
                thread t(&send_and_recv_share, i, ad_num, ref(start_msg),
                         winner_id);
                t_vec.push_back(move(t));
            }
            for (auto& th : t_vec) {
                th.join();
            }
            Committee c2 =
                Committee(base_bucket, LAMBDA, parallel, parallel_num);
            c2.initShares(ad_num - 1);
            c2.deserial_addShares_parallel_opt(shares_vec);
            vector<vector<BIGNUM*>> c2_share_opt = c2.getShares_opt();
            sum_s1_removed =
                sumBNVec_opt(c2_share_opt, env, parallel, parallel_num);
            cout << c2.getShares().size() << endl;
        }

        string sum_s1_str_removed = serializeShareVec_opt(sum_s1_removed);
        cout << "sum_s1_removed size: " << sum_s1_str_removed.size() << endl;

        // exchange hash
        string sum_s1_str_removed_hash = sha256(sum_s1_str_removed);
        thread t1_second2_hash(&sendShare, client_fd_write,
                               ref(sum_s1_str_removed_hash));
        string sum_s2_str_removed_hash;
        thread t2_second2_hash(&recvShare, client_fd_read,
                               ref(sum_s2_str_removed_hash));
        t2_second2_hash.join();
        t1_second2_hash.join();

        thread t1_second2(&sendShare, client_fd_write, ref(sum_s1_str_removed));
        string sum_s2_str_removed;
        thread t2_second2(&recvShare, client_fd_read, ref(sum_s2_str_removed));
        t2_second2.join();
        assert(sha256(sum_s2_str_removed) == sum_s2_str_removed_hash);
        sum_s2_removed = c.bn_deserializeShare_opt(sum_s2_str_removed);
        t1_second2.join();

        vector<vector<BIGNUM*>> removed_share_input;
        removed_share_input.emplace_back(sum_s1_removed);
        removed_share_input.emplace_back(sum_s2_removed);
        cout << "start sum removed" << endl;
        vector<BIGNUM*> sum_vec_removed =
            sumBNVec_opt(removed_share_input, env);
        cout << "finish sum removed" << endl;

        if (loop == 1) {
            vector<int> bits_res = decode_bit_vec_batch_opt(sum_vec_removed);
            for (int i = bits_res.size() - 1; i >= 0; i--) {
                if (bits_res[i] == 1) {
                    second_price = i;
                    break;
                }
            }
            DEBUG << "Round 1 AND decode on second price (using shares): "
                  << second_price << endl;
            start_bucket = revealed_ids[second_price];
            DEBUG << "second price start bucket: " << start_bucket << endl;
            BUCKET_NUM = base_bucket;
            loop = second_price / base_bucket + 1;
            for (int i = 1; i < loop; i++) {
                interval = interval / base_bucket;
            }
            cout << "after loop 1 enter round: " << loop
                 << " interval: " << interval << endl;
            continue;
        }

        int second_price = decode_bit_vec_opt(sum_vec_removed);
        start_bucket = second_price * interval + start_bucket;

        DEBUG << "AND decode on second price (using shares): " << second_price
              << endl;
    }
    endtime = system_clock::now();
    cout << "TIME: find second highest: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;
}