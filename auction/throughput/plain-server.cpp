#include <random>
#include <sstream>

#include "server.hpp"

#define BID_RANGE (1 << 15)
#define BIDDER_NUM 96

class PlainServer : public Server {
    void handle(string msg, SSL* ssl) {
        // deserialize
        for (int i = 0; i < BIDDER_NUM; i++) {
            int bid = atoi(msg.c_str());
        }

        srand(time(0));
        // run a second price auction
        vector<int> bids(BIDDER_NUM);
        for (int i = 0; i < bids.size(); i++) {
            bids[i] = rand() % BID_RANGE;
        }

        int max = bids[0];
        int secondMax = bids[1];
        int maxId = 0;
        int secondId = 0;
        for (int i = 0; i < bids.size(); i++) {
            if (max < bids[i]) {
                secondMax = max;
                secondId = maxId;
                max = bids[i];
                maxId = i;
            }
        }

        stringstream ss;
        ss << maxId << " " << secondMax;
        // return the winner and winning bid as response
        string response = ss.str();
        assert(sendMsg(ssl, response));
    }
};

int main(int argc, char* argv[]) {
    string ip = "0.0.0.0:6667";
    // fixed 1 sec
    double time_period = 1.0;
    int opt;
    while ((opt = getopt(argc, argv, "i:t:")) != -1) {
        if (opt == 'i') {
            ip = string(optarg);
        } else if (opt == 't') {
            time_period = atoi(optarg);
        }
    }
    PlainServer server;
    server.run(ip, time_period);
    return 0;
}