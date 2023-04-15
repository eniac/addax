#include "addax-lib.h"

void load_all_shares(string dir_name, string s1_file_names,
                     string s2_file_names, vector<string>& s1_vec,
                     vector<string>& s2_vec) {
    int ad_num = s1_vec.size();
    ifstream in1(s1_file_names);
    ifstream in2(s2_file_names);
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
    int task_num = 11;
    int parallel_num_committee = 8;
    int parallel_num_sum = 8;
    int parallel_sum = true;
    string s1_filenames = "";
    string s2_filenames = "";
    string dir_name = "";

    int opt;
    while ((opt = getopt(argc, argv, "a:b:s:S:d:t:")) != -1) {
        if (opt == 'a') {
            ad_num = atoi(optarg);
        } else if (opt == 'b') {
            BUCKET_NUM = atoi(optarg);
        } else if (opt == 's') {
            s1_filenames = string(optarg);
        } else if (opt == 'S') {
            s2_filenames = string(optarg);
        } else if (opt == 'd') {
            dir_name = string(optarg);
        } else if (opt == 't') {
            task_num = atoi(optarg);
        }
    }

    assert(s1_filenames != "");
    assert(s2_filenames != "");
    assert(dir_name != "");
    DEBUG << "simulating " << ad_num << " advertisers\n";
    DEBUG << "simulating " << BUCKET_NUM << " buckets\n";
    DEBUG << "lambda: " << LAMBDA << endl;
    DEBUG << "task num: " << task_num << endl;
    DEBUG << "s1 filename: " << s1_filenames << endl;
    DEBUG << "s2 filename: " << s2_filenames << endl;
    DEBUG << "dir name: " << dir_name << endl;

    vector<string> all_advs_s1_vec(ad_num);
    vector<string> all_advs_s2_vec(ad_num);

    double all_time = 0.0;

    // load all s1 s2 from files
    load_all_shares(dir_name, s1_filenames, s2_filenames, all_advs_s1_vec,
                    all_advs_s2_vec);

    system_clock::time_point starttime, endtime, start, end;

    for (int run_times = 0; run_times < task_num; run_times++) {
        double total_time = 0.0;
        Committee c1 =
            Committee(BUCKET_NUM, LAMBDA, parallel, parallel_num_committee);
        Committee c2 =
            Committee(BUCKET_NUM, LAMBDA, parallel, parallel_num_committee);
        DEBUG << run_times << " run\n";
        start = system_clock::now();

        // committee receive and decode share vecs
        starttime = system_clock::now();
        if (parallel) {
            c1.initShares(ad_num);
            c2.initShares(ad_num);
            c1.deserial_addShares_parallel_opt(all_advs_s1_vec);
            c2.deserial_addShares_parallel_opt(all_advs_s2_vec);
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
        total_time +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count() /
            2;

        // using shares to decode
        starttime = system_clock::now();
        vector<vector<BIGNUM*>> c1_share = c1.getShares_opt();
        vector<vector<BIGNUM*>> c2_share = c2.getShares_opt();
        vector<BIGNUM*> sum_s1 =
            sumBNVec_opt(c1_share, env, parallel_sum, parallel_num_sum);
        vector<BIGNUM*> sum_s2 =
            sumBNVec_opt(c2_share, env, parallel_sum, parallel_num_sum);

        endtime = system_clock::now();
        DEBUG << "TIME: running max using shares (sum local shares): "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                         .count() /
                     2
              << endl;

        total_time +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count() /
            2;

        starttime = system_clock::now();
        vector<vector<BIGNUM*>> share_input;
        share_input.emplace_back(sum_s1);
        share_input.emplace_back(sum_s2);
        // For simplicity, the local version still sums up randomness
        // in interactive version, randomness is not serialized when sending out
        // shares to another party, see serializeShareVec_out()
        vector<BIGNUM*> sum_vec_s = sumBNVec_opt(share_input, env);

        endtime = system_clock::now();
        DEBUG
            << "TIME: running max using shares (sum up shares of sum vectors): "
            << duration_cast<std::chrono::duration<double>>(endtime - starttime)
                   .count()
            << endl;
        total_time +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count();

        starttime = system_clock::now();
        int decode_bid = decode_bit_vec_opt(sum_vec_s);
        DEBUG << "AND decode (using shares): " << decode_bid << endl;
        endtime = system_clock::now();
        DEBUG << "TIME: running max using shares (decode): "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                     .count()
              << endl;
        total_time +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count();

        // start generate random and reveal shares
        // does not matter, we still find round by round as here we do not
        // really send shares out
        starttime = system_clock::now();
        int winner_id;
        vector<int> ids;
        for (int i = 0; i < ad_num; i++) {
            ids.push_back(i);
        }
        srand(100);
        while (true) {
            int tmp_id = rand() % ids.size();
            winner_id = ids[tmp_id];
            vector<BIGNUM*> s;
            for (int i = 0; i < LAMBDA; i++) {
                BIGNUM* v = BN_new();
                env.add_mod(
                    v,
                    c1.revealBitShare_opt(winner_id, decode_bid * LAMBDA + i),
                    c2.revealBitShare_opt(winner_id, decode_bid * LAMBDA + i));
                s.push_back(v);
            }

            int bit = decode_bit(s);
            if (bit == 1) {
                DEBUG << "(note here the id is not bidder id, it's sequence of "
                         "file names in s1_filenames/s2_filenames): "
                      << winner_id << " is the winner\n";
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

        // start next round
        starttime = system_clock::now();
        vector<BIGNUM*> removed_s1 = c1.revealAdShare_opt(winner_id);
        vector<BIGNUM*> removed_s2 = c2.revealAdShare_opt(winner_id);
        subShare_opt(sum_s1, removed_s1, env);
        subShare_opt(sum_s2, removed_s2, env);
        vector<vector<BIGNUM*>> removed_share_input;
        removed_share_input.push_back(sum_s1);
        removed_share_input.push_back(sum_s2);
        vector<BIGNUM*> sum_vec_removed =
            sumBNVec_opt(removed_share_input, env);
        int second_price = decode_bit_vec_opt(sum_vec_removed);
        DEBUG << "AND decode on second price (using shares): " << second_price
              << endl;
        endtime = system_clock::now();
        DEBUG << "TIME: finding second highest price: "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                     .count()
              << endl;

        total_time +=
            duration_cast<std::chrono::duration<double>>(endtime - starttime)
                .count();
        all_time += total_time;
        cout << "total time for this round: " << total_time << endl;
        freeSumBNvec_opt(sum_s1);
        freeSumBNvec_opt(sum_s2);
        freeSumBNvec_opt(sum_vec_s);
        freeSumBNvec_opt(sum_vec_removed);
        c1.free();
        c2.free();
    }
    return 0;
}
