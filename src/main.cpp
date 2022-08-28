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

    auto channel = ssh.CreateChannel([](std::string data)
                                     { spdlog::info(data); },
                                     "xterm-256color");
    if (!channel)
    {
        spdlog::error("CreateChannel failed");
        return -1;
    }

    if (!channel->Write("top\n"))
    {
        spdlog::error("Write failed");
        return -1;
    }

    channel->Wait();

    delete channel;

    return 0;
}