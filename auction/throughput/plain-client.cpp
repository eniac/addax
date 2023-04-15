#include "client.hpp"

class PlainClient : public Client {
    string genReq() { return to_string(100); }
};

int main(int argc, char* argv[]) {
    string ip = "127.0.0.1:6667";
    int offer_load = 10;
    // time unit is 1 sec, so 60 means 1 min
    int time_unit = 60;
    int opt;
    while ((opt = getopt(argc, argv, "i:l:t:")) != -1) {
        if (opt == 'l') {
            offer_load = atoi(optarg);
        } else if (opt == 'i') {
            ip = string(optarg);
        } else if (opt == 't') {
            time_unit = atoi(optarg);
        }
    }

    PlainClient client;
    client.run(ip, offer_load, time_unit);
    return 0;
}