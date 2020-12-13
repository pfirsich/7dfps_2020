#include <chrono>
#include <thread>

#include <iostream>

#include <fmt/format.h>

#include <docopt/docopt.h>

#include "client.hpp"
#include "server.hpp"
#include "util.hpp"
#include "version.hpp"

using namespace std::chrono_literals;
using namespace std::literals;

static const auto usage = R"(
7dfps

Usage:
  7dfps
  7dfps solo
  7dfps connect <host> <port> [--gamecode=<gamecode>]
  7dfps server <host> <port> [--exit-after-game] [--exit-timeout=<timeout>] [--gamecode=<gamecode>]
  7dfps -h | --help
  7dfps --version

Options:
  -h --help                 Show this help.
  --version                 Show version.
  --exit-after-game         Exit the server when the game ends.
  --exit-timeout=<timeout>  Exit the server if no players have joined in the specified time (in seconds). [default: 60]
  --gamecode=<gamecode>     Gamecode to use.
)"s;

Port getPort(const std::map<std::string, docopt::value>& args)
{
    const auto port = parseInt<uint16_t>(args.at("<port>").asString());
    if (!port) {
        fmt::print(stderr, "Port must be in [0, 65535]\n{}\n", usage);
        std::exit(255);
    }
    return *port;
}

uint32_t getGameCode(const std::map<std::string, docopt::value>& args)
{
    if (args.at("--gamecode")) {
        const auto gameCode = parseInt<uint32_t>(args.at("--gamecode").asString(), 16);
        if (!gameCode) {
            fmt::print(stderr, "Gamecode must be uint32_t\n{}\n", usage);
            std::exit(255);
        }
        return *gameCode;
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (enet_initialize()) {
        fmt::print(stderr, "Could not initialize ENet\n");
        return 1;
    }
    atexit(enet_deinitialize);

    const auto args
        = docopt::docopt(usage, { argv + 1, argv + argc }, true, std::to_string(version));
    for (auto const& arg : args)
        std::cout << arg.first << ": " << arg.second << std::endl;

    if (args.at("solo").asBool()) {
        Server server;
        std::atomic<bool> serverFailed { false };
        std::thread serverThread([&server, &serverFailed]() {
            if (!server.run("127.0.0.1", 8192, 0))
                serverFailed.store(true);
        });

        while (!server.isRunning() && !serverFailed.load())
            std::this_thread::sleep_for(100ms);

        if (serverFailed.load()) {
            fmt::print(stderr, "Error starting server\n");
            serverThread.join();
            return 1;
        }

        fmt::print("Server started\n");

        Client client;
        const auto res = client.run(HostPort { "127.0.0.1", 8192 }, 0);
        if (!res) {
            fmt::print(stderr, "Error starting client\n");
        }
        fmt::print("Client stopped\n");

        server.stop();
        serverThread.join();
        fmt::print("Server stopped\n");

        return res ? 0 : 1;
    } else if (args.at("connect").asBool()) {
        Client client;
        const auto res = client.run(
            HostPort { args.at("<host>").asString(), getPort(args) }, getGameCode(args));
        if (!res) {
            fmt::print(stderr, "Error starting client\n");
        }
        fmt::print("Client stopped\n");
        return res ? 0 : 1;
    } else if (args.at("server").asBool()) {
        Server server;
        const auto res = server.run(args.at("<host>").asString(), getPort(args), getGameCode(args));
        if (!res) {
            fmt::print(stderr, "Error starting server\n");
        }
        fmt::print("Server stopped\n");
        return res ? 0 : 1;
    } else {
        Client client;
        const auto res = client.run(std::nullopt, 0);
        if (!res) {
            fmt::print(stderr, "Error starting client\n");
        }
        fmt::print("Client stopped\n");
        return res ? 0 : 1;
    }
    return 0;
}
