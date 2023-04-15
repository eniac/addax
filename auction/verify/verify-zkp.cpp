#include "addax-lib.h"

int main(int argc, char *argv[]) {
    Crypto env = Crypto();

    bool parallel = true;
    int parallel_num = 8;
    bool interactive = false;

    int opt;
    while ((opt = getopt(argc, argv, "b:")) != -1) {
        if (opt == 'b') {
            BUCKET_NUM = atoi(optarg);
        } else {
            assert(0);
        }
    }

    int bid = rand() % BUCKET_NUM + 1;
    if (bid >= BUCKET_NUM) {
        bid = BUCKET_NUM;
    }

    load_generators("generators.txt", env);
    Advertiser ad = Advertiser(bid, BUCKET_NUM, LAMBDA, env, parallel,
                               parallel_num, true, false, true);

    system_clock::time_point start, end;

    // get all strings
    string randMask_bid_str = ad.getRandMask_bid_str();
    string randMask_bid_commitment_str = ad.getRandMask_bid_commitment_str();
    string bid_commitment_str = ad.getBid_commitment_str();
    string zkp_u_str = ad.getZKP_u_str();
    string zkp_v_str = ad.getZKP_v_str();
    string zkp_z_str = ad.getZKP_z_str();

    // deserialize commitments etc.
    start = system_clock::now();
    vector<EC_POINT *> randMask_bid_commitment =
        ad.deserializeCommitment(randMask_bid_commitment_str, env);
    vector<EC_POINT *> bid_commitment =
        ad.deserializeCommitment(bid_commitment_str, env);
    vector<EC_POINT *> zkp_u = ad.deserializeCommitment(zkp_u_str, env);
    vector<BIGNUM *> zkp_v = deserializeZKP(zkp_v_str);
    vector<BIGNUM *> zkp_z = deserializeZKP(zkp_z_str);
    end = system_clock::now();
    DEBUG << "deserialize zkp: "
          << duration_cast<std::chrono::duration<double>>(end - start).count()
          << endl;

    // verify zkp
    assert(verify_zkp(bid_commitment, randMask_bid_commitment, zkp_u, zkp_v,
                      zkp_z, BUCKET_NUM, parallel_num));
    end = system_clock::now();
    DEBUG << "verify zkp (total time): "
          << duration_cast<std::chrono::duration<double>>(end - start).count()
          << endl;
}