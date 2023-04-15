#include "addax-lib.h"

// cost of verifier
// for ineractive protocol, assume that we verify 2*round of sum vectors and bit
// encodings for bogus shares

void load_all_shares(string dir_name, string s1_file_names,
                     string s2_file_names, vector<string> &s1_vec,
                     vector<string> &s2_vec, string commit_filename,
                     string commit_dir_name, vector<string> &commit_vec) {
    int ad_num = s1_vec.size();
    ifstream in1(s1_file_names);
    ifstream in2(s2_file_names);
    ifstream incom(commit_filename);
    for (int i = 0; i < ad_num; i++) {
        string n1, n2, n3;
        getline(in1, n1);
        getline(in2, n2);
        getline(incom, n3);
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
        ifstream commfile;
        commfile.open(commit_dir_name + "/" + n3);  // open the input file
        std::stringstream ssc;
        ssc << commfile.rdbuf();  // read the file
        commit_vec[i] = ssc.str();
        cout << (commit_dir_name + "/" + n3) << endl;
    }
    in1.close();
    in2.close();
}

int main(int argc, char *argv[]) {
    Crypto env = Crypto();
    BIGNUM *p = env.getP();
    int bit_len = env.bitLen();
    BN_CTX *ctx = env.getCtx();
    int order_bit_len = env.orderBitLen();

    int ad_num = 100;
    bool parallel = true;
    int parallel_num = 8;
    int task_num = 1;
    int parallel_num_committee = 8;
    int parallel_num_sum = 8;
    int parallel_sum = true;
    bool interactive = false;

    string s1_filenames = "";
    string s2_filenames = "";
    string commit_filenames = "";
    string share_dir_name = "";
    string commit_dir_name = "";

    int opt;
    while ((opt = getopt(argc, argv, "a:b:t:is:S:d:D:c:")) != -1) {
        if (opt == 'a') {
            ad_num = atoi(optarg);
        } else if (opt == 'b') {
            BUCKET_NUM = atoi(optarg);
        } else if (opt == 't') {
            task_num = atoi(optarg);
        } else if (opt == 'c') {
            cout << optarg << endl;
            commit_filenames = string(optarg);
        } else if (opt == 'i') {
            interactive = true;
        } else if (opt == 's') {
            s1_filenames = string(optarg);
        } else if (opt == 'S') {
            s2_filenames = string(optarg);
        } else if (opt == 'd') {
            share_dir_name = string(optarg);
        } else if (opt == 'D') {
            commit_dir_name = string(optarg);
        }
    }

    DEBUG << "simulating " << ad_num << " advertisers\n";
    DEBUG << "simulating " << BUCKET_NUM << " buckets\n";
    DEBUG << "lambda: " << LAMBDA << endl;
    DEBUG << "task num: " << task_num << endl;
    DEBUG << "interactive: " << interactive << endl;
    DEBUG << "s1 filenames: " << s1_filenames << endl;
    DEBUG << "s2 filenames: " << s2_filenames << endl;
    DEBUG << "share dir: " << share_dir_name << endl;
    DEBUG << "commit dir: " << commit_dir_name << endl;
    DEBUG << "commit filenames: " << commit_filenames << endl;

    assert(s1_filenames != "");
    assert(s2_filenames != "");
    assert(share_dir_name != "");
    assert(commit_dir_name != "");
    assert(commit_filenames != "");

    load_generators("generators.txt", env);
    double total_time = 0.0;

    system_clock::time_point starttime, endtime, start, end;

    vector<string> all_advs_s1_vec(ad_num);
    vector<string> all_advs_s2_vec(ad_num);
    vector<string> commit_str_vec(ad_num);

    double all_time = 0.0;

    // load all s1 s2 from files
    load_all_shares(share_dir_name, s1_filenames, s2_filenames, all_advs_s1_vec,
                    all_advs_s2_vec, commit_filenames, commit_dir_name,
                    commit_str_vec);

    for (int times = 0; times < task_num; times++) {
        int curr_ad_num = ad_num;
        DEBUG << "simulating " << curr_ad_num << " advertisers for " << times
              << " run" << endl;
        Committee c1 =
            Committee(BUCKET_NUM, LAMBDA, parallel, parallel_num_committee);
        Committee c2 =
            Committee(BUCKET_NUM, LAMBDA, parallel, parallel_num_committee);
        start = system_clock::now();

        // committee receive and decode share vecs
        starttime = system_clock::now();
        if (parallel) {
            c1.initShares(curr_ad_num);
            c2.initShares(curr_ad_num);
            c1.deserial_addShares_parallel(all_advs_s1_vec);
            c2.deserial_addShares_parallel(all_advs_s2_vec);
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

        vector<vector<BIGNUM *>> sum_s1_raw =
            sumBNvec(c1.getShares(), parallel_sum, parallel_num_sum);
        vector<vector<BIGNUM *>> sum_s2_raw =
            sumBNvec(c2.getShares(), parallel_sum, parallel_num_sum);

        // searialize shares (two sum vectors)
        string sum_s1_str = serializeShareVec_out(sum_s1_raw);
        string sum_s2_str = serializeShareVec_out(sum_s2_raw);

        // serialize n bits
        vector<string> serialized_bit_encoding_s1;
        vector<string> serialized_bit_encoding_s2;
        for (int i = 0; i < curr_ad_num; i++) {
            serialized_bit_encoding_s1.push_back(
                serializeBit(c1.getShares()[i][0]));
            serialized_bit_encoding_s2.push_back(
                serializeBit(c2.getShares()[i][0]));
        }

        Advertiser verifier = Advertiser(0, BUCKET_NUM, LAMBDA, env, parallel,
                                         parallel_num, false, true);
        // deserialize two share vectors
        starttime = system_clock::now();
        vector<vector<BIGNUM *>> sum_s1 = deserializeShare_out(sum_s1_str);
        vector<vector<BIGNUM *>> sum_s2 = deserializeShare_out(sum_s2_str);
        endtime = system_clock::now();
        DEBUG << "TIME: verifier deserialize sum vectors: "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                     .count()
              << endl;

        starttime = system_clock::now();
        verifier.init_commitments(curr_ad_num);
        if (parallel) {
            verifier.deserialize_parallel_single(commit_str_vec, parallel_num);
        } else {
            assert(0);
        }
        endtime = system_clock::now();
        DEBUG << "TIME: verifier deserialize commitments: "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                     .count()
              << endl;

        // deserialize bits
        starttime = system_clock::now();
        vector<vector<BIGNUM *>> sum_bit_vec = deserializeShare_bit(
            serialized_bit_encoding_s1, serialized_bit_encoding_s2);

        endtime = system_clock::now();
        DEBUG << "TIME: verifier deserialize bits: "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                     .count()
              << endl;

        vector<vector<vector<BIGNUM *>>> share_input;
        share_input.push_back(sum_s1);
        share_input.push_back(sum_s2);
        vector<vector<BIGNUM *>> sum_vec_s = sumBNvec(share_input);
        starttime = system_clock::now();
        int decode_bid = decode_bit_vec(sum_vec_s);
        DEBUG << "AND decode (using shares): " << decode_bid << endl;
        endtime = system_clock::now();
        DEBUG << "TIME: decode one bit: "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                     .count()
              << endl;

        bool verify;
        // substract one share, for simplicity use share of first advertiser
        subShare(sum_s1, c1.getShares()[0]);
        subShare(sum_s2, c2.getShares()[0]);

        // verify sum vectors
        starttime = system_clock::now();
        vector<vector<BIGNUM *>> sum_vec_removed;
        if (!interactive) {
            vector<vector<vector<BIGNUM *>>> removed_share_input;
            removed_share_input.push_back(sum_s1);
            removed_share_input.push_back(sum_s2);

            sum_vec_removed = sumBNvec(removed_share_input);
            verify = verifySum(verifier.getTotal_bid_commitment(), sum_vec_s,
                               sum_vec_removed, env, parallel, parallel_num,
                               interactive);
        } else {
            verify =
                verifySum(verifier.getTotal_bid_commitment(), sum_vec_s,
                          sum_vec_s, env, parallel, parallel_num, interactive);
        }
        DEBUG << "verify: " << verify << endl;
        assert(verify);
        endtime = system_clock::now();
        DEBUG << "TIME: verifier verify sum vectors (interactive only 1, "
                 "non-interactive 2): "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                     .count()
              << endl;

        starttime = system_clock::now();
        vector<vector<EC_POINT *>> bit_commitments =
            verifier.getTotal_bid_commitment();
        // for simplicity, we verify the first bit
        bool verify_bit_res = verify_bit(sum_bit_vec, bit_commitments);
        DEBUG << "verify bits: " << verify_bit_res << endl;
        assert(verify_bit_res);

        endtime = system_clock::now();
        DEBUG << "TIME: verifier verifies bits: "
              << duration_cast<std::chrono::duration<double>>(endtime -
                                                              starttime)
                     .count()
              << endl;

        verifier.free();
        freeSumBNvec(sum_s1);
        freeSumBNvec(sum_s2);
        freeSumBNvec(sum_s1_raw);
        freeSumBNvec(sum_s2_raw);
        freeSumBNvec(sum_vec_s);
        if (!interactive) freeSumBNvec(sum_vec_removed);
        c1.free();
        c2.free();
    }
    return 0;
}
