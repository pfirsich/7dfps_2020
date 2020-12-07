#include <fmt/format.h>

#include <chrono>
#include <thread>

#include "client.hpp"
#include "server.hpp"

using namespace std::chrono_literals;

/* TODO
 * ECS: Make EntityId fully internal and only use EntityHandle outside of ECS implementation
 * ECS: Move all (short) implementations out of the header part
 */

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        Server server;
        std::atomic<bool> serverFailed { false };
        std::thread serverThread([&server, &serverFailed]() {
            if (!server.run("127.0.0.1", 8192))
                serverFailed.store(true);
        });

        while (!server.isRunning() && !serverFailed.load())
            std::this_thread::sleep_for(100ms);

        if (serverFailed.load()) {
            fmt::print("Error starting server.\n");
            serverThread.join();
            return 1;
        }

        fmt::print("Server started.\n");

        Client client;
        const auto res = client.run("127.0.0.1", 8192);
        if (!res) {
            fmt::print("Error starting client.\n");
        }
        fmt::print("Client stopped.\n");

        server.stop();
        serverThread.join();
        fmt::print("Server stopped.\n");

        return res ? 0 : 1;
    } else {
        return 1;
    }
    return 0;
}
