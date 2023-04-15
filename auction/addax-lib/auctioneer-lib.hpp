#ifndef AUCTIONEER_LIB_HPP
#define AUCTIONEER_LIB_HPP

#include <assert.h>
#include <getopt.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "addax-header.h"
#include "crypto-lib.hpp"
#include "serialization-lib.hpp"

class Committee {
   public:
    Committee(int bucket_num_, int lambda_, bool parallel_ = false,
              int parallel_num_ = 4) {
        lambda = lambda_;
        bucket_num = bucket_num_;
        shares = vector<vector<vector<BIGNUM*>>>();
        parallel = parallel_;
        parallel_num = parallel_num_;
    }

    void free() {
        for (int i = 0; i < shares.size(); i++) {
            for (int j = 0; j < shares[0].size(); j++) {
                for (int k = 0; k < shares[0][0].size(); k++) {
                    BN_free(shares[i][j][k]);
                }
            }
        }
        for (int i = 0; i < shares_opt.size(); i++) {
            for (int j = 0; j < shares_opt[i].size(); j++) {
                BN_free(shares_opt[i][j]);
            }
        }
    }

    vector<vector<BIGNUM*>> deserializeShare(string s) {
        unsigned char* str = (unsigned char*)s.c_str();
        int pos = 0;
        vector<vector<BIGNUM*>> res = vector<vector<BIGNUM*>>();
        for (int i = 0; i < bucket_num; i++) {
            res.push_back(vector<BIGNUM*>());
            for (int j = 0; j < lambda; j++) {
                int len = (int)str[pos];
                assert(len < max_bn_bytes);
                BIGNUM* tmp = BN_bin2bn(str + pos + 1, len, NULL);
                assert(tmp != NULL);
                res[i].push_back(tmp);
                pos += 1 + len;
                assert(pos <= s.size());
            }
        }
        return res;
    }

    void initShares(int ad_num) {
        shares = vector<vector<vector<BIGNUM*>>>(ad_num);
        shares_opt = vector<vector<BIGNUM*>>(ad_num);
    }

    void deserial_worker(vector<string>& s, int id) {
        for (int i = 0; i < s.size(); i++) {
            if (i % parallel_num != id) continue;
            shares[i] = deserializeShare(s[i]);
        }
    }

    void deserial_addShares_parallel(vector<string>& s) {
        vector<thread> t_vec;
        for (int i = 0; i < parallel_num; i++) {
            thread t(&Committee::deserial_worker, this, ref(s), i);
            t_vec.push_back(move(t));
        }
        for (auto& th : t_vec) {
            th.join();
        }
    }

    vector<BIGNUM*> bn_deserializeShare_opt(string s) {
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

    void deserial_worker_opt(vector<string>& s, int id) {
        for (int i = 0; i < s.size(); i++) {
            if (i % parallel_num != id) continue;
            shares_opt[i] = bn_deserializeShare_opt(s[i]);
        }
    }

    void deserial_addShares_parallel_opt(vector<string>& s) {
        vector<thread> t_vec;
        for (int i = 0; i < parallel_num; i++) {
            thread t(&Committee::deserial_worker_opt, this, ref(s), i);
            t_vec.push_back(move(t));
        }
        for (auto& th : t_vec) {
            th.join();
        }
    }

    vector<vector<vector<BIGNUM*>>> getShares() { return shares; }

    vector<vector<BIGNUM*>> getShares_opt() { return shares_opt; }

    vector<BIGNUM*> revealBitShare(int id, int bucket_id) {
        assert(id >= 0 && id < shares.size());
        return shares[id][bucket_id];
    }

    BIGNUM* revealBitShare_opt(int id, int bucket_id) {
        return shares_opt[id][bucket_id];
    }

    vector<vector<BIGNUM*>> revealAdShare(int id) {
        assert(id >= 0 && id < shares.size());
        return shares[id];
    }

    vector<BIGNUM*> revealAdShare_opt(int id) {
        assert(id >= 0 && id < shares_opt.size());
        return shares_opt[id];
    }

    void clearShares() {
        for (int i = 0; i < shares.size(); i++) {
            for (int j = 0; j < shares[i].size(); j++) {
                for (int k = 0; k < shares[i][j].size(); k++) {
                    BN_free(shares[i][j][k]);
                }
            }
        }
        shares = vector<vector<vector<BIGNUM*>>>();
    }

   private:
    int bucket_num;
    int lambda;
    int max_bn_bytes = 30;
    int max_ec_bytes = 30;
    vector<vector<vector<BIGNUM*>>> shares;
    bool parallel;
    int parallel_num;

    // 1d shares
    vector<vector<BIGNUM*>> shares_opt;
};

int decode_bit_vec(vector<vector<BIGNUM*>> bit_vec) {
    assert(bit_vec.size() == BUCKET_NUM);
    vector<int> res;
    for (int i = 0; i < BUCKET_NUM; i++) {
        assert(bit_vec[i].size() == LAMBDA);
        int bit = 0;
        // we only decode using lambda - 1 as the last one is randomness
        for (int j = 0; j < LAMBDA - 1; j++) {
            // there is one non-zero number
            if (!BN_is_zero(bit_vec[i][j])) {
                bit = 1;
                break;
            }
        }
        res.push_back(bit);
    }
    assert(res.size() == BUCKET_NUM);

    int flag = 0;
    int bucket_id = -1;
    for (int i = 0; i < res.size(); i++) {
        if (res[i] == 0 && flag == 0) {
            flag = 1;
            bucket_id = i;
            continue;
        }
        if (flag == 1) {
            if (res[i] == 1) {
                DEBUG << i << "shouldn't have 1\n";
            }
            assert(res[i] == 0);
        }
    }
    // no 0 at all, all 1s
    if (bucket_id == -1) {
        return BUCKET_NUM - 1;
    }
    // all 0s, no 1
    if (bucket_id == 0) {
        return 0;
    }
    return bucket_id - 1;
}

vector<int> decode_bit_vec_batch_opt(vector<BIGNUM*> bit_vec) {
    assert(bit_vec.size() == BUCKET_NUM * LAMBDA);
    vector<int> res;
    for (int i = 0; i < BUCKET_NUM; i++) {
        int bit = 0;
        for (int j = 0; j < LAMBDA - 1; j++) {
            // there is one non-zero num
            if (!BN_is_zero(bit_vec[i * LAMBDA + j])) {
                bit = 1;
                break;
            }
        }
        res.push_back(bit);
    }
    assert(res.size() == BUCKET_NUM);
    return res;
}

int decode_bit(vector<BIGNUM*> bit_vec) {
    for (int j = 0; j < LAMBDA - 1; j++) {
        // there is one odd num
        if (!BN_is_zero(bit_vec[j])) {
            return 1;
        }
    }
    return 0;
}

void sumBNVec_handler(vector<vector<vector<BIGNUM*>>>& input, int id,
                      int parallel_num, vector<vector<BIGNUM*>>& sum_vec) {
    Crypto env = Crypto();
    for (size_t i = 0; i < input.size(); i++) {
        assert(input[i].size() == BUCKET_NUM);
    }

    assert(input.size() >= 2);
    for (size_t i = 0; i < BUCKET_NUM; i++) {
        if (i % parallel_num != id) continue;
        assert(input[0][i].size() == LAMBDA);
        assert(input[1][i].size() == LAMBDA);
        for (size_t j = 0; j < LAMBDA; j++) {
            BIGNUM* sum_val = BN_new();
            env.add_mod(sum_val, input[0][i][j], input[1][i][j]);
            sum_vec[i][j] = (sum_val);
        }
    }

    for (size_t k = 2; k < input.size(); k++) {
        for (size_t i = 0; i < BUCKET_NUM; i++) {
            if (i % parallel_num != id) continue;
            assert(input[k][i].size() == LAMBDA);
            for (size_t j = 0; j < LAMBDA; j++) {
                env.add_mod(sum_vec[i][j], input[k][i][j], sum_vec[i][j]);
            }
        }
    }
    env.free();
}

vector<vector<BIGNUM*>> sumBNvec(vector<vector<vector<BIGNUM*>>> input,
                                 bool parallel = false, int parallel_num = 4) {
    vector<vector<BIGNUM*>> sum_vec(BUCKET_NUM, vector<BIGNUM*>(LAMBDA, NULL));
    if (parallel) {
        vector<thread> t_vec;
        for (int i = 0; i < parallel_num; i++) {
            thread t(&sumBNVec_handler, ref(input), i, parallel_num,
                     ref(sum_vec));
            t_vec.push_back(move(t));
        }
        for (int i = 0; i < t_vec.size(); i++) {
            t_vec[i].join();
        }
    } else {
        Crypto env = Crypto();
        for (size_t i = 0; i < input.size(); i++) {
            if (input[i].size() != BUCKET_NUM) {
                DEBUG << input[i].size() << " " << BUCKET_NUM << endl;
            }
            assert(input[i].size() == BUCKET_NUM);
        }

        assert(input.size() >= 2);
        for (size_t i = 0; i < BUCKET_NUM; i++) {
            assert(input[0][i].size() == LAMBDA);
            assert(input[1][i].size() == LAMBDA);
            for (size_t j = 0; j < LAMBDA; j++) {
                BIGNUM* sum_val = BN_new();
                env.add_mod(sum_val, input[0][i][j], input[1][i][j]);
                sum_vec[i][j] = (sum_val);
            }
        }

        for (size_t k = 2; k < input.size(); k++) {
            for (size_t i = 0; i < BUCKET_NUM; i++) {
                assert(input[k][i].size() == LAMBDA);
                for (size_t j = 0; j < LAMBDA; j++) {
                    env.add_mod(sum_vec[i][j], input[k][i][j], sum_vec[i][j]);
                }
            }
        }
        env.free();
    }

    return sum_vec;
}

void freeSumBNvec(vector<vector<BIGNUM*>>& input) {
    for (int i = 0; i < input.size(); i++) {
        for (int j = 0; j < input[0].size(); j++) {
            BN_free(input[i][j]);
        }
    }
}

void freeSumBNvec_opt(vector<BIGNUM*>& input) {
    for (int i = 0; i < input.size(); i++) {
        BN_free(input[i]);
    }
}

void subShare(vector<vector<BIGNUM*>> shares, vector<vector<BIGNUM*>> removed) {
    Crypto env = Crypto();
    assert(shares.size() == removed.size());
    for (int i = 0; i < shares.size(); i++) {
        assert(shares[i].size() == removed[i].size());
        for (int j = 0; j < shares[i].size(); j++) {
            env.sub_mod(shares[i][j], shares[i][j], removed[i][j]);
        }
    }
    env.free();
}

void subShare_opt(vector<BIGNUM*> shares, vector<BIGNUM*> removed,
                  Crypto& env) {
    assert(shares.size() == removed.size());
    for (int i = 0; i < shares.size(); i++) {
        env.sub_mod(shares[i], shares[i], removed[i]);
    }
}

vector<int> generate_ids(int loop_round, int total_bucket, int base_bucket,
                         int start_bucket) {
    vector<int> ids;
    int interval = total_bucket / base_bucket;
    for (int i = 1; i < loop_round; i++) {
        interval = interval / base_bucket;
    }
    int id = start_bucket;
    int times = 0;
    for (; times < base_bucket; times++, id += interval) {
        ids.push_back(id);
    }
    return ids;
}

void sumBNVec_handler_opt(vector<vector<BIGNUM*>>& input, int id,
                          int parallel_num, vector<BIGNUM*>& res) {
    Crypto env = Crypto();
    int gap = (BUCKET_NUM * LAMBDA / parallel_num);
    if (BUCKET_NUM * LAMBDA % parallel_num != 0) {
        gap += 1;
    }
    for (size_t i = id * gap;
         ((i < (id + 1) * gap) && (i < (BUCKET_NUM * LAMBDA))); i++) {
        assert(i < BUCKET_NUM * LAMBDA);
        res[i] = BN_new();
        if (input.size() > 2)
            BN_add(res[i], input[0][i], input[1][i]);
        else {
            env.add_mod(res[i], input[0][i], input[1][i]);
        }
    }
    if (input.size() == 2) return;
    for (size_t i = 2; i < input.size() - 1; i++) {
        for (size_t j = id * gap;
             ((j < (id + 1) * gap) && (j < (BUCKET_NUM * LAMBDA))); j++) {
            assert(j < BUCKET_NUM * LAMBDA);

            BN_add(res[j], input[i][j], res[j]);
        }
    }
    for (size_t i = id * gap;
         ((i < (id + 1) * gap) && (i < (BUCKET_NUM * LAMBDA))); i++) {
        assert(i < BUCKET_NUM * LAMBDA);

        env.add_mod(res[i], res[i], input[input.size() - 1][i]);
    }
    env.free();
}

vector<BIGNUM*> sumBNVec_opt(vector<vector<BIGNUM*>>& input, Crypto& env,
                             bool parallel = false, int parallel_num = 8) {
    vector<BIGNUM*> res(BUCKET_NUM * LAMBDA);
    if (parallel) {
        // a naive check just for simplicity...
        // assert((BUCKET_NUM * LAMBDA) % parallel_num == 0);
        vector<thread> t_vec;
        for (int i = 0; i < parallel_num; i++) {
            thread t(&sumBNVec_handler_opt, ref(input), i, parallel_num,
                     ref(res));
            t_vec.push_back(move(t));
        }
        for (int i = 0; i < t_vec.size(); i++) {
            t_vec[i].join();
        }
    } else {
        for (size_t i = 0; i < BUCKET_NUM * LAMBDA; i++) {
            res[i] = BN_new();
            if (input.size() > 2)
                BN_add(res[i], input[0][i], input[1][i]);
            else {
                env.add_mod(res[i], input[0][i], input[1][i]);
            }
        }
        if (input.size() == 2) return res;
        for (size_t i = 2; i < input.size() - 1; i++) {
            for (size_t j = 0; j < BUCKET_NUM * LAMBDA; j++) {
                BN_add(res[j], input[i][j], res[j]);
            }
        }
        for (size_t i = 0; i < BUCKET_NUM * LAMBDA; i++) {
            env.add_mod(res[i], res[i], input[input.size() - 1][i]);
        }
    }
    return res;
}

// 1d arrary decode
int decode_bit_vec_opt(vector<BIGNUM*> bit_vec) {
    assert(bit_vec.size() == BUCKET_NUM * LAMBDA);
    vector<int> res;
    for (int i = 0; i < BUCKET_NUM; i++) {
        int bit = 0;
        // we only decode using lambda - 1 as the last one is randomness
        for (int j = 0; j < LAMBDA - 1; j++) {
            // there is one non-zero number
            if (!BN_is_zero(bit_vec[i * LAMBDA + j])) {
                bit = 1;
                break;
            }
        }
        res.push_back(bit);
    }
    assert(res.size() == BUCKET_NUM);

    int flag = 0;
    int bucket_id = -1;
    for (int i = 0; i < res.size(); i++) {
        if (res[i] == 0 && flag == 0) {
            flag = 1;
            bucket_id = i;
            continue;
        }
        if (flag == 1) {
            if (res[i] == 1) {
                DEBUG << i << "shouldn't have 1\n";
            }
            assert(res[i] == 0);
        }
    }
    // no 0 at all, all 1s
    if (bucket_id == -1) {
        return BUCKET_NUM - 1;
    }
    // all 0s, no 1
    if (bucket_id == 0) {
        return 0;
    }
    return bucket_id - 1;
}

#endif  // AUCTIONEER_LIB_HPP
