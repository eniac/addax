#include <map>

#include "addax-lib.h"
#include "net.hpp"

vector<int> adv_fds;
map<int, int> fd_map;
vector<string> shares_vec;

void conn_and_recv_share(int id, int parallel_num, vector<string> ips) {
    for (int i = 0; i < ips.size(); i++) {
        if (i % parallel_num != id) continue;
        string ip = ips[i];
        cout << "connect to: " << ip << endl;
        string response;
        int fd = connect_to_addr(ip);
        assert(fd > 0);
        bool recv_flag = recv_response(fd, response);
        assert(recv_flag);
        char a = *(response.substr(0, 1).c_str());
        int idx = a;
        // cout << fd << " recv id " << idx << " size: " << response.size() <<
        // endl;
        shares_vec[idx] = response.substr(1);
        adv_fds[idx] = fd;
    }
}

void send_and_recv_share(int id, int parallel_num, string& msg, int winner_id) {
    for (int i = 0; i < adv_fds.size(); i++) {
        if (i % parallel_num != id || (winner_id != -1 && i == winner_id))
            continue;
        string response;
        int fd = adv_fds[i];
        assert(sendMsg(fd, msg));
        bool recv_flag = recv_response(fd, response);
        assert(recv_flag);
        char a = *(response.substr(0, 1).c_str());
        int idx = a;
        // cout << fd << " recv id " << idx << " size: " << response.size() <<
        // endl;
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
    if (bid != resp) {
        cout << bid << " " << resp << endl;
    }
    assert(bid == resp);
}

void connect_to_publisher(int* fd, string ip) { *fd = connect_to_addr(ip); }

int main(int argc, char* argv[]) {
    Crypto env = Crypto();
    BIGNUM* p = env.getP();
    int bit_len = env.bitLen();
    BN_CTX* ctx = env.getCtx();
    int order_bit_len = env.orderBitLen();

    int ad_num = 20;
    bool parallel = true;
    int parallel_num = 8;
    string client_ip = "0.0.0.0:6666";
    string publisher_ip = "0.0.0.0:6667";
    int idx = 0;
    string filename = "advs.txt";
    bool interactive = true;
    int round = 2;
    int base_bucket = 100;
    // whether to run first price baseline
    // false default (second price auction by default)
    bool first_price = false;

    int opt;
    while ((opt = getopt(argc, argv, "a:b:l:n:c:p:i:f:r:q")) != -1) {
        if (opt == 'a') {
            ad_num = atoi(optarg);
        } else if (opt == 'b') {
            BUCKET_NUM = atoi(optarg);
        } else if (opt == 'l') {
            LAMBDA = atoi(optarg);
        } else if (opt == 'p') {
            publisher_ip = string(optarg);
        } else if (opt == 'n') {
            parallel_num = atoi(optarg);
        } else if (opt == 'c') {
            client_ip = string(optarg);
        } else if (opt == 'i') {
            idx = atoi(optarg);
        } else if (opt == 'f') {
            filename = string(optarg);
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
    DEBUG << "publisher ip: " << publisher_ip << endl;
    DEBUG << "client ip: " << client_ip << endl;
    DEBUG << "index: " << idx << endl;
    DEBUG << "filename: " << filename << endl;
    DEBUG << "interactive: " << interactive << endl;
    DEBUG << "running as first price? " << first_price << endl;

    int p_port = 6666;

    adv_fds = vector<int>(ad_num, 0);
    shares_vec = vector<string>(ad_num);
    int total_bucket = BUCKET_NUM;
    int interval = total_bucket / base_bucket;
    int start_bucket = 0;

    BUCKET_NUM = base_bucket;

    // read list of advertisers from advs.txt
    vector<string> adv_ips;
    fstream fs;
    fs.open(filename, fstream::in);
    int curr_read_id = 0;
    for (std::string line; getline(fs, line);) {
        adv_ips.push_back(line);
        curr_read_id++;
        if (curr_read_id == ad_num) {
            assert(adv_ips.size() == ad_num);
            break;
        }
    }
    double net_total = 0.0;
    double deserialize_total = 0.0;
    double compute_total = 0.0;
    system_clock::time_point starttime, endtime, start, end;
    system_clock::time_point start_total, end_total;
    starttime = system_clock::now();
    start_total = system_clock::now();

    start = system_clock::now();
    // connect to publisher
    int publisher_fd_write, publisher_fd_read;
    thread t1_connect(&connect_to_publisher, &publisher_fd_read,
                      publisher_ip + ":" + to_string(p_port));
    thread t2_connect(&connect_to_publisher, &publisher_fd_write,
                      publisher_ip + ":" + to_string(p_port + 1));

    endtime = system_clock::now();
    net_total +=
        duration_cast<std::chrono::duration<double>>(endtime - starttime)
            .count();
    cout << "TIME: connect to publisher: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    starttime = system_clock::now();

    vector<thread> t_vec;
    for (int i = 0; i < ad_num; i++) {
        thread t(&conn_and_recv_share, i, ad_num, adv_ips);
        t_vec.push_back(move(t));
    }

    t1_connect.join();
    t2_connect.join();
    assert(publisher_fd_write > 0);
    assert(publisher_fd_read > 0);
    cout << "publisher fd: " << publisher_fd_read << " " << publisher_fd_write
         << endl;

    for (auto& th : t_vec) {
        th.join();
    }

    endtime = system_clock::now();
    net_total +=
        duration_cast<std::chrono::duration<double>>(endtime - starttime)
            .count();
    cout << "TIME: receive shares: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    starttime = system_clock::now();

    vector<Committee> c_vec;
    vector<vector<BIGNUM*>> sum_vec;
    vector<int> decode_vec;
    vector<int> revealed_ids =
        generate_ids(1, total_bucket, base_bucket, start_bucket);

    Committee c = Committee(base_bucket, LAMBDA, parallel, parallel_num);
    c.initShares(ad_num);
    c.deserial_addShares_parallel_opt(shares_vec);

    vector<vector<BIGNUM*>> recv_shares = c.getShares_opt();
    vector<BIGNUM*> sum_s1 =
        sumBNVec_opt(recv_shares, env, parallel, parallel_num);
    string sum_s1_str = serializeShareVec_opt(sum_s1);

    endtime = system_clock::now();
    deserialize_total +=
        duration_cast<std::chrono::duration<double>>(endtime - starttime)
            .count();
    cout << "TIME: deserialize shares + sum: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    starttime = system_clock::now();

    // exchange hash
    string sum_s1_str_hash = sha256(sum_s1_str);
    thread t1_hash(&sendShare, publisher_fd_write, ref(sum_s1_str_hash));
    string sum_s2_str_hash;
    thread t2_hash(&recvShare, publisher_fd_read, ref(sum_s2_str_hash));
    t2_hash.join();
    t1_hash.join();

    // send then receive
    thread t1(&sendShare, publisher_fd_write, ref(sum_s1_str));
    string sum_s2_str;
    thread t2(&recvShare, publisher_fd_read, ref(sum_s2_str));
    t2.join();
    endtime = system_clock::now();
    net_total +=
        duration_cast<std::chrono::duration<double>>(endtime - starttime)
            .count();

    cout << "TIME: send + recv first time "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    starttime = system_clock::now();
    assert(sha256(sum_s2_str) == sum_s2_str_hash);
    vector<BIGNUM*> sum_s2 = c.bn_deserializeShare_opt(sum_s2_str);
    t1.join();

    endtime = system_clock::now();
    deserialize_total +=
        duration_cast<std::chrono::duration<double>>(endtime - starttime)
            .count();

    cout << "TIME: deserialize first time "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    starttime = system_clock::now();
    vector<vector<BIGNUM*>> share_input;
    share_input.emplace_back(sum_s1);
    share_input.emplace_back(sum_s2);
    vector<BIGNUM*> sum_vec_s = sumBNVec_opt(share_input, env);
    int decode_bid = decode_bit_vec_opt(sum_vec_s);
    start_bucket = decode_bid * interval + start_bucket;
    cout << "AND decode (using shares): " << decode_bid << endl;

    endtime = system_clock::now();
    compute_total +=
        duration_cast<std::chrono::duration<double>>(endtime - starttime)
            .count();
    cout << "TIME: decode max: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    end = system_clock::now();
    cout << "TIME: conn + recv + compute first round: "
         << duration_cast<std::chrono::duration<double>>(end - start).count()
         << endl;

    c_vec.emplace_back(c);
    sum_vec.emplace_back(sum_s1);
    decode_vec.emplace_back(start_bucket);
    shares_vec.clear();
    shares_vec = vector<string>(ad_num);
    start = system_clock::now();

    for (int loop = 2; loop <= round; loop++) {
        interval = interval / base_bucket;
        vector<int> ids =
            generate_ids(loop, total_bucket, base_bucket, start_bucket);
        revealed_ids.insert(revealed_ids.end(), ids.begin(), ids.end());
        // send start buckt to all advertisers
        starttime = system_clock::now();
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
        endtime = system_clock::now();
        net_total +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count();
        cout << "TIME: recv share, round: "
             << duration_cast<std::chrono::duration<double>>(endtime -
                                                             starttime)
                    .count()
             << endl;

        starttime = system_clock::now();

        // repeat running
        Committee c1 = Committee(base_bucket, LAMBDA, parallel, parallel_num);
        c1.initShares(ad_num);
        c1.deserial_addShares_parallel_opt(shares_vec);

        vector<vector<BIGNUM*>> c1_shares_opt = c1.getShares_opt();
        vector<BIGNUM*> sum_s1 =
            sumBNVec_opt(c1_shares_opt, env, parallel, parallel_num);
        string sum_s1_str = serializeShareVec_opt(sum_s1);

        endtime = system_clock::now();
        compute_total +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count();

        starttime = system_clock::now();

        // exchange hash
        string sum_s1_str_hash = sha256(sum_s1_str);
        thread t1_hash(&sendShare, publisher_fd_write, ref(sum_s1_str_hash));
        string sum_s2_str_hash;
        thread t2_hash(&recvShare, publisher_fd_read, ref(sum_s2_str_hash));
        t2_hash.join();
        t1_hash.join();

        // send then receive
        thread t1(&sendShare, publisher_fd_write, ref(sum_s1_str));
        string sum_s2_str;
        thread t2(&recvShare, publisher_fd_read, ref(sum_s2_str));
        t2.join();
        endtime = system_clock::now();
        net_total +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count();
        cout << "TIME: send and recv to publisher: "
             << duration_cast<std::chrono::duration<double>>(endtime -
                                                             starttime)
                    .count()
             << endl;

        starttime = system_clock::now();
        assert(sha256(sum_s2_str) == sum_s2_str_hash);
        vector<BIGNUM*> sum_s2 = c1.bn_deserializeShare_opt(sum_s2_str);
        t1.join();
        endtime = system_clock::now();
        deserialize_total +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count();
        cout << "TIME: deserialize: "
             << duration_cast<std::chrono::duration<double>>(endtime -
                                                             starttime)
                    .count()
             << endl;

        starttime = system_clock::now();
        vector<vector<BIGNUM*>> share_input;
        share_input.emplace_back(sum_s1);
        share_input.emplace_back(sum_s2);
        vector<BIGNUM*> sum_vec_s = sumBNVec_opt(share_input, env);
        decode_bid = decode_bit_vec_opt(sum_vec_s);
        start_bucket = decode_bid * interval + start_bucket;
        cout << "AND decode (using shares): " << decode_bid << endl;
        cout << "start_bucket: " << start_bucket << endl;

        c_vec.push_back(c1);
        sum_vec.push_back(sum_s1);
        shares_vec.clear();
        decode_vec.push_back(start_bucket);
        shares_vec = vector<string>(ad_num);
        endtime = system_clock::now();
        compute_total +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count();
    }

    end = system_clock::now();
    cout << "TIME: highest bid first round: "
         << duration_cast<std::chrono::duration<double>>(end - start).count()
         << endl;

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

    cout << "sequence start\n";
    for (int i = 0; i < revealed_ids.size(); i++) {
        cout << revealed_ids[i] << " ";
    }
    cout << endl;

    cout << "serialize start\n";

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
    cout << "serialize finish\n";

    start = system_clock::now();

    // exchange hash
    string all_bits_s1_str_hash = sha256(all_bits_s1_str);
    thread t1_winner_hash(&sendShare, publisher_fd_write,
                          ref(all_bits_s1_str_hash));
    string all_bits_s2_str_hash;
    thread t2_winner_hash(&recvShare, publisher_fd_read,
                          ref(all_bits_s2_str_hash));
    t2_winner_hash.join();
    t1_winner_hash.join();

    thread t1_winner(&sendShare, publisher_fd_write, ref(all_bits_s1_str));
    string all_bits_s2_str;
    thread t2_winner(&recvShare, publisher_fd_read, ref(all_bits_s2_str));
    t2_winner.join();
    t1_winner.join();
    end = system_clock::now();
    net_total +=
        duration_cast<std::chrono::duration<double>>(end - start).count();
    assert(sha256(all_bits_s2_str) == all_bits_s2_str_hash);

    int winner_id;
    for (int i = 0; i < seq_ids.size(); i++) {
        vector<BIGNUM*> s1;
        s1.emplace_back(c_vec[round - 1].revealBitShare_opt(
            seq_ids[i], decode_bid * LAMBDA));
        s1.emplace_back(c_vec[round - 1].revealBitShare_opt(
            seq_ids[i], decode_bid * LAMBDA + 1));

        start = system_clock::now();

        winner_id = seq_ids[i];
        uint32_t size_s2_str = ntohl(
            *((uint32_t*)all_bits_s2_str.substr(0, sizeof(uint32_t)).c_str()));
        string s2_str = all_bits_s2_str.substr(sizeof(uint32_t), size_s2_str);
        all_bits_s2_str =
            all_bits_s2_str.substr(sizeof(uint32_t) + size_s2_str);

        vector<BIGNUM*> s2 = deserializeBit(s2_str);
        end = system_clock::now();
        deserialize_total +=
            duration_cast<std::chrono::duration<double>>(end - start).count();

        start = system_clock::now();
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
        // DEBUG << winner_id << " is not the winner\n";
        end = system_clock::now();
        compute_total +=
            duration_cast<std::chrono::duration<double>>(end - start).count();
    }

    endtime = system_clock::now();
    cout << "TIME: find winner: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    starttime = system_clock::now();
    ask_adv_bid(winner_id, start_bucket);
    endtime = system_clock::now();
    net_total +=
        duration_cast<std::chrono::duration<double>>(endtime - starttime)
            .count();
    cout << "TIME: ask winner: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;

    if (first_price) {
        cout << "You are running first price auction, stop here" << endl;
        end_total = system_clock::now();
        cout << "TIME: network total: " << net_total << endl;
        cout << "TIME: searialize + deserialize total: " << deserialize_total
             << endl;
        cout << "TIME: compute total: " << compute_total << endl;
        cout << "TIME: total: "
             << duration_cast<std::chrono::duration<double>>(end_total -
                                                             start_total)
                    .count()
             << endl;
        return 0;
    }

    // // find second highest
    starttime = system_clock::now();
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
            end = system_clock::now();
            compute_total +=
                duration_cast<std::chrono::duration<double>>(end - start)
                    .count();
        } else {
            start = system_clock::now();
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
            end = system_clock::now();
            net_total +=
                duration_cast<std::chrono::duration<double>>(end - start)
                    .count();

            start = system_clock::now();
            Committee c2 =
                Committee(base_bucket, LAMBDA, parallel, parallel_num);
            c2.initShares(ad_num - 1);
            c2.deserial_addShares_parallel_opt(shares_vec);
            end = system_clock::now();
            compute_total +=
                duration_cast<std::chrono::duration<double>>(end - start)
                    .count();
            start = system_clock::now();
            vector<vector<BIGNUM*>> c2_shares_opt = c2.getShares_opt();
            sum_s1_removed =
                sumBNVec_opt(c2_shares_opt, env, parallel, parallel_num);
            end = system_clock::now();
            compute_total +=
                duration_cast<std::chrono::duration<double>>(end - start)
                    .count();
        }

        start = system_clock::now();
        string sum_s1_str_removed = serializeShareVec_opt(sum_s1_removed);
        end = system_clock::now();
        deserialize_total +=
            duration_cast<std::chrono::duration<double>>(end - start).count();

        start = system_clock::now();

        // exchange hash
        string sum_s1_str_removed_hash = sha256(sum_s1_str_removed);
        thread t1_second2_hash(&sendShare, publisher_fd_write,
                               ref(sum_s1_str_removed_hash));
        string sum_s2_str_removed_hash;
        thread t2_second2_hash(&recvShare, publisher_fd_read,
                               ref(sum_s2_str_removed_hash));
        t2_second2_hash.join();
        t1_second2_hash.join();

        thread t1_second2(&sendShare, publisher_fd_write,
                          ref(sum_s1_str_removed));
        string sum_s2_str_removed;
        thread t2_second2(&recvShare, publisher_fd_read,
                          ref(sum_s2_str_removed));
        t2_second2.join();
        end = system_clock::now();
        net_total +=
            duration_cast<std::chrono::duration<double>>(end - start).count();

        start = system_clock::now();
        assert(sha256(sum_s2_str_removed) == sum_s2_str_removed_hash);
        sum_s2_removed = c.bn_deserializeShare_opt(sum_s2_str_removed);
        t1_second2.join();
        end = system_clock::now();
        deserialize_total +=
            duration_cast<std::chrono::duration<double>>(end - start).count();

        start = system_clock::now();
        vector<vector<BIGNUM*>> removed_share_input;
        removed_share_input.emplace_back(sum_s1_removed);
        removed_share_input.emplace_back(sum_s2_removed);
        vector<BIGNUM*> sum_vec_removed =
            sumBNVec_opt(removed_share_input, env);

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
            end = system_clock::now();
            compute_total +=
                duration_cast<std::chrono::duration<double>>(end - start)
                    .count();
            loop = second_price / base_bucket + 1;
            for (int i = 1; i < loop; i++) {
                interval = interval / base_bucket;
            }
            cout << "after loop 1 enter round: " << loop
                 << " interval: " << interval << endl;
            continue;
        }

        second_price = decode_bit_vec_opt(sum_vec_removed);
        start_bucket = second_price * interval + start_bucket;
        end = system_clock::now();
        compute_total +=
            duration_cast<std::chrono::duration<double>>(end - start).count();

        DEBUG << "AND decode on second price (using shares): " << second_price
              << endl;
        DEBUG << "second price start bucket: " << start_bucket << endl;
    }
    endtime = system_clock::now();
    cout << "TIME: find second highest: "
         << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count()
         << endl;
    end_total = system_clock::now();
    cout << "TIME: network total: " << net_total << endl;
    cout << "TIME: searialize + deserialize total: " << deserialize_total
         << endl;
    cout << "TIME: compute total: " << compute_total << endl;
    cout << "TIME: total: "
         << duration_cast<std::chrono::duration<double>>(end_total -
                                                         start_total)
                .count()
         << endl;
}