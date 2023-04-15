#ifndef CRYPTO_LIB_HPP
#define CRYPTO_LIB_HPP

#include <assert.h>
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

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "addax-header.h"

using namespace std;
using namespace chrono;

const EC_POINT** generators = NULL;

class Crypto {
   public:
    Crypto() {
        outbio = BIO_new(BIO_s_file());
        outbio = BIO_new_fp(stdout, BIO_NOCLOSE);
        ctx = BN_CTX_new();
        // secp192r1
        eccgrp = 409;
        myecc = EC_KEY_new_by_curve_name(eccgrp);
        ecgrp = EC_KEY_get0_group(myecc);
        order = BN_new();
        EC_GROUP_get_order(ecgrp, order, ctx);
        order_bit_len = BN_num_bits(order);
        bit_len = BN_num_bits(order);

        p = BN_new();
        BN_zero(p);
    }

    void free() {
        BN_CTX_free(ctx);
        BN_free(p);
        EC_KEY_free(myecc);
        BIO_free(outbio);
    }

    BIGNUM* getP() { return p; }

    BIGNUM* getOrder() { return order; }

    int bitLen() { return bit_len; }

    BN_CTX* getCtx() { return ctx; }

    int orderBitLen() { return order_bit_len; }

    BIO* getBio() { return outbio; }

    EC_POINT* genCommitment(BIGNUM* v) {
        EC_POINT* c = EC_POINT_new(ecgrp);
        assert(EC_POINT_mul(ecgrp, c, v, NULL, NULL, ctx) == 1);
        return c;
    }

    EC_POINT* genCommitment_batch(vector<BIGNUM*> v) {
        assert(generators != NULL);
        BIGNUM** v_arr1 = new BIGNUM*[LAMBDA];
        assert(v.size() == LAMBDA);
        for (int i = 0; i < LAMBDA; i++) {
            v_arr1[i] = v[i];
        }
        const BIGNUM** v_arr = const_cast<const BIGNUM**>(v_arr1);
        EC_POINT* c = EC_POINT_new(ecgrp);
        assert(EC_POINTs_mul(ecgrp, c, NULL, LAMBDA, generators, v_arr, ctx) ==
               1);
        delete[] v_arr1;
        return c;
    }

    const EC_GROUP* getGroup() { return ecgrp; }

    EC_POINT* newEcPoint() { return EC_POINT_new(ecgrp); }

    void freeEcPoint(EC_POINT* ec) { EC_POINT_free(ec); }

    void addEcPoint(EC_POINT* a, EC_POINT* b, EC_POINT* dest) {
        assert(EC_POINT_add(ecgrp, dest, a, b, ctx) == 1);
    }

    void mulEcPoint(EC_POINT* p, BIGNUM* v, EC_POINT* dst) {
        assert(EC_POINT_mul(ecgrp, dst, NULL, p, v, ctx) == 1);
    }

    bool eqEcPoint(EC_POINT* a, EC_POINT* b) {
        if (EC_POINT_cmp(ecgrp, a, b, ctx) == 0) {
            return true;
        } else {
            return false;
        }
    }

    int EC2buf(EC_POINT* p, unsigned char* addr, int len) {
        return EC_POINT_point2oct(ecgrp, p, POINT_CONVERSION_COMPRESSED, addr,
                                  len, ctx);
    }

    EC_POINT* buf2point(unsigned char* addr, int len) {
        EC_POINT* p = EC_POINT_new(ecgrp);
        int res = EC_POINT_oct2point(ecgrp, p, addr, len, ctx);
        if (res == 1) {
            return p;
        } else {
            return NULL;
        }
    }

    void add_mod(BIGNUM* r, BIGNUM* a, BIGNUM* b) {
        BN_mod_add(r, a, b, order, ctx);
    }

    void sub_mod(BIGNUM* r, BIGNUM* a, BIGNUM* b) {
        BN_mod_sub(r, a, b, order, ctx);
    }

    void mul_mod(BIGNUM* r, BIGNUM* a, BIGNUM* b) {
        BN_mod_mul(r, a, b, order, ctx);
    }

    BIGNUM* randNum() {
        BIGNUM* v = BN_new();
        BN_rand_range(v, getOrder());
        return v;
    }

    string serializeBN(BIGNUM* v) {
        unsigned char tmp[max_bn_bytes];
        memset(tmp, 0, max_bn_bytes);
        int byte_len = BN_num_bytes(v);
        if (byte_len >= max_bn_bytes) {
            DEBUG << "bn byte len is larger than expected max bytes\n";
        }
        assert(byte_len < max_bn_bytes);
        int bn_len = BN_bn2bin(v, tmp);
        string tmp_str = string((const char*)tmp, byte_len);
        return tmp_str;
    }

    string serializeEC(EC_POINT* p) {
        unsigned char tmp[max_ec_bytes];
        int byte_len = EC2buf(p, tmp, max_ec_bytes);
        string tmp_str = string((const char*)tmp, byte_len);
        return tmp_str;
    }

    void mod(BIGNUM* v) { BN_mod(v, v, order, ctx); }

   private:
    BIO* outbio;
    BN_CTX* ctx;
    int eccgrp;
    EC_KEY* myecc = NULL;
    const EC_GROUP* ecgrp;
    BIGNUM* order;
    int bit_len;
    BIGNUM* p;
    int order_bit_len;
    int max_bn_bytes = 30;
    int max_ec_bytes = 30;
};

string sha256(string str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

// should only be called once, at the very beginning
void load_generators(string filename, Crypto& env) {
    assert(generators == NULL);
    generators = const_cast<const EC_POINT**>(new EC_POINT*[LAMBDA]);
    std::ifstream inFile;
    inFile.open(filename);  // open the input file
    std::stringstream strStream;
    strStream << inFile.rdbuf();            // read the file
    std::string str_gen = strStream.str();  // str holds the content of the file

    int max_ec_bytes = 30;
    unsigned char* str = (unsigned char*)str_gen.c_str();
    int pos = 0;
    for (int j = 0; j < LAMBDA; j++) {
        int len = (int)str[pos];
        assert(len < max_ec_bytes);
        EC_POINT* p = env.buf2point(str + pos + 1, len);
        generators[j] = p;
        pos += 1 + len;
        assert(pos <= str_gen.size());
    }
    assert(pos == str_gen.size());
}

#endif  // CRYPTO_LIB_HPP
