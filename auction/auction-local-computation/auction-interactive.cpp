#include "addax-lib.h"

void load_all_shares(string dir_name, string s1_file_names,
                     string s2_file_names, vector<string>& s1_vec,
                     vector<string>& s2_vec) {
    int ad_num = s1_vec.size();
    ifstream in1(s1_file_names);
    ifstream in2(s2_file_names);
    cout << s1_file_names << " " << s2_file_names << endl;
    for (int i = 0; i < ad_num; i++) {
        string n1, n2;
        getline(in1, n1);
        getline(in2, n2);
        ifstream s1file;
        s1file.open(dir_name + "/" + n1);  // open the input file
        std::stringstream ss1;
        ss1 << s1file.rdbuf();  // read the file
        s1_vec[i] = ss1.str();
        ifstream s2file;
        s2file.open(dir_name + "/" + n2);  // open the input file
        std::stringstream ss2;
        ss2 << s2file.rdbuf();  // read the file
        s2_vec[i] = ss2.str();
        cout << (dir_name + "/" + n1) << " " << (dir_name + "/" + n2) << endl;
    }
    in1.close();
    in2.close();
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
    int task_num = 1;
    int parallel_num_committee = 8;
    int parallel_num_sum = 8;
    int parallel_sum = true;
    int total_loop = 2;

    string s1_filenames = "";
    string s2_filenames = "";
    string dir_name = "";

    int opt;
    while ((opt = getopt(argc, argv, "a:b:t:r:s:S:d:")) != -1) {
        if (opt == 'a') {
            ad_num = atoi(optarg);
        } else if (opt == 'b') {
            BUCKET_NUM = atoi(optarg);
        } else if (opt == 't') {
            task_num = atoi(optarg);
        } else if (opt == 'r') {
            total_loop = atoi(optarg);
        } else if (opt == 's') {
            s1_filenames = string(optarg);
        } else if (opt == 'S') {
            s2_filenames = string(optarg);
        } else if (opt == 'd') {
            dir_name = string(optarg);
        }
    }

    assert(s1_filenames != "");
    assert(s2_filenames != "");
    assert(dir_name != "");

    int total_bucket = 10000;
    int base_bucket = 100;
    if (total_loop == 2) {
        base_bucket = 100;
    } else if (total_loop == 4) {
        base_bucket = 10;
        // for 4-round do not need to sum in parallel
        parallel_sum = false;
    } else {
        assert(0);
    }
    BUCKET_NUM = base_bucket;

    DEBUG << "simulating " << ad_num << " advertisers\n";
    DEBUG << "simulating " << BUCKET_NUM << " buckets\n";
    DEBUG << "lambda: " << LAMBDA << endl;
    DEBUG << "task num: " << task_num << endl;
    DEBUG << "round: " << total_loop << endl;
    DEBUG << "base bucket: " << base_bucket << endl;

    vector<vector<string>> all_advs_s1_vec;
    vector<vector<string>> all_advs_s2_vec;

    // load all files
    for (int i = 0; i < total_loop; i++) {
        vector<string> s1_vec_round(ad_num);
        vector<string> s2_vec_round(ad_num);
        load_all_shares(dir_name + "/" + to_string(i + 1),
                        s1_filenames + "/" + to_string(i + 1) + "-round-s1-idx",
                        s1_filenames + "/" + to_string(i + 1) + "-round-s2-idx",
                        s1_vec_round, s2_vec_round);
        all_advs_s1_vec.push_back(s1_vec_round);
        all_advs_s2_vec.push_back(s2_vec_round);
    }

    // load files with one fewer bidder
    for (int i = 0; i < total_loop; i++) {
        vector<string> s1_vec_round(ad_num - 1);
        vector<string> s2_vec_round(ad_num - 1);
        load_all_shares(dir_name + "/" + to_string(i + 1 + total_loop),
                        s1_filenames + "/" + to_string(i + 1 + total_loop) +
                            "-round-s1-idx",
                        s1_filenames + "/" + to_string(i + 1 + total_loop) +
                            "-round-s2-idx",
                        s1_vec_round, s2_vec_round);
        all_advs_s1_vec.push_back(s1_vec_round);
        all_advs_s2_vec.push_back(s2_vec_round);
    }

    double total_time = 0.0;
    double all_time = 0.0;

    system_clock::time_point starttime, endtime, start, end;

    for (int run_times = 0; run_times < task_num; run_times++) {
        vector<Committee> c1_vec;
        vector<Committee> c2_vec;
        vector<vector<BIGNUM*>> sum1_vec;
        vector<vector<BIGNUM*>> sum2_vec;

        DEBUG << run_times << " run\n";
        total_time = 0.0;

        int start_bucket = 0;
        int interval = total_bucket;
        int decode_bid = 0;
        for (int loop = 1; loop <= total_loop; loop++) {
            Committee c1 = Committee(base_bucket, LAMBDA, parallel,
                                     parallel_num_committee);
            Committee c2 = Committee(base_bucket, LAMBDA, parallel,
                                     parallel_num_committee);
            interval = interval / base_bucket;

            start = system_clock::now();
            BUCKET_NUM = base_bucket;
            // committee receive and decode share vecs
            starttime = system_clock::now();
            if (parallel) {
                c1.initShares(ad_num);
                c2.initShares(ad_num);
                c1.deserial_addShares_parallel_opt(all_advs_s1_vec[loop - 1]);
                c2.deserial_addShares_parallel_opt(all_advs_s2_vec[loop - 1]);
            } else {
                assert(0);
            }
            endtime = system_clock::now();
            DEBUG << "TIME: deserialize share vecs per committee: "
                  << duration_cast<std::chrono::duration<double>>(endtime -
                                                                  starttime)
                             .count() /
                         2
                  << endl;
            total_time += duration_cast<std::chrono::duration<double>>(
                              endtime - starttime)
                              .count() /
                          2;

            // using shares to decode
            starttime = system_clock::now();
            vector<vector<BIGNUM*>> c1_shares_opt = c1.getShares_opt();
            vector<vector<BIGNUM*>> c2_shares_opt = c2.getShares_opt();
            vector<BIGNUM*> sum_s1 = sumBNVec_opt(
                c1_shares_opt, env, parallel_sum, parallel_num_sum);
            vector<BIGNUM*> sum_s2 = sumBNVec_opt(
                c2_shares_opt, env, parallel_sum, parallel_num_sum);

            endtime = system_clock::now();
            DEBUG << "TIME: running max using shares (sum local shares): "
                  << duration_cast<std::chrono::duration<double>>(endtime -
                                                                  starttime)
                             .count() /
                         2
                  << endl;

            total_time += duration_cast<std::chrono::duration<double>>(
                              endtime - starttime)
                              .count() /
                          2;

            starttime = system_clock::now();
            vector<vector<BIGNUM*>> share_input;
            share_input.emplace_back(sum_s1);
            share_input.emplace_back(sum_s2);
            vector<BIGNUM*> sum_vec_s = sumBNVec_opt(share_input, env);
            endtime = system_clock::now();
            DEBUG << "TIME: running max using shares (sum up shares of sum "
                     "vectors): "
                  << duration_cast<std::chrono::duration<double>>(endtime -
                                                                  starttime)
                         .count()
                  << endl;
            total_time += duration_cast<std::chrono::duration<double>>(
                              endtime - starttime)
                              .count();

            starttime = system_clock::now();
            decode_bid = decode_bit_vec_opt(sum_vec_s);
            start_bucket = decode_bid * interval + start_bucket;
            DEBUG << "AND decode (using shares): " << start_bucket << endl;

            endtime = system_clock::now();
            DEBUG << "TIME: running max using shares (decode): "
                  << duration_cast<std::chrono::duration<double>>(endtime -
                                                                  starttime)
                         .count()
                  << endl;
            c1_vec.push_back(c1);
            c2_vec.push_back(c2);
            sum1_vec.emplace_back(sum_s1);
            sum2_vec.emplace_back(sum_s2);
            total_time += duration_cast<std::chrono::duration<double>>(
                              endtime - starttime)
                              .count();
        }

        // start generate random and reveal shares
        starttime = system_clock::now();
        int winner_id;
        vector<int> ids;
        for (int i = 0; i < ad_num; i++) {
            ids.push_back(i);
        }
        // pre-generated unbiased random seed, as it's local it's as expensive
        // as revealing all ties. Won't affect latency.
        srand(100);
        while (true) {
            int tmp_id = rand() % ids.size();
            winner_id = ids[tmp_id];
            vector<BIGNUM*> s;
            for (int i = 0; i < LAMBDA; i++) {
                BIGNUM* v = BN_new();
                env.add_mod(v,
                            c1_vec[total_loop - 1].revealBitShare_opt(
                                winner_id, decode_bid * LAMBDA + i),
                            c2_vec[total_loop - 1].revealBitShare_opt(
                                winner_id, decode_bid * LAMBDA + i));
                s.push_back(v);
            }
            int bit = decode_bit(s);
            if (bit == 1) {
                DEBUG << winner_id << " is the winner\n";
                break;
            }
            ids.erase(ids.begin() + tmp_id);
        }
        endtime = system_clock::now();
        DEBUG << "TIME: finding index of winner: "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                     .count()
              << endl;
        total_time +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count();

        // sale price
        int second_price = 0;
        start_bucket = 0;
        interval = total_bucket;
        for (int loop = 1; loop <= total_loop; loop++) {
            interval = interval / base_bucket;
            vector<BIGNUM*> sum_s1_removed, sum_s2_removed;
            if (loop == 1) {
                cout << "enter here: using local share\n";
                BUCKET_NUM = total_loop * base_bucket;
                start = system_clock::now();
                for (int i = 0; i < total_loop; i++) {
                    vector<BIGNUM*> removed_s1 =
                        c1_vec[i].revealAdShare_opt(winner_id);
                    vector<BIGNUM*> sum_s1_removed_round = sum1_vec[i];
                    subShare_opt(sum_s1_removed_round, removed_s1, env);
                    sum_s1_removed.insert(sum_s1_removed.end(),
                                          sum_s1_removed_round.begin(),
                                          sum_s1_removed_round.end());
                    vector<BIGNUM*> removed_s2 =
                        c2_vec[i].revealAdShare_opt(winner_id);
                    vector<BIGNUM*> sum_s2_removed_round = sum2_vec[i];
                    subShare_opt(sum_s2_removed_round, removed_s2, env);
                    sum_s2_removed.insert(sum_s2_removed.end(),
                                          sum_s2_removed_round.begin(),
                                          sum_s2_removed_round.end());
                }
                end = system_clock::now();
                total_time +=
                    duration_cast<std::chrono::duration<double>>(end - start)
                        .count();
            } else {
                start = system_clock::now();

                Committee c1 =
                    Committee(base_bucket, LAMBDA, parallel, parallel_num);
                c1.initShares(ad_num - 1);
                c1.deserial_addShares_parallel_opt(
                    all_advs_s1_vec[loop + total_loop - 1]);
                Committee c2 =
                    Committee(base_bucket, LAMBDA, parallel, parallel_num);
                c2.initShares(ad_num - 1);
                c2.deserial_addShares_parallel_opt(
                    all_advs_s2_vec[loop + total_loop - 1]);
                vector<vector<BIGNUM*>> c1_sum_opt = c1.getShares_opt();
                vector<vector<BIGNUM*>> c2_sum_opt = c2.getShares_opt();
                sum_s1_removed =
                    sumBNVec_opt(c1_sum_opt, env, parallel_sum, parallel_num);
                sum_s2_removed =
                    sumBNVec_opt(c2_sum_opt, env, parallel_sum, parallel_num);
                end = system_clock::now();
                total_time +=
                    duration_cast<std::chrono::duration<double>>(end - start)
                        .count() /
                    2;
            }

            start = system_clock::now();
            vector<vector<BIGNUM*>> removed_share_input;
            removed_share_input.push_back(sum_s1_removed);
            removed_share_input.push_back(sum_s2_removed);
            vector<BIGNUM*> sum_vec_removed =
                sumBNVec_opt(removed_share_input, env);

            if (loop == 1) {
                vector<int> bits_res =
                    decode_bit_vec_batch_opt(sum_vec_removed);
                for (int i = bits_res.size() - 1; i >= 0; i--) {
                    if (bits_res[i] == 1) {
                        second_price = i;
                        break;
                    }
                }
                DEBUG << "Round 1 AND decode on second price (using shares): "
                      << second_price << endl;
                // as the shares are just random, for simplicity just use 1000
                // here for simulation of local time...
                start_bucket = 1000;
                DEBUG << "second price start bucket: " << start_bucket << endl;
                BUCKET_NUM = base_bucket;
                end = system_clock::now();
                total_time +=
                    duration_cast<std::chrono::duration<double>>(end - start)
                        .count();
                cout << "after loop 1 enter round: " << loop
                     << " interval: " << interval << endl;
                continue;
            }

            second_price = decode_bit_vec_opt(sum_vec_removed);
            start_bucket = second_price * interval + start_bucket;
            end = system_clock::now();
            total_time +=
                duration_cast<std::chrono::duration<double>>(end - start)
                    .count();

            DEBUG << "AND decode on second price (using shares): "
                  << second_price << endl;
            DEBUG << "second price start bucket (note: these are bogus shares, "
                     "so ignore this number): "
                  << start_bucket << endl;
        }

        for (int i = 0; i < total_loop; i++) {
            c1_vec[i].free();
            c2_vec[i].free();
            freeSumBNvec_opt(sum1_vec[i]);
            freeSumBNvec_opt(sum2_vec[i]);
        }
        cout << "total time for " << run_times << " run: " << total_time
             << endl;
    }
    return 0;
}
