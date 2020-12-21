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
ARBITRARY COMPLEXITY - A video game made for 7DFPS 2020

Usage:
  complexity
  complexity solo
  complexity connect <host> <port> [--gamecode=<gamecode>]
  complexity server <host> <port> [--exit-after-game] [--exit-timeout=<timeout>] [--gamecode=<gamecode>]
  complexity -h | --help
  complexity --version

Options:
  -h --help                 Show this help.
  --version                 Show version.
  --exit-timeout=<timeout>  Exit the server after there are no players on it for the specified number of seconds. [default: 900]
  --gamecode=<gamecode>     Gamecode to use.
)"s;

Port getPort(const std::map<std::string, docopt::value>& args)
{
    const auto port = parseInt<uint16_t>(args.at("<port>").asString());
    if (!port) {
        printErr("Port must be in [0, 65535]\n{}", usage);
        std::exit(255);
    }
    return *port;
}

uint32_t getGameCode(const std::map<std::string, docopt::value>& args)
{
    if (args.at("--gamecode")) {
        const auto gameCode = parseInt<uint32_t>(args.at("--gamecode").asString(), 16);
        if (!gameCode) {
            printErr("Gamecode must be uint32\n{}", usage);
            std::exit(255);
        }
        return *gameCode;
    }
    return 0;
}

float getExitTimeout(const std::map<std::string, docopt::value>& args)
{
    const auto timeout = parseFloat(args.at("--exit-timeout").asString());
    if (!timeout) {
        printErr("Exit timeout must be uint32\n{}", usage);
        std::exit(255);
    }
    return *timeout;
}

int main(int argc, char** argv)
{
    if (enet_initialize()) {
        printErr("Could not initialize ENet");
        return 1;
    }
    atexit(enet_deinitialize);

    const auto args
        = docopt::docopt(usage, { argv + 1, argv + argc }, true, std::to_string(version));
    // for (auto const& arg : args) std::cout << arg.first << ": " << arg.second << std::endl;

    if (args.at("solo").asBool()) {
        Server server;
        std::atomic<bool> serverFailed { false };
        float exitTimeout = getExitTimeout(args);
        std::thread serverThread([&server, &serverFailed, exitTimeout]() {
            if (!server.run("127.0.0.1", 8192, 0, exitTimeout))
                serverFailed.store(true);
        });

        while (!server.isRunning() && !serverFailed.load())
            std::this_thread::sleep_for(100ms);

        if (serverFailed.load()) {
            printErr("Error starting server");
            serverThread.join();
            return 1;
        }

        println("Server started");

        Client client;
        const auto res = client.run(HostPort { "127.0.0.1", 8192 }, 0);
        if (!res) {
            printErr("Error starting client");
        }
        println("Client stopped");

        server.stop();
        serverThread.join();
        println("Server stopped");

        return res ? 0 : 1;
    } else if (args.at("connect").asBool()) {
        Client client;
        const auto res = client.run(
            HostPort { args.at("<host>").asString(), getPort(args) }, getGameCode(args));
        if (!res) {
            printErr("Error starting client");
        }
        println("Client stopped");
        return res ? 0 : 1;
    } else if (args.at("server").asBool()) {
        Server server;
        const auto res = server.run(
            args.at("<host>").asString(), getPort(args), getGameCode(args), getExitTimeout(args));
        if (!res) {
            printErr("Error starting server");
        }
        println("Server stopped");
        return res ? 0 : 1;
    } else {
        Client client;
        const auto res = client.run(std::nullopt, 0);
        if (!res) {
            printErr("Error starting client");
        }
        println("Client stopped");
        return res ? 0 : 1;
    }
    return 0;
}
