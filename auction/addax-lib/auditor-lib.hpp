#ifndef AUDITOR_LIB_HPP
#define AUDITOR_LIB_HPP

#include <future>
#include <thread>

#include "crypto-lib.hpp"

using namespace std;
using namespace chrono;

void verifySum_handler(vector<vector<EC_POINT*>>& c_vec,
                       vector<vector<BIGNUM*>>& sum_vec,
                       vector<vector<BIGNUM*>>& removed_sum_vec, Crypto& env,
                       int id, int total, promise<bool>& promiseObj,
                       bool interactive) {
    struct timeval starttime, endtime;
    gettimeofday(&starttime, 0);
    assert(sum_vec.size() == BUCKET_NUM);
    assert(c_vec.size() >= 2);
    vector<EC_POINT*> new_sum_vec(BUCKET_NUM, NULL);
    for (size_t i = 0; i < BUCKET_NUM; i++) {
        if (i % total != id) continue;
        assert(sum_vec[i].size() == LAMBDA);
        // check sum
        EC_POINT* sum = env.newEcPoint();
        EC_POINT_copy(sum, c_vec[1][i]);
        // env.addEcPoint(c_vec[0][i][j], c_vec[1][i][j], sum);
        for (size_t k = 2; k < c_vec.size(); k++) {
            env.addEcPoint(c_vec[k][i], sum, sum);
        }
        new_sum_vec[i] = sum;
    }
    gettimeofday(&endtime, 0);
    // DEBUG << "TIME: sum up (commitments): " << ((double)(endtime.tv_sec -
    // starttime.tv_sec) + (double) (endtime.tv_usec - starttime.tv_usec) /
    // 1000000) << endl;

    gettimeofday(&starttime, 0);
    for (size_t i = 0; i < BUCKET_NUM; i++) {
        if (i % total != id) continue;
        bool eq2 = true;
        if (!interactive) {
            EC_POINT* sum_p_2 = env.genCommitment_batch(removed_sum_vec[i]);
            eq2 = env.eqEcPoint(new_sum_vec[i], sum_p_2);
            env.freeEcPoint(sum_p_2);
        }
        env.addEcPoint(new_sum_vec[i], c_vec[0][i], new_sum_vec[i]);
        EC_POINT* sum_p = env.genCommitment_batch(sum_vec[i]);
        bool eq = env.eqEcPoint(new_sum_vec[i], sum_p);
        if (eq != true || eq2 != true) {
            DEBUG << i << " commitment mismatch\n";
            promiseObj.set_value(false);
            return;
        }
        env.freeEcPoint(new_sum_vec[i]);
        env.freeEcPoint(sum_p);
    }
    gettimeofday(&endtime, 0);
    // DEBUG << "TIME: verify (commitments): " << ((double)(endtime.tv_sec -
    // starttime.tv_sec) + (double) (endtime.tv_usec - starttime.tv_usec) /
    // 1000000) << endl;
    promiseObj.set_value(true);
    return;
}

bool verifySum(vector<vector<EC_POINT*>> c_vec, vector<vector<BIGNUM*>> sum_vec,
               vector<vector<BIGNUM*>>& removed_sum_vec, Crypto& env,
               bool parallel = false, int parallel_num = 4,
               bool interactive = false) {
    if (parallel) {
        vector<thread> t_vec;
        vector<Crypto> envs;
        vector<promise<bool>> res_vec;
        for (int i = 0; i < parallel_num; i++) {
            envs.push_back(Crypto());
            res_vec.push_back(move(promise<bool>()));
        }
        for (int i = 0; i < parallel_num; i++) {
            thread t(&verifySum_handler, ref(c_vec), ref(sum_vec),
                     ref(removed_sum_vec), ref(envs[i]), i, parallel_num,
                     ref(res_vec[i]), interactive);
            t_vec.push_back(move(t));
        }
        for (int i = 0; i < t_vec.size(); i++) {
            t_vec[i].join();
            bool res = res_vec[i].get_future().get();
            if (res == false) return false;
            envs[i].free();
        }
        return true;
    }

    // must be parallel!!
    assert(0);
    return true;
}

// assume by default 0 bit to verify
void verify_bit_handler(vector<vector<BIGNUM*>>& sum_bit,
                        vector<vector<EC_POINT*>>& bit_commitments, int id,
                        int total, promise<bool>& promiseObj, int verify_bit) {
    Crypto env = Crypto();
    for (size_t i = 0; i < sum_bit.size(); i++) {
        if (i % total != id) continue;
        EC_POINT* sum_p = env.genCommitment_batch(sum_bit[i]);
        if (!env.eqEcPoint(bit_commitments[i][verify_bit], sum_p)) {
            promiseObj.set_value(false);
            return;
        }
        env.freeEcPoint(sum_p);
    }
    promiseObj.set_value(true);
    return;
}

bool verify_bit(vector<vector<BIGNUM*>>& sum_bit,
                vector<vector<EC_POINT*>>& bit_commitments,
                int parallel_num = 8, int verify_bit = 0) {
    vector<thread> t_vec;
    vector<promise<bool>> res_vec;
    for (int i = 0; i < parallel_num; i++) {
        res_vec.push_back(move(promise<bool>()));
    }
    for (int i = 0; i < parallel_num; i++) {
        thread t(&verify_bit_handler, ref(sum_bit), ref(bit_commitments), i,
                 parallel_num, ref(res_vec[i]), verify_bit);
        t_vec.push_back(move(t));
    }
    for (int i = 0; i < t_vec.size(); i++) {
        t_vec[i].join();
        bool res = res_vec[i].get_future().get();
        if (res == false) return false;
    }
    return true;
}

void verify_zkp_handler(vector<EC_POINT*>& bid_commitment,
                        vector<EC_POINT*>& randMask_bid_commitment,
                        vector<EC_POINT*>& zkp_u, vector<BIGNUM*>& zkp_v,
                        vector<BIGNUM*>& zkp_z, int bucket_num,
                        promise<bool>& promiseObj, int id, int parallel_num) {
    Crypto env = Crypto();
    for (int i = 0; i < bucket_num; i++) {
        if (i % parallel_num != id) continue;

        // check hash value
        string h_str = env.serializeEC(bid_commitment[i]);
        h_str.append(env.serializeEC(randMask_bid_commitment[i]));
        h_str.append(env.serializeEC(zkp_u[i]));
        string v_str = sha256(h_str);
        BIGNUM* v =
            BN_bin2bn((unsigned char*)v_str.c_str(), v_str.size(), NULL);
        env.mod(v);
        if (BN_cmp(v, zkp_v[i]) != 0) {
            promiseObj.set_value(false);
            return;
        }

        // check proofs
        EC_POINT* cz = env.newEcPoint();
        env.mulEcPoint(bid_commitment[i], zkp_z[i], cz);
        EC_POINT* uzv = env.newEcPoint();
        env.mulEcPoint(randMask_bid_commitment[i], zkp_v[i], uzv);
        env.addEcPoint(zkp_u[i], uzv, uzv);
        if (!env.eqEcPoint(cz, uzv)) {
            promiseObj.set_value(false);
            return;
        }
    }
    promiseObj.set_value(true);
    return;
}

bool verify_zkp(vector<EC_POINT*>& bid_commitment,
                vector<EC_POINT*>& randMask_bid_commitment,
                vector<EC_POINT*>& zkp_u, vector<BIGNUM*>& zkp_v,
                vector<BIGNUM*>& zkp_z, int bucket_num, int parallel_num = 8) {
    vector<thread> t_vec;
    vector<promise<bool>> res_vec;
    for (int i = 0; i < parallel_num; i++) {
        res_vec.push_back(move(promise<bool>()));
    }
    for (int i = 0; i < parallel_num; i++) {
        thread t(&verify_zkp_handler, ref(bid_commitment),
                 ref(randMask_bid_commitment), ref(zkp_u), ref(zkp_v),
                 ref(zkp_z), bucket_num, ref(res_vec[i]), i, parallel_num);
        t_vec.push_back(move(t));
    }
    for (int i = 0; i < t_vec.size(); i++) {
        t_vec[i].join();
        bool res = res_vec[i].get_future().get();
        if (res == false) return false;
    }
    return true;
}

#endif  // AUDITOR_LIB_HPP
