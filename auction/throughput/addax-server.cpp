#include <random>
#include <sstream>

#include "addax-lib.h"
#include "server.hpp"

vector<BIGNUM*> bn_deserialize_opt(string s) {
    unsigned char* str = (unsigned char*)s.c_str();
    int max_bn_bytes = 30;
    int pos = 0;
    vector<BIGNUM*> res = vector<BIGNUM*>(LAMBDA * BUCKET_NUM);
    for (int i = 0; i < BUCKET_NUM * LAMBDA; i++) {
        int len = (int)str[pos];
        assert(len < max_bn_bytes);
        res[i] = BN_bin2bn(str + pos + 1, len, NULL);
        pos += 1 + len;
        assert(pos <= s.size());
    }
    return res;
}

class AddaxServer : public Server {
    vector<string> s1_vec;
    vector<string> s2_vec;
    Crypto env;
    int ad_num = 96;
    int total_loop = 2;
    vector<vector<string>> all_advs_s1_vec;
    vector<vector<string>> all_advs_s2_vec;
    bool first_price = false;

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
            s1file.open(dir_name + "/" + n1);
            std::stringstream ss1;
            ss1 << s1file.rdbuf();
            s1_vec[i] = ss1.str();
            ifstream s2file;
            s2file.open(dir_name + "/" + n2);
            std::stringstream ss2;
            ss2 << s2file.rdbuf();
            s2_vec[i] = ss2.str();
            cout << (dir_name + "/" + n1) << " " << (dir_name + "/" + n2)
                 << endl;
        }
        in1.close();
        in2.close();
    }

    void handle(string msg, SSL* ssl) {
        bool parallel = false;
        int parallel_num = 1;
        int parallel_num_committee = 1;
        int parallel_num_sum = 1;
        bool parallel_sum = false;

        int total_bucket = 10000;
        int base_bucket = 100;
        if (total_loop == 2) {
            base_bucket = 100;
        } else if (total_loop == 4) {
            base_bucket = 10;
        } else {
            assert(0);
        }
        BUCKET_NUM = base_bucket;

        int start_bucket = 0;
        int interval = total_bucket;
        int decode_bid = 0;

        vector<vector<BIGNUM*>> shareS1;
        vector<vector<BIGNUM*>> shareS2;

        int loop_to_handle = total_loop;
        // If first price, cut the loop by half
        if (first_price) {
            cout << "you are running as first-price\n";
            loop_to_handle = loop_to_handle / 2;
        }
        cout << "loop to handle: " << loop_to_handle << endl;

        // this is simulating two servers, and so it includes the cost of
        // computing sale price
        for (int loop = 1; loop <= loop_to_handle; loop++) {
            interval = interval / base_bucket;

            BUCKET_NUM = base_bucket;

            shareS1 = vector<vector<BIGNUM*>>(ad_num);
            shareS2 = vector<vector<BIGNUM*>>(ad_num);
            // committee receive and decode share vecs
            for (int i = 0; i < ad_num; i++) {
                shareS1[i] = bn_deserialize_opt(all_advs_s1_vec[loop - 1][i]);
                shareS2[i] = bn_deserialize_opt(all_advs_s2_vec[loop - 1][i]);
            }

            // using shares to decode
            vector<BIGNUM*> sum_s1 = sumBNVec_opt(shareS1, env);
            vector<BIGNUM*> sum_s2 = sumBNVec_opt(shareS2, env);

            vector<vector<BIGNUM*>> share_input;
            share_input.emplace_back(sum_s1);
            share_input.emplace_back(sum_s2);
            vector<BIGNUM*> sum_vec_s = sumBNVec_opt(share_input, env);
            decode_bid = decode_bit_vec_opt(sum_vec_s);
            start_bucket = decode_bid * interval + start_bucket;
        }

        // start generate random and reveal shares
        int winner_id;
        vector<int> ids;
        for (int i = 0; i < ad_num; i++) {
            ids.push_back(i);
        }
        // pre-generated unbiased random seed, as it's local it's as
        // expensive as revealing all ties. Won't affect latency.
        srand(100);
        while (true) {
            int tmp_id = rand() % ids.size();
            winner_id = ids[tmp_id];
            vector<BIGNUM*> s;
            for (int i = 0; i < LAMBDA; i++) {
                BIGNUM* v = BN_new();
                env.add_mod(v, shareS1[winner_id][decode_bid * LAMBDA + i],
                            shareS2[winner_id][decode_bid * LAMBDA + i]);
                s.push_back(v);
            }
            int bit = decode_bit(s);
            if (bit == 1) {
                break;
            }
            ids.erase(ids.begin() + tmp_id);
        }

        stringstream ss;
        // dummy string as response
        ss << 10 << " " << 10;
        // return the winner and winning bid as response
        string response = ss.str();
        assert(sendMsg(ssl, response));
    }

   public:
    AddaxServer(string dir_name, string s1_filenames, string s2_filenames,
                int ad_num_, int total_loop_, bool first_price_) {
        env = Crypto();
        ad_num = ad_num_;
        total_loop = total_loop_;
        first_price = first_price_;

        // load all files
        for (int i = 0; i < total_loop; i++) {
            vector<string> s1_vec_round(ad_num);
            vector<string> s2_vec_round(ad_num);
            load_all_shares(
                dir_name + "/" + to_string(i + 1),
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
        cout << "finish load\n";
    }
};

int main(int argc, char* argv[]) {
    string ip = "0.0.0.0:6667";
    string s1_filenames = "";
    string s2_filenames = "";
    string dir_name = "";
    int ad_num = 96;
    int total_loop = 2;
    bool first_price = false;

    // fixed 1 sec
    double time_period = 1.0;
    int opt;
    while ((opt = getopt(argc, argv, "i:t:s:S:d:a:r:q")) != -1) {
        if (opt == 'i') {
            ip = string(optarg);
        } else if (opt == 't') {
            time_period = atoi(optarg);
        } else if (opt == 's') {
            s1_filenames = string(optarg);
        } else if (opt == 'S') {
            s2_filenames = string(optarg);
        } else if (opt == 'd') {
            dir_name = string(optarg);
        } else if (opt == 'a') {
            ad_num = atoi(optarg);
        } else if (opt == 'r') {
            total_loop = atoi(optarg);
        } else if (opt == 'q') {
            first_price = true;
        }
    }
    AddaxServer server = AddaxServer(dir_name, s1_filenames, s2_filenames,
                                     ad_num, total_loop, first_price);
    server.run(ip, time_period);
    return 0;
}