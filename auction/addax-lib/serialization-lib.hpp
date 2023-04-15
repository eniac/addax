#ifndef SERIALIZATION_LIB_HPP
#define SERIALIZATION_LIB_HPP

#include <assert.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "addax-header.h"
#include "crypto-lib.hpp"

using namespace std;
using namespace chrono;

string serializeBit(vector<BIGNUM*>& bit_encodings) {
    int max_bn_bytes = 30;
    string res;
    unsigned char tmp[max_bn_bytes];
    assert(bit_encodings.size() == LAMBDA);
    for (size_t j = 0; j < bit_encodings.size(); j++) {
        memset(tmp, 0, max_bn_bytes);
        int byte_len = BN_num_bytes(bit_encodings[j]);
        if (byte_len >= max_bn_bytes) {
            DEBUG << "bn byte len " << j << " " << j
                  << " is larger than expected max bytes\n";
        }
        assert(byte_len < max_bn_bytes);
        tmp[0] = byte_len;
        int bn_len = BN_bn2bin(bit_encodings[j], tmp + 1);
        if (bn_len != byte_len) {
            DEBUG << "bn2bin: " << bn_len << " and num bytes: " << byte_len
                  << " inconsistent\n";
        }
        assert(bn_len == byte_len);
        string tmp_str = string((const char*)tmp, byte_len + 1);
        res.append(tmp_str);
    }
    return res;
}

vector<BIGNUM*> deserializeBit(string& bit_share) {
    vector<BIGNUM*> res;
    unsigned char* str = (unsigned char*)bit_share.c_str();
    int max_bn_bytes = 30;
    int pos = 0;
    for (int j = 0; j < LAMBDA; j++) {
        int len = (int)str[pos];
        assert(len < max_bn_bytes);
        BIGNUM* tmp = BN_bin2bn(str + pos + 1, len, NULL);
        assert(tmp != NULL);
        res.push_back(tmp);
        pos += 1 + len;
        assert(pos <= bit_share.size());
    }
    return res;
}

string serializeZKP(vector<BIGNUM*>& bit_encodings) {
    int max_bn_bytes = 30;
    string res;
    unsigned char tmp[max_bn_bytes];
    assert(bit_encodings.size() == BUCKET_NUM);
    double concatenate = 0.0;
    system_clock::time_point start, end;
    for (size_t j = 0; j < bit_encodings.size(); j++) {
        memset(tmp, 0, max_bn_bytes);
        int byte_len = BN_num_bytes(bit_encodings[j]);
        if (byte_len >= max_bn_bytes) {
            DEBUG << "bn byte len " << j << " " << byte_len
                  << " is larger than expected max bytes\n";
        }
        assert(byte_len < max_bn_bytes);
        tmp[0] = byte_len;
        int bn_len = BN_bn2bin(bit_encodings[j], tmp + 1);
        if (bn_len != byte_len) {
            DEBUG << "bn2bin: " << bn_len << " and num bytes: " << byte_len
                  << " inconsistent\n";
        }
        assert(bn_len == byte_len);
        start = system_clock::now();
        string tmp_str = string((const char*)tmp, byte_len + 1);
        res.append(tmp_str);
        end = system_clock::now();
        concatenate +=
            duration_cast<std::chrono::duration<double>>(end - start).count();
    }
    DEBUG << "zkp concatenate time: " << concatenate << endl;
    return res;
}

vector<BIGNUM*> deserializeZKP(string& bit_share) {
    vector<BIGNUM*> res;
    unsigned char* str = (unsigned char*)bit_share.c_str();
    int max_bn_bytes = 30;
    int pos = 0;
    for (int j = 0; j < BUCKET_NUM; j++) {
        int len = (int)str[pos];
        assert(len < max_bn_bytes);
        BIGNUM* tmp = BN_bin2bn(str + pos + 1, len, NULL);
        assert(tmp != NULL);
        res.push_back(tmp);
        pos += 1 + len;
        assert(pos <= bit_share.size());
    }
    return res;
}

string serializeShareVec_out(vector<vector<BIGNUM*>> share) {
    string res;
    int max_bn_bytes = 30;
    unsigned char tmp[max_bn_bytes];
    for (size_t i = 0; i < share.size(); i++) {
        for (size_t j = 0; j < share[i].size(); j++) {
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
                DEBUG << "bn2bin: " << bn_len << " and num bytes: " << byte_len
                      << " inconsistent\n";
            }
            assert(bn_len == byte_len);
            string tmp_str = string((const char*)tmp, byte_len + 1);
            res.append(tmp_str);
        }
    }
    return res;
}

string serializeShareVec_opt(vector<BIGNUM*> share) {
    string res;
    int max_bn_bytes = 30;
    unsigned char tmp[max_bn_bytes];
    for (size_t i = 0; i < share.size(); i++) {
        memset(tmp, 0, max_bn_bytes);
        int byte_len = BN_num_bytes(share[i]);
        if (byte_len >= max_bn_bytes) {
            DEBUG << "bn byte len " << i
                  << " is larger than expected max bytes\n";
        }
        assert(byte_len < max_bn_bytes);
        tmp[0] = byte_len;
        int bn_len = BN_bn2bin(share[i], tmp + 1);
        if (bn_len != byte_len) {
            DEBUG << "bn2bin: " << bn_len << " and num bytes: " << byte_len
                  << " inconsistent\n";
        }
        assert(bn_len == byte_len);
        string tmp_str = string((const char*)tmp, byte_len + 1);
        res.append(tmp_str);
    }
    return res;
}

vector<vector<BIGNUM*>> deserializeShare_out(string s) {
    unsigned char* str = (unsigned char*)s.c_str();
    int max_bn_bytes = 30;
    int pos = 0;
    vector<vector<BIGNUM*>> res = vector<vector<BIGNUM*>>();
    for (int i = 0; i < BUCKET_NUM; i++) {
        res.push_back(vector<BIGNUM*>());
        for (int j = 0; j < LAMBDA; j++) {
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

void deserializeShare_bit_handler(vector<string>& s1_vec,
                                  vector<string>& s2_vec, int id, int total_num,
                                  vector<vector<BIGNUM*>>& sum_bit_vec) {
    Crypto env = Crypto();
    for (int i = 0; i < s1_vec.size(); i++) {
        if (i % total_num != id) continue;
        unsigned char* str = (unsigned char*)s1_vec[i].c_str();
        int max_bn_bytes = 30;
        int pos = 0;
        for (int j = 0; j < LAMBDA; j++) {
            int len = (int)str[pos];
            assert(len < max_bn_bytes);
            BIGNUM* tmp = BN_bin2bn(str + pos + 1, len, NULL);
            assert(tmp != NULL);
            sum_bit_vec[i][j] = (tmp);
            pos += 1 + len;
            assert(pos <= s1_vec[i].size());
        }

        // start deserialize s2
        str = (unsigned char*)s2_vec[i].c_str();
        pos = 0;
        for (int j = 0; j < LAMBDA; j++) {
            int len = (int)str[pos];
            assert(len < max_bn_bytes);
            BIGNUM* tmp = BN_bin2bn(str + pos + 1, len, NULL);
            assert(tmp != NULL);
            env.add_mod(sum_bit_vec[i][j], tmp, sum_bit_vec[i][j]);
            pos += 1 + len;
            assert(pos <= s2_vec[i].size());
            BN_free(tmp);
        }
    }
    env.free();
}

vector<vector<BIGNUM*>> deserializeShare_bit(vector<string>& s1_vec,
                                             vector<string>& s2_vec,
                                             bool parallel = true,
                                             int parallel_num = 8) {
    assert(s1_vec.size() == s2_vec.size());
    vector<vector<BIGNUM*>> sum_bit_vec(s1_vec.size(),
                                        vector<BIGNUM*>(LAMBDA, NULL));
    vector<thread> t_vec;
    for (int i = 0; i < parallel_num; i++) {
        thread t(&deserializeShare_bit_handler, ref(s1_vec), ref(s2_vec), i,
                 parallel_num, ref(sum_bit_vec));
        t_vec.push_back(move(t));
    }
    for (int i = 0; i < t_vec.size(); i++) {
        t_vec[i].join();
    }
    return sum_bit_vec;
}

#endif  // SERIALIZATION_LIB_HPP
