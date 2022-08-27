#pragma once
#include <string>
#include "WinSock2.h"
#include "libssh2.h"

class Ssh2Channel
{
public:
    Ssh2Channel(LIBSSH2_CHANNEL *channel);
    ~Ssh2Channel();
    Ssh2Channel(const Ssh2Channel &) = delete;
    Ssh2Channel operator=(const Ssh2Channel &) = delete;

    std::string Read(int timeout, const std::string str_end = "$");
    std::string Read();
    bool Write(const std::string &data);

private:
    LIBSSH2_CHANNEL *channel_;
};

class Ssh2Client
{
public:
    Ssh2Client(const std::string &server_ip, const int server_port = 22);
    ~Ssh2Client();

    bool Connect(const std::string &username, const std::string &password);
    bool Disconnect();

    Ssh2Channel *CreateChannel(const std::string &pty_type = "vanilla");

private:
    std::string server_ip_;
    int server_port_ = 22;
    // std::string username_;
    // std::string password_;
    SOCKET socket_fd_;
    LIBSSH2_SESSION *session_;
};