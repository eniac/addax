#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace chrono;

vector<string> split(string &s, const string &delim) {
    vector<string> results;
    if (s == "") {
        return results;
    }
    size_t prev = 0, cur = 0;
    do {
        cur = s.find(delim, prev);
        if (cur == string::npos) {
            cur = s.length();
        }
        string part = s.substr(prev, cur - prev);
        if (!part.empty()) {
            results.emplace_back(part);
        }
        prev = cur + delim.length();
    } while (cur < s.length() && prev < s.length());
    return results;
}

struct sockaddr_in string_to_struct_addr(string &str) {
    struct sockaddr_in result;
    memset(&result, 0, sizeof(result));
    auto addr_and_port = split(str, ":");
    result.sin_addr.s_addr = inet_addr(addr_and_port[0].c_str());
    result.sin_family = AF_INET;
    result.sin_port = htons(atoi(addr_and_port[1].c_str()));
    return result;
}

// -1 indicate errrors
int connect_to_addr(string addr) {
    struct timeval Timeout;
    Timeout.tv_sec = 6000;
    Timeout.tv_usec = 0;
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sockaddr = string_to_struct_addr(addr);
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        fprintf(stderr, "Get flags error:%s\n", strerror(errno));
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        cout << "get flags failed\n";
        close(fd);
        return -1;
    }
    fd_set wait_set;
    int res = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    // connected
    if (res == 0) {
        if (fcntl(fd, F_SETFL, flags) < 0) {
            cout << "connect fcntl failed\n";
            close(fd);
            return -1;
        }
        return fd;
    } else {
        FD_ZERO(&wait_set);
        FD_SET(fd, &wait_set);

        // wait for socket to be writable; return after given timeout
        res = select(fd + 1, NULL, &wait_set, NULL, &Timeout);
        // return -1;
    }
    if (fcntl(fd, F_SETFL, flags) < 0) {
        cout << "connect fcntl failed\n";
        close(fd);
        return -1;
    }

    if (res < 0) {
        cout << "connect timeout\n";
        close(fd);
        return -1;
    }

    if (FD_ISSET(fd, &wait_set)) {
        // cout << "get a valid fd: " << fd << endl;
        return fd;
    } else {
        cout << "connect timeout\n";
        close(fd);
        return -1;
    }
    assert(0);
    return -1;
}

bool do_write(SSL *ssl, const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = SSL_write(ssl, &buf[sent], len - sent);
        if (n < 0) {
            fprintf(stderr, "Fail to write: %s\n", strerror(errno));
            return false;
        }
        sent += n;
    }
    return true;
}

bool do_read(SSL *ssl, char *buf, int len) {
    int rcvd = 0;
    while (rcvd < len) {
        int n = SSL_read(ssl, &buf[rcvd], len - rcvd);
        if (n < 0) {
            fprintf(stderr, "Fail to read: %d %s\n", errno, strerror(errno));
            return false;
        }
        rcvd += n;
    }
    return true;
}

// Note this only supports at most 4GB data
// Due to the we use `uint32_t` as the length of message
string formatMsg(string input) {
    char len[sizeof(uint32_t)];
    uint32_t str_len = htonl(input.size());
    memcpy(len, &str_len, sizeof(uint32_t));
    string prefix = string(len, sizeof(uint32_t));
    prefix.append(input);
    assert(prefix.size() == input.size() + sizeof(uint32_t));
    return prefix;
}

bool do_read_blocking(int fd, char *buf, int len) {
    int rcvd = 0;
    while (rcvd < len) {
        int n = read(fd, &buf[rcvd], len - rcvd);
        if (n < 0) {
            fprintf(stderr, "Fail to read: %d %s\n", errno, strerror(errno));
            return false;
        }
        rcvd += n;
    }
    return true;
}

bool sendMsg(SSL *ssl, string msg) {
    string send_msg = formatMsg(msg);
    if (!do_write(ssl, send_msg.c_str(), send_msg.size())) {
        return false;
    }
    return true;
}

bool recvMsg(SSL *ssl, string &res) {
    // start receiving response
    uint32_t res_len = 0;
    if (do_read(ssl, (char *)&res_len, sizeof(uint32_t)) == false) {
        cout << "receive header fail\n";
        return false;
    }
    res_len = ntohl(res_len);
    char *tmp = (char *)malloc(sizeof(char) * res_len);
    memset(tmp, 0, res_len);
    if (do_read(ssl, tmp, res_len) == false) {
        cout << "receive response fail\n";
        return false;
    }
    res = string(tmp, res_len);
    free(tmp);
    return true;
}