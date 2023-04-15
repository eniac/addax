#include <arpa/inet.h>
#include <assert.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <iostream>

#include "net.hpp"

using namespace std;
using namespace chrono;

class Server {
   private:
    virtual void handle(string msg, SSL *ssl) = 0;

    SSL_CTX *create_context() {
        const SSL_METHOD *method;
        SSL_CTX *ctx;

        method = TLS_server_method();

        ctx = SSL_CTX_new(method);
        if (!ctx) {
            perror("Unable to create SSL context");
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }

        return ctx;
    }

    void configure_context(SSL_CTX *ctx) {
        /* Set the key and cert */
        if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <=
            0) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }

        if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <=
            0) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
    }

   public:
    Server() {}

    void run(string ip, double time_period) {
        int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
        int sock_opt;
        assert(setsockopt(listen_fd, SOL_SOCKET,
                          TCP_NODELAY | SO_REUSEADDR | SO_REUSEPORT, &sock_opt,
                          sizeof(sock_opt)) >= 0);
        struct sockaddr_in servaddr = string_to_struct_addr(ip);
        assert(bind(listen_fd, (sockaddr *)&servaddr, sizeof(servaddr)) >= 0);
        assert(listen(listen_fd, 100) >= 0);

        // create and configure SSL context
        SSL_CTX *ctx = create_context();
        configure_context(ctx);

        system_clock::time_point total_start, total_end;
        bool started = false;
        int handled_task = 0;
        bool found = false;

        system_clock::time_point start, end;
        while (true) {
            // accept for one connection
            struct sockaddr_in clientaddr;
            socklen_t clientaddrlen = sizeof(clientaddr);
            int client_fd = accept(listen_fd, (struct sockaddr *)&clientaddr,
                                   &clientaddrlen);

            // establish tls connection
            SSL *ssl = SSL_new(ctx);
            SSL_set_fd(ssl, client_fd);
            assert(SSL_accept(ssl) > 0);

            string recv_msg;
            start = system_clock::now();
            if (!recvMsg(ssl, recv_msg)) {
                cout << "receive msg fail\n";
                continue;
            }
            if (!started) {
                total_start = system_clock::now();
                started = true;
            }

            // invoke handler function of server class
            handle(recv_msg, ssl);
            end = system_clock::now();

            cout << "server handle time: "
                 << duration_cast<std::chrono::duration<double>>(end - start)
                        .count()
                 << endl;

            handled_task++;

            total_end = system_clock::now();
            cout << "elapsed time: "
                 << duration_cast<std::chrono::duration<double>>(total_end -
                                                                 total_start)
                        .count()
                 << ", handled task: " << handled_task << endl;
            if (!found && duration_cast<std::chrono::duration<double>>(
                              total_end - total_start)
                                  .count() > time_period) {
                cout << "FOUND throughput " << handled_task << " per "
                     << time_period << "s" << endl;
                found = true;
            }
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(client_fd);
        }
    }
};
