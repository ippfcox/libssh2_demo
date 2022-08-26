#include "ssh2.hpp"
#include "spdlog/spdlog.h"

int main()
{
    Ssh2Client ssh("10.1.41.183");
    if (!ssh.Connect("root", "root"))
    {
        spdlog::error("Connect failed");
        return -1;
    }

    auto channel = ssh.CreateChannel("xterm-256color");
    if (!channel)
    {
        spdlog::error("CreateChannel failed");
        return -1;
    }

    if (!channel->Write("ls -la\n"))
    {
        spdlog::error("Write failed");
        return -1;
    }

    spdlog::info(channel->Read());

    delete channel;

    return 0;
}