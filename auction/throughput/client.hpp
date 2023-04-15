#include <thread>
#include <algorithm>

#include "net.hpp"

class Client {
   private:
    virtual string genReq() = 0;

    void sendReq(string ip, vector<double> &handle_time, int id) {
        system_clock::time_point start, end;
        start = system_clock::now();
        const SSL_METHOD *method = TLS_client_method();

        SSL_CTX *ctx = SSL_CTX_new(method);
        assert(ctx != NULL);
        SSL *ssl = SSL_new(ctx);
        int fd = connect_to_addr(ip);
        SSL_set_fd(ssl, fd);
        assert(SSL_connect(ssl) == 1);

        // send request
        string reqMsg = genReq();
        assert(sendMsg(ssl, reqMsg));

        // receive response
        string response;
        recvMsg(ssl, response);
        end = system_clock::now();
        handle_time[id] =
            duration_cast<std::chrono::duration<double>>(end - start).count();
        cout << id << " handle time: " << handle_time[id] << endl;

        SSL_free(ssl);
        close(fd);
        SSL_CTX_free(ctx);
    }

   public:
    Client() {}

    void run(string ip, int offer_load, int time_unit) {
        // init handle_time
        vector<double> handle_time = vector<double>(offer_load, 0.0);
        assert(handle_time.size() == offer_load);

        // 1s
        unsigned int sleep_period = 1000000 / 1 / offer_load * time_unit;
        cout << "sleep period: " << sleep_period << endl;
        vector<thread> t_vec;

        system_clock::time_point start, end;
        start = system_clock::now();
        for (int i = 0; i < offer_load; i++) {
            thread t(&Client::sendReq, this, ip, ref(handle_time), i);
            t_vec.push_back(move(t));
            usleep(sleep_period);
        }

        for (auto &th : t_vec) {
            th.join();
        }
        end = system_clock::now();
        cout
            << "total time spent: "
            << duration_cast<std::chrono::duration<double>>(end - start).count()
            << endl;

        double total_handle_time = 0.0;
        for (int i = 0; i < handle_time.size(); i++) {
            assert(handle_time[i] != 0.0);
            total_handle_time += handle_time[i];
        }

        sort(handle_time.begin(), handle_time.end());
        int idx_50 = handle_time.size() * 0.5;
        int idx_99 = handle_time.size() * 0.99 - 1;

        cout << "50 percent, id: " << idx_50 << ", handle_time: " << handle_time[idx_50] << endl;

        cout << "99 percent, id: " << idx_99 << ", handle_time: " << handle_time[idx_99] << endl;

        cout << "avg handle time: " << total_handle_time / offer_load << endl;
    }
};