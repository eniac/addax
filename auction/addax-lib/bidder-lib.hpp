#ifndef BIDDER_LIB_HPP
#define BIDDER_LIB_HPP

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

class Advertiser {
   public:
    vector<vector<BIGNUM*>> encode_and_split_bit(int bit, int bit_len,
                                                 int order_bit_len,
                                                 Crypto& env) {
        BIGNUM* zero = BN_new();
        BN_zero(zero);
        assert(bit == 0 || bit == 1);
        vector<vector<BIGNUM*>> res = vector<vector<BIGNUM*>>();
        for (int i = 0; i < 3; i++) {
            res.push_back(vector<BIGNUM*>());
        }

        for (int i = 0; i < lambda; i++) {
            BIGNUM* s1 = BN_new();
            BN_rand_range(s1, env.getOrder());
            BIGNUM* s2 = BN_new();
            BN_rand_range(s2, env.getOrder());
            BIGNUM* sum = BN_new();
            env.add_mod(sum, s1, s2);
            // note: lambda bit is randomness, should be purely random
            if (bit == 0 && i < lambda - 1) {
                env.sub_mod(s2, zero, s1);
                env.add_mod(sum, s1, s2);
                assert(BN_is_zero(sum));
            }
            assert(BN_num_bits(sum) <= order_bit_len);
            assert(BN_is_negative(s2) == 0);
            assert(BN_is_negative(s1) == 0);
            assert(BN_is_negative(sum) == 0);
            res[0].push_back(sum);
            res[1].push_back(s1);
            res[2].push_back(s2);
        }

        return res;
    }

    vector<vector<vector<BIGNUM*>>> generate_and_split_bid_vec(
        int bid, int bit_len, int order_bit_len, Crypto& env) {
        vector<vector<vector<BIGNUM*>>> bid_vec =
            vector<vector<vector<BIGNUM*>>>();
        for (int i = 0; i < bucket_num; i++) {
            // means 1
            if (i <= bid) {
                vector<vector<BIGNUM*>> tmp_vec =
                    encode_and_split_bit(1, bit_len, order_bit_len, env);
                bid_vec.push_back(tmp_vec);
            }
            // 0
            else {
                vector<vector<BIGNUM*>> tmp_vec =
                    encode_and_split_bit(0, bit_len, order_bit_len, env);
                bid_vec.push_back(tmp_vec);
            }
        }
        return bid_vec;
    }

    Advertiser(int b, int bucket_, int lambda_, Crypto& env,
               bool parallel = false, int parallel_num = 4,
               bool commit_gen = true, bool empty = false,
               bool interactive = false) {
        struct timeval starttime, endtime;
        gettimeofday(&starttime, 0);
        bid = b;
        bit_len = env.bitLen();
        order_bit_len = env.orderBitLen();
        bucket_num = bucket_;
        lambda = lambda_;
        if (empty) return;

        bid_vec = vector<vector<BIGNUM*>>();
        s1_vec = vector<vector<BIGNUM*>>();
        s2_vec = vector<vector<BIGNUM*>>();
        vector<vector<vector<BIGNUM*>>> bid_vec_all =
            generate_and_split_bid_vec(bid, bit_len, order_bit_len, env);
        for (int i = 0; i < bucket_num; i++) {
            bid_vec.push_back(bid_vec_all[i][0]);
            s1_vec.push_back(bid_vec_all[i][1]);
            s2_vec.push_back(bid_vec_all[i][2]);
        }
        gettimeofday(&endtime, 0);
        DEBUG << "Ad encode: "
              << ((double)(endtime.tv_sec - starttime.tv_sec) +
                  (double)(endtime.tv_usec - starttime.tv_usec) / 1000000)
              << endl;

        gettimeofday(&starttime, 0);
        s1_vec_str = serializeShareVec(s1_vec, interactive, s1_share_str_vec);
        s2_vec_str = serializeShareVec(s2_vec, interactive, s2_share_str_vec);
        gettimeofday(&endtime, 0);
        DEBUG << "Ad serialize shares: "
              << ((double)(endtime.tv_sec - starttime.tv_sec) +
                  (double)(endtime.tv_usec - starttime.tv_usec) / 1000000)
              << endl;

        if (!commit_gen) return;

        // assume generators is not empty!!! for verification, call
        // load_generators first

        gettimeofday(&starttime, 0);
        if (parallel) {
            vector<EC_POINT*> tmp_s(bucket_num, NULL);
            bid_commitment = tmp_s;
            tmp_s.clear();

            randMask_bid_commitment = vector<EC_POINT*>(bucket_num, NULL);
            randMask_bid_vec = vector<vector<BIGNUM*>>();
            for (int i = 0; i < bucket_num; i++) {
                randMask_bid_vec.push_back(vector<BIGNUM*>(lambda, NULL));
            }

            zkp_u = vector<EC_POINT*>(bucket_num, NULL);
            zkp_v = vector<BIGNUM*>(bucket_num, NULL);
            zkp_z = vector<BIGNUM*>(bucket_num, NULL);

            vector<thread> t_vec;
            vector<Crypto> envs;
            for (int i = 0; i < parallel_num; i++) {
                envs.push_back(Crypto());
            }
            for (int i = 0; i < parallel_num; i++) {
                thread t(&Advertiser::commit_parallel_single, this, i,
                         parallel_num, ref(bid_vec), ref(envs[i]));
                t_vec.push_back(move(t));
            }

            for (auto& th : t_vec) {
                th.join();
            }
            for (int i = 0; i < envs.size(); i++) {
                envs[i].free();
            }
        } else {
            cout << "must run in parallel\n";
            assert(0);
        }
        gettimeofday(&endtime, 0);
        DEBUG << "Ad commit: "
              << ((double)(endtime.tv_sec - starttime.tv_sec) +
                  (double)(endtime.tv_usec - starttime.tv_usec) / 1000000)
              << endl;

        gettimeofday(&starttime, 0);
        // serialize commitments and shares
        total_commitment_str = serializeCommitment(bid_commitment, env);

        total_bid_commitment = vector<vector<EC_POINT*>>();
        gettimeofday(&endtime, 0);
        DEBUG << "Ad serialize commitment: "
              << ((double)(endtime.tv_sec - starttime.tv_sec) +
                  (double)(endtime.tv_usec - starttime.tv_usec) / 1000000)
              << endl;

        // serialize r*M, C(r*M), zkp
        vector<string> place_holder;
        gettimeofday(&starttime, 0);
        randMask_bid_str =
            serializeShareVec(randMask_bid_vec, interactive, place_holder);
        gettimeofday(&endtime, 0);
        DEBUG << "Ad serialize randmask * bid vec: "
              << ((double)(endtime.tv_sec - starttime.tv_sec) +
                  (double)(endtime.tv_usec - starttime.tv_usec) / 1000000)
              << endl;

        gettimeofday(&starttime, 0);
        randMask_commitment_str =
            serializeCommitment(randMask_bid_commitment, env);
        gettimeofday(&endtime, 0);
        DEBUG << "Ad serialize randmask * bid vec commitment: "
              << ((double)(endtime.tv_sec - starttime.tv_sec) +
                  (double)(endtime.tv_usec - starttime.tv_usec) / 1000000)
              << endl;

        gettimeofday(&starttime, 0);
        zkp_u_str = serializeCommitment(zkp_u, env);
        gettimeofday(&endtime, 0);
        DEBUG << "Ad serialize zkp_u: "
              << ((double)(endtime.tv_sec - starttime.tv_sec) +
                  (double)(endtime.tv_usec - starttime.tv_usec) / 1000000)
              << endl;

        gettimeofday(&starttime, 0);
        zkp_v_str = serializeZKP(zkp_v);
        gettimeofday(&endtime, 0);
        DEBUG << "Ad serialize zkp_v: "
              << ((double)(endtime.tv_sec - starttime.tv_sec) +
                  (double)(endtime.tv_usec - starttime.tv_usec) / 1000000)
              << endl;

        gettimeofday(&starttime, 0);
        zkp_z_str = serializeZKP(zkp_z);
        gettimeofday(&endtime, 0);
        DEBUG << "Ad serialize zkp_z: "
              << ((double)(endtime.tv_sec - starttime.tv_sec) +
                  (double)(endtime.tv_usec - starttime.tv_usec) / 1000000)
              << endl;
    }

    void free() {
        // free total_bid_commitment
        for (int i = 0; i < total_bid_commitment.size(); i++) {
            for (int j = 0; j < total_bid_commitment[i].size(); j++) {
                EC_POINT_free(total_bid_commitment[i][j]);
            }
        }
    }

    vector<vector<BIGNUM*>> getBidVec() { return bid_vec; }

    vector<vector<BIGNUM*>> getS1() { return s1_vec; }

    vector<vector<BIGNUM*>> getS2() { return s2_vec; }

    string getS1_str_by_vec(vector<int>& ids) {
        string res;
        for (int i = 0; i < ids.size(); i++) {
            res.append(s1_share_str_vec[ids[i]]);
        }
        return res;
    }

    string getS2_str_by_vec(vector<int>& ids) {
        string res;
        for (int i = 0; i < ids.size(); i++) {
            res.append(s2_share_str_vec[ids[i]]);
        }
        return res;
    }

    vector<vector<EC_POINT*>> getS1_commitment() { return s1_commitment; }

    vector<vector<EC_POINT*>> getS2_commitment() { return s2_commitment; }

    string getS1_str() { return s1_vec_str; }

    string getS2_str() { return s2_vec_str; }

    string getS1_commitment_str() { return s1_commitment_str; }

    string getS2_commitment_str() { return s2_commitment_str; }

    string getBid_commitment_str() { return total_commitment_str; }

    vector<vector<EC_POINT*>> getTotal_bid_commitment() {
        return total_bid_commitment;
    }

    string getRandMask_bid_commitment_str() { return randMask_commitment_str; }

    string getRandMask_bid_str() { return randMask_bid_str; }

    string getZKP_u_str() { return zkp_u_str; }

    string getZKP_v_str() { return zkp_v_str; }

    string getZKP_z_str() { return zkp_z_str; }

    void init_commitments(int ad_num) {
        vector<EC_POINT*> tmp = vector<EC_POINT*>();
        for (int i = 0; i < ad_num; i++) {
            total_bid_commitment.push_back(tmp);
        }
    }

    void deserial_addCommitment_bid(string s, Crypto env, int id) {
        total_bid_commitment[id] = deserializeCommitment(s, env);
    }

    void deserialize_parallel_single(vector<string>& com_strs,
                                     int parallel_num) {
        vector<thread> t_vec;
        for (int i = 0; i < parallel_num; i++) {
            thread t(&Advertiser::deserialize_parallel_handler_single, this,
                     ref(com_strs), i, parallel_num);
            t_vec.push_back(move(t));
        }

        for (auto& th : t_vec) {
            th.join();
        }
    }

   private:
    int bid;
    int bit_len;
    int order_bit_len;
    int bucket_num;
    int lambda;
    int max_bn_bytes = 30;
    int max_ec_bytes = 30;
    vector<vector<BIGNUM*>> bid_vec;
    vector<vector<BIGNUM*>> s1_vec;
    vector<vector<BIGNUM*>> s2_vec;
    vector<vector<EC_POINT*>> s1_commitment;
    vector<vector<EC_POINT*>> s2_commitment;
    vector<EC_POINT*> bid_commitment;
    vector<vector<EC_POINT*>> total_bid_commitment;

    // r*bid_vec
    vector<vector<BIGNUM*>> randMask_bid_vec;
    // commitment of r*bid_vec
    vector<EC_POINT*> randMask_bid_commitment;
    // zero knowledge proofs
    vector<EC_POINT*> zkp_u;
    vector<BIGNUM*> zkp_v;
    vector<BIGNUM*> zkp_z;
    string randMask_bid_str;
    string randMask_commitment_str;
    string zkp_u_str;
    string zkp_v_str;
    string zkp_z_str;

    string s1_vec_str;
    string s2_vec_str;
    string s1_commitment_str;
    string s2_commitment_str;
    string total_commitment_str;

    // interactive protocol
    vector<string> s1_share_str_vec;
    vector<string> s2_share_str_vec;
    vector<string> s1_commitment_str_vec;
    vector<string> s2_commitment_str_vec;

    void deserialize_parallel_handler_single(vector<string>& s1_com_strs,
                                             int id, int total) {
        Crypto env = Crypto();
        int len = s1_com_strs.size();
        for (int i = 0; i < len; i++) {
            if (i % total != id) continue;
            deserial_addCommitment_bid(s1_com_strs[i], env, i);
        }
        env.free();
    }

    void commit_parallel(int id, int total, vector<vector<BIGNUM*>>& s1_vec,
                         vector<vector<BIGNUM*>>& s2_vec, Crypto& env) {
        for (int i = 0; i < bucket_num; i++) {
            if (i % total != id) continue;
            for (int j = 0; j < lambda; j++) {
                // cout << "T" << id << " " << i << " " << j << endl;
                s1_commitment[i][j] = (env.genCommitment(s1_vec[i][j]));
                s2_commitment[i][j] = (env.genCommitment(s2_vec[i][j]));
            }
        }
    }

    void commit_parallel_single(int id, int total,
                                vector<vector<BIGNUM*>>& bid_vec, Crypto& env) {
        for (int i = 0; i < bucket_num; i++) {
            if (i % total != id) continue;
            bid_commitment[i] = env.genCommitment_batch(bid_vec[i]);

            // generate r*M
            BIGNUM* r = env.randNum();
            for (int j = 0; j < lambda; j++) {
                randMask_bid_vec[i][j] = BN_new();
                env.mul_mod(randMask_bid_vec[i][j], r, bid_vec[i][j]);
            }

            // commit r*M
            randMask_bid_commitment[i] =
                env.genCommitment_batch(randMask_bid_vec[i]);

            // generate zkp
            BIGNUM* rd = env.randNum();
            zkp_u[i] = env.newEcPoint();
            env.mulEcPoint(bid_commitment[i], rd, zkp_u[i]);
            string h_str = env.serializeEC(bid_commitment[i]);
            h_str.append(env.serializeEC(randMask_bid_commitment[i]));
            h_str.append(env.serializeEC(zkp_u[i]));
            string v_str = sha256(h_str);
            zkp_v[i] =
                BN_bin2bn((unsigned char*)v_str.c_str(), v_str.size(), NULL);
            env.mod(zkp_v[i]);
            zkp_z[i] = BN_new();
            env.mul_mod(zkp_z[i], zkp_v[i], r);
            env.add_mod(zkp_z[i], zkp_z[i], rd);
        }
    }

    string serializeShareVec(vector<vector<BIGNUM*>> share, bool interactive,
                             vector<string>& share_str_vec) {
        string res;
        unsigned char tmp[max_bn_bytes];
        system_clock::time_point start, end;
        double concatenate = 0.0;
        string conca_str;
        for (size_t i = 0; i < share.size(); i++) {
            assert(share.size() == bucket_num);
            string tmp_res = "";
            for (size_t j = 0; j < share[i].size(); j++) {
                assert(share[i].size() == lambda);
                memset(tmp, 0, max_bn_bytes);
                int byte_len = BN_num_bytes(share[i][j]);
                if (byte_len >= max_bn_bytes) {
                    DEBUG << "bn byte len " << i << " " << j
                          << " is larger than expected max bytes\n";
                }
                assert(byte_len < max_bn_bytes);
                tmp[0] = byte_len;
                int bn_len = BN_bn2bin(share[i][j], tmp + 1);
                if (bn_len != byte_len) {
                    DEBUG << "bn2bin: " << bn_len
                          << " and num bytes: " << byte_len
                          << " inconsistent\n";
                }
                assert(bn_len == byte_len);
                string tmp_str = string((const char*)tmp, byte_len + 1);
                res.append(tmp_str);
                if (interactive) tmp_res.append(tmp_str);
            }
            if (interactive) {
                start = system_clock::now();
                share_str_vec.push_back(tmp_res);
                conca_str.append(tmp_res);
                end = system_clock::now();
                concatenate +=
                    duration_cast<std::chrono::duration<double>>(end - start)
                        .count();
            }
        }
        DEBUG << "shares concatenate time: " << concatenate << endl;
        return res;
    }

    string serializeCommitment(vector<EC_POINT*> share, Crypto& env) {
        string res;
        system_clock::time_point start, end;
        double concatenate = 0.0;
        unsigned char tmp[max_ec_bytes];
        for (size_t i = 0; i < share.size(); i++) {
            assert(share.size() == bucket_num);
            memset(tmp, 0, max_ec_bytes);
            int byte_len = env.EC2buf(share[i], tmp + 1, max_ec_bytes - 1);
            tmp[0] = byte_len;
            string tmp_str = string((const char*)tmp, byte_len + 1);
            start = system_clock::now();
            res.append(tmp_str);
            end = system_clock::now();
            concatenate +=
                duration_cast<std::chrono::duration<double>>(end - start)
                    .count();
        }
        DEBUG << "commitments concatenate time: " << concatenate << endl;
        return res;
    }

   public:
    vector<EC_POINT*> deserializeCommitment(string s, Crypto& env) {
        unsigned char* str = (unsigned char*)s.c_str();
        int pos = 0;
        vector<EC_POINT*> res = vector<EC_POINT*>();
        for (int i = 0; i < bucket_num; i++) {
            int len = (int)str[pos];
            assert(len < max_ec_bytes);
            EC_POINT* tmp = env.buf2point(str + pos + 1, len);
            assert(tmp != NULL);
            res.push_back(tmp);
            pos += 1 + len;
            assert(pos <= s.size());
        }
        return res;
    }
};

#endif  // BIDDER_LIB_HPP
