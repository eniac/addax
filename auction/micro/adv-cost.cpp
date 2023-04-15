#include "addax-lib.h"

int main(int argc, char *argv[]) {
    Crypto env = Crypto();
    BIGNUM *p = env.getP();
    int bit_len = env.bitLen();
    BN_CTX *ctx = env.getCtx();
    int order_bit_len = env.orderBitLen();

    bool parallel = true;
    int parallel_num = 8;

    int opt;
    while ((opt = getopt(argc, argv, "b:")) != -1) {
        if (opt == 'b') {
            BUCKET_NUM = atoi(optarg);
        }
    }
    int bid = rand() % BUCKET_NUM + 1;
    if (bid >= BUCKET_NUM) {
        bid = BUCKET_NUM;
    }

    load_generators("generators.txt", env);
    Advertiser ad = Advertiser(bid, BUCKET_NUM, LAMBDA, env, parallel,
                               parallel_num, true, false, true);

    string s1_str = ad.getS1_str();
    string s2_str = ad.getS2_str();
    string commit_str = ad.getBid_commitment_str();
    cout << "share 1 size: " << s1_str.size() << endl;
    cout << "share 2 size: " << s2_str.size() << endl;
    cout << "commitment size: " << commit_str.size() << endl;

    // measure size of zkp
    cout << "randmask bid vec size: " << ad.getRandMask_bid_str().size()
         << endl;
    cout << "randmask commitment size: "
         << ad.getRandMask_bid_commitment_str().size() << endl;

    cout << "zkp size: "
         << ad.getZKP_u_str().size() + ad.getZKP_v_str().size() +
                ad.getZKP_z_str().size()
         << endl;

    return 0;
}