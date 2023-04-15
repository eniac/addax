#include "addax-lib.h"
#include "net.hpp"

int publisher_fd = 0;
int total_bucket = 10000;
int base_bucket = 100;
int idx = 0;
int bid = 0;

void sendShare_client(int client_fd, string &client_share_str) {
    cout << "start sending to client\n";
    assert(sendMsg(client_fd, client_share_str));
    cout << "finish sending: " << client_share_str.size() << endl;
}

void sendShare_publisher(string publisher_ip, string &publisher_share_str) {
    cout << "start trying to connect to publisher\n";
    publisher_fd = connect_and_send(publisher_ip, publisher_share_str);
    cout << "finish sending share 2: " << publisher_share_str.size() << endl;
}

bool recv_response_blocking(int fd, string &res) {
    // start receiving response
    uint32_t res_len = 0;
    if (do_read_blocking(fd, (char *)&res_len, sizeof(uint32_t)) == false) {
        cout << "receive header fail\n";
        return false;
    }
    res_len = ntohl(res_len);
    char *tmp = (char *)malloc(sizeof(char) * res_len);
    memset(tmp, 0, res_len);
    if (do_read_blocking(fd, tmp, res_len) == false) {
        cout << "receive response fail\n";
        return false;
    }
    res = string(tmp, res_len);
    free(tmp);
    return true;
}

void listen_reqs_client(int fd, Advertiser &ad) {
    while (1) {
        string res;
        bool recv_flag = recv_response_blocking(fd, res);
        assert(res.size() == 8);
        int start_bucket = *((int *)(res.c_str()));
        // magic number to indicate asking for bid
        if (start_bucket == 0xFFFFFFF) {
            int check_bid = *((int *)(res.c_str() + 4));
            int resp = bid;
            // ask for bid not the same as bid, return -1
            if (check_bid != bid) {
                resp = -1;
            }
            cout << "send bid: " << resp << endl;
            assert(sendMsg(fd, string((char *)&resp, sizeof(int))));
            continue;
        }
        int loop = *((int *)(res.c_str() + 4));
        cout << "loop: " << loop << " start bucket: " << start_bucket << endl;
        vector<int> ids =
            generate_ids(loop, total_bucket, base_bucket, start_bucket);
        char idx_str = idx;
        string msg = idx_str + ad.getS1_str_by_vec(ids);
        assert(sendMsg(fd, msg));
        cout << idx << " sends to browser, size: " << msg.size() << endl;
    }
}

void listen_reqs_publisher(int fd, Advertiser &ad) {
    while (1) {
        string res;
        bool recv_flag = recv_response_blocking(fd, res);
        assert(res.size() == 8);
        int start_bucket = *((int *)(res.c_str()));
        // magic number to indicate asking for bid
        if (start_bucket == 0xFFFFFFF) {
            int check_bid = *((int *)(res.c_str() + 4));
            int resp = bid;
            // ask for bid not the same as bid, return -1
            if (check_bid != bid) {
                resp = -1;
            }
            assert(sendMsg(fd, string((char *)&resp, sizeof(int))));
            continue;
        }
        int loop = *((int *)(res.c_str() + 4));
        vector<int> ids =
            generate_ids(loop, total_bucket, base_bucket, start_bucket);
        char idx_str = idx;
        string msg = idx_str + ad.getS2_str_by_vec(ids);
        assert(sendMsg(fd, msg));
        cout << idx << " sends to publisher, size: " << msg.size() << endl;
    }
}

int main(int argc, char *argv[]) {
    Crypto env = Crypto();
    BIGNUM *p = env.getP();
    int bit_len = env.bitLen();
    BN_CTX *ctx = env.getCtx();
    int order_bit_len = env.orderBitLen();

    int ad_num = 20;
    bool parallel = true;
    int parallel_num = 8;
    string local_ip = "0.0.0.0:6666";
    string publisher_ip = "0.0.0.0:6667";
    idx = 0;
    int round = 2;

    int opt;
    while ((opt = getopt(argc, argv, "a:b:l:n:c:p:i:r:")) != -1) {
        if (opt == 'a') {
            ad_num = atoi(optarg);
        } else if (opt == 'b') {
            BUCKET_NUM = atoi(optarg);
        } else if (opt == 'l') {
            LAMBDA = atoi(optarg);
        } else if (opt == 'p') {
            publisher_ip = string(optarg);
        } else if (opt == 'n') {
            parallel_num = atoi(optarg);
        } else if (opt == 'c') {
            local_ip = string(optarg);
        } else if (opt == 'i') {
            idx = atoi(optarg);
        } else if (opt == 'r') {
            round = atoi(optarg);
        }
    }

    if (round == 2) {
        base_bucket = 100;
    } else if (round == 4) {
        base_bucket = 10;
    } else {
        assert(0);
    }

    total_bucket = BUCKET_NUM;

    DEBUG << "simulating " << ad_num << " advertisers\n";
    DEBUG << "simulating " << BUCKET_NUM << " buckets\n";
    DEBUG << "lambda: " << LAMBDA << endl;
    DEBUG << "parallel: " << parallel << endl;
    DEBUG << "parallel num: " << parallel_num << endl;
    DEBUG << "publisher ip: " << publisher_ip << endl;
    DEBUG << "local ip: " << local_ip << endl;
    DEBUG << "index: " << idx << endl;

    // init advertiser
    srand(time(0) - idx * 10);
    bid = rand() % BUCKET_NUM + 1;
    Advertiser ad = Advertiser(bid, BUCKET_NUM, LAMBDA, env, parallel,
                               parallel_num, false, false, true);
    cout << "idx: " << idx << " bid: " << bid << endl;

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    int sock_opt;
    assert(setsockopt(listen_fd, SOL_SOCKET,
                      TCP_NODELAY | SO_REUSEADDR | SO_REUSEPORT, &sock_opt,
                      sizeof(sock_opt)) >= 0);
    struct sockaddr_in servaddr = string_to_struct_addr(local_ip);
    assert(bind(listen_fd, (sockaddr *)&servaddr, sizeof(servaddr)) >= 0);
    assert(listen(listen_fd, 100) >= 0);
    cout << "start listening on " << local_ip << endl;
    int recv_adv_num = 0;
    vector<thread> adv_t_vec;
    double_t total_time = 0.0;
    double_t accept_sock_time = 0.0;
    system_clock::time_point starttime, endtime, start, end;
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    int client_fd =
        accept(listen_fd, (struct sockaddr *)&clientaddr, &clientaddrlen);

    // potential can change it two thread of sending
    // send two shares
    char index_str = idx;
    int start_bucket = 0;
    cout << "start generate ids\n";
    vector<int> ids = generate_ids(1, total_bucket, base_bucket, start_bucket);
    string client_share_str = index_str + ad.getS1_str_by_vec(ids);
    string publisher_share_str = index_str + ad.getS2_str_by_vec(ids);
    cout << "start threads\n";
    thread t1(&sendShare_client, client_fd, ref(client_share_str));
    thread t2(&sendShare_publisher, publisher_ip, ref(publisher_share_str));
    t1.join();
    t2.join();

    // listen on two sockets
    thread t_client(&listen_reqs_client, client_fd, ref(ad));
    thread t_publisher(&listen_reqs_publisher, publisher_fd, ref(ad));
    t_client.join();
    t_publisher.join();
}