#include "addax-lib.h"

int main(int argc, char *argv[]) {
    Crypto env = Crypto();
    BIGNUM *p = env.getP();
    int bit_len = env.bitLen();
    BN_CTX *ctx = env.getCtx();
    int order_bit_len = env.orderBitLen();

    int ad_num = 20;
    bool parallel = true;
    int parallel_num = 8;
    bool interactive = false;

    string share_file = "";
    string commit_file = "";
    int opt;
    while ((opt = getopt(argc, argv, "b:s:c:i")) != -1) {
        if (opt == 'b') {
            BUCKET_NUM = atoi(optarg);
        } else if (opt == 's') {
            share_file = string(optarg);
        } else if (opt == 'c') {
            commit_file = string(optarg);
        } else {
            assert(0);
        }
    }
    assert(share_file != "");
    assert(commit_file != "");
    load_generators("generators.txt", env);
    struct timeval time;
    gettimeofday(&time, NULL);
    srand((time.tv_sec * 1000) + (time.tv_usec / 1000));

    int bid = rand() % BUCKET_NUM + 1;
    if (bid >= BUCKET_NUM) {
        bid = BUCKET_NUM;
    }
    Advertiser ad = Advertiser(bid, BUCKET_NUM, LAMBDA, env, parallel,
                               parallel_num, true, false, true);

    string s1_str = ad.getS1_str();
    string s2_str = ad.getS2_str();
    string commit_str = ad.getBid_commitment_str();
    string s1_file = share_file + "-s1-bid-" + to_string(bid);
    string s2_file = share_file + "-s2-bid-" + to_string(bid);
    string comm_file = commit_file + "-comm-bid-" + to_string(bid);
    cout << "s1 file: " << s1_file << endl;
    cout << "s2 file: " << s2_file << endl;
    cout << "comm file: " << comm_file << endl;

    ofstream s1_out(s1_file);
    ofstream s2_out(s2_file);
    ofstream comm_out(comm_file);
    s1_out << s1_str;
    s2_out << s2_str;
    comm_out << commit_str;
    s1_out.close();
    s2_out.close();
    comm_out.close();
    return 0;
}