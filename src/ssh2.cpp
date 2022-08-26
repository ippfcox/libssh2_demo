#include "ssh2.hpp"
#include "spdlog/spdlog.h"
#include "WS2tcpip.h"

// channel不是在这创建的却要在这销毁，合适吗
Ssh2Channel::Ssh2Channel(LIBSSH2_CHANNEL *channel)
    : channel_(channel)
{
}

Ssh2Channel::~Ssh2Channel()
{
    if (channel_)
    {
        libssh2_channel_free(channel_);
        channel_ = nullptr;
    }
}

std::string Ssh2Channel::Read(const std::string str_end, int timeout)
{
    std::string data;

    auto fds = new LIBSSH2_POLLFD[1];
    fds[0].type = LIBSSH2_POLLFD_CHANNEL;
    fds[0].fd.channel = channel_;
    fds[0].events = LIBSSH2_POLLFD_POLLIN | LIBSSH2_POLLFD_POLLOUT;

    while (timeout > 0)
    {
        if (libssh2_poll(fds, 1, 10) < 1)
        {
            timeout -= 50;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (fds[0].revents & LIBSSH2_POLLFD_POLLIN)
        {
            auto buffer = new char[64 * 1024];
            size_t n = libssh2_channel_read(channel_, buffer, sizeof(buffer));
            if (n == LIBSSH2_ERROR_EAGAIN)
            {
                timeout -= 50;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            else if (n <= 0)
            {
                return data;
            }
            else
            {
                data += std::string(buffer);
            }
        }
    }

    spdlog::error("read timeout");

    return data;
}

bool Ssh2Channel::Write(const std::string &data)
{
    return libssh2_channel_write(channel_, data.c_str(), data.size());
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

Ssh2Channel *Ssh2Client::CreateChannel(const std::string &pty_type)
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

    return new Ssh2Channel(channel);
}