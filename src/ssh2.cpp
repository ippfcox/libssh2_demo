#include <chrono>
#include "ssh2.hpp"
#include "spdlog/spdlog.h"
#include "WS2tcpip.h"

// channel不是在这创建的却要在这销毁，合适吗
Ssh2Channel::Ssh2Channel(LIBSSH2_CHANNEL *channel, std::function<void(std::string)> write_func)
    : channel_(channel), write_func_(write_func)
{
    spdlog::info("channel created");
    reader_ = std::thread([this]()
                          { Read(); });
}

Ssh2Channel::~Ssh2Channel()
{
    if (channel_)
    {
        libssh2_channel_free(channel_);
        channel_ = nullptr;
    }
}

void Ssh2Channel::Read()
{
    libssh2_channel_set_blocking(channel_, 0);

    while (true)
    {
        char buffer[64 * 1024] = {0};
        auto n = libssh2_channel_read(channel_, buffer, sizeof(buffer));
        if (n == LIBSSH2_ERROR_EAGAIN)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        else if (n < 0)
        {
            spdlog::error("libssh2_channel_read failed: {}", n);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        else
            write_func_(std::string(buffer));
    }
}

bool Ssh2Channel::Write(const std::string &data)
{
    return libssh2_channel_write(channel_, data.c_str(), data.size());
}

void Ssh2Channel::Wait()
{
    reader_.join();
}

Ssh2Client::Ssh2Client(const std::string &server_ip, const int server_port)
    : server_ip_(server_ip), server_port_(server_port)
{
    socket_fd_ = INVALID_SOCKET;
    session_ = nullptr;
    libssh2_init(0);
}

Ssh2Client::~Ssh2Client()
{
    Disconnect();
    libssh2_exit();
}

bool Ssh2Client::Connect(const std::string &username, const std::string &password)
{
    WORD socket_version = MAKEWORD(2, 2);
    WSAData wsa_data;
    if (WSAStartup(socket_version, &wsa_data) != 0)
    {
        spdlog::error("WSAStartup failed");
        return false;
    }

    if ((socket_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
    {
        spdlog::error("socket failed");
        return false;
    }

    sockaddr_in sin = {0};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(server_port_);
    inet_pton(AF_INET, server_ip_.c_str(), &sin.sin_addr);
    if (connect(socket_fd_, (sockaddr *)&sin, sizeof(sockaddr_in)) == SOCKET_ERROR)
    {
        spdlog::error("connect {}:{} failed", server_ip_, server_port_);
        Disconnect();
        return false;
    }

    spdlog::info("socket connected to {}:{}", server_ip_, server_port_);

    session_ = libssh2_session_init();
    // libssh2_session_set_blocking(session_, 1);
    if (libssh2_session_handshake(session_, socket_fd_) != 0)
    {
        spdlog::error("libssh2_session_handshake failed");
        Disconnect();
        return false;
    }

    std::string fingerprint = libssh2_hostkey_hash(session_, LIBSSH2_HOSTKEY_HASH_SHA1);
    std::string userauth_list = libssh2_userauth_list(session_, username.c_str(), username.size());

    if (userauth_list.find("password") != -1)
    {
        if (libssh2_userauth_password(session_, username.c_str(), password.c_str()) != 0)
        {
            spdlog::error("libssh2_userauth_password failed");
            Disconnect();
            return false;
        }
    }
    else if (userauth_list.find("keyboard-interactive") != -1)
    {
        spdlog::error("not implemented");
        Disconnect();
        return false;
        // if (libssh2_userauth_keyboard_interactive(session_, username_.c_str(),))
    }
    else if (userauth_list.find("publickey") != -1)
    {
        spdlog::error("not implemented");
        Disconnect();
        return false;
    }

    spdlog::info("ssh authoricated with username {}", username);

    return true;
}

bool Ssh2Client::Disconnect()
{
    if (session_)
    {
        libssh2_session_disconnect(session_, "bye");
        libssh2_session_free(session_);
        session_ = nullptr;
    }

    if (socket_fd_ != INVALID_SOCKET)
    {
        closesocket(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
    }

    return true;
}

Ssh2Channel *Ssh2Client::CreateChannel(std::function<void(std::string)> write_func, const std::string &pty_type)
{
    auto channel = libssh2_channel_open_session(session_);
    if (!channel)
    {
        spdlog::error("libssh2_channel_open_session failed");
        return nullptr;
    }

    if (libssh2_channel_request_pty(channel, pty_type.c_str()) != 0)
    {
        spdlog::error("libssh2_channel_request_pty failed");
        libssh2_channel_free(channel);
        return nullptr;
    }

    if (libssh2_channel_shell(channel) != 0)
    {
        spdlog::error("libssh2_channel_shell failed");
        libssh2_channel_free(channel);
        return nullptr;
    }

    return new Ssh2Channel(channel, write_func);
}