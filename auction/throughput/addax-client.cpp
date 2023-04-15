#include "client.hpp"

class AddaxClient : public Client {
    int round = 2;
    // Just random stuff to start the auction
    string genReq() { return to_string(100); }

   public:
    AddaxClient(int r) {
        assert(r == 2 || r == 4);
        round = r;
    }
};

int main(int argc, char* argv[]) {
    string ip = "127.0.0.1:6667";
    int offer_load = 10;
    // time unit is 1 sec, so 60 means 1 min
    int time_unit = 60;
    int opt;
    int round = 2;
    while ((opt = getopt(argc, argv, "i:l:t:r:")) != -1) {
        if (opt == 'l') {
            offer_load = atoi(optarg);
        } else if (opt == 'i') {
            ip = string(optarg);
        } else if (opt == 't') {
            time_unit = atoi(optarg);
        } else if (opt == 'r') {
            round = atoi(optarg);
        }
    }

    AddaxClient client(round);
    client.run(ip, offer_load, time_unit);
    return 0;
}