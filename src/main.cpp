#include "ssh2.hpp"
#include "spdlog/spdlog.h"

int main()
{
    Ssh2Client ssh("10.1.41.176");
    ssh.Connect("root", "root");
    auto channel = ssh.CreateChannel();
    channel->Write("ls -la\n");
    spdlog::info(channel->Read());

    delete channel;

    return 0;
}