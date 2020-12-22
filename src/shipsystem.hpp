#pragma once

#include <cassert>
#include <deque>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <fmt/format.h>
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "random.hpp"
#include "singleton.hpp"

namespace fs = std::filesystem;

namespace comp {
struct Terminal {
    std::string systemName;
};
}

struct ShipState {
    float engineThrottle = 0.0f;
    float reactorPower = 0.0f;

    bool operator==(const ShipState& other) const;
    bool operator!=(const ShipState& other) const;
};

template <typename T, typename V, typename Func>
std::optional<size_t> findField(const T& container, const V& fieldValue, Func&& func)
{
    for (size_t i = 0; i < container.size(); ++i)
        if (func(container[i]) == fieldValue)
            return i;
    return std::nullopt;
}

class MessageBus : public Singleton<MessageBus> {
    friend class Singleton<MessageBus>;

public:
    using MessageId = std::string;
    using EndpointId = std::string;

    struct Message {
        using Field = std::variant<std::string, float>;

        MessageId id;
        std::vector<Field> fields;
    };

    using MessageHandler = std::function<void(const EndpointId& sender, const Message&)>;

    void registerEndpoint(const EndpointId& id);
    void subscribe(const EndpointId& subscriber, const MessageId& messageId, MessageHandler func);
    void send(const EndpointId& sender, const EndpointId& destination, const Message& message);
    void broadcast(const EndpointId& sender, const Message& message);

    // To remove references of objects captured by lambdas
    void clearEndpoints();

private:
    struct Subscription {
        MessageId messageId;
        MessageHandler messageHandler;
    };

    struct Endpoint {
        EndpointId id;
        std::vector<Subscription> subscriptions;

        std::optional<size_t> findSubscription(const MessageId& messageId) const;
    };

    MessageBus() = default;

    std::optional<size_t> findEndpoint(const EndpointId& id) const;

    void send(const EndpointId& sender, const Endpoint& destination, const Message& message);

    std::vector<Endpoint> endpoints_;
};

class ShipSystem {
public:
    using Name = std::string;
    using TickFunction = std::function<void(void)>;
    using SensorId = std::string;
    using SensorValue = float;
    using SensorFunc = std::function<SensorValue(void)>;
    using CommandArg = std::variant<std::string, float>;
    using CommandFunc = std::function<bool(const std::vector<CommandArg>&)>;
    using LogId = std::string;

    enum class LogLevel { Debug = 0, Info, Warning, Error };

    static constexpr auto DefaultManual = "<empty>";

    bool alarm = false;

    ShipSystem(const Name& name);

    virtual ~ShipSystem();

    void addTick(float interval, TickFunction func);
    void update();

    void addBuiltinCommands();
    void addCommand(const std::string& command, const std::optional<std::string>& subCommand,
        const std::vector<std::string>& arguments, CommandFunc func);
    void addInternalCommand(const std::string& command, CommandFunc func);
    void executeCommand(const std::string& command);
    void executeCommand(const std::vector<std::string>& args);
    void executeInternalCommand(const std::string& command);
    bool commandRunning() const;

    void addSensor(const SensorId& id, SensorFunc func);
    std::vector<SensorId> getSensors() const;
    SensorValue getSensor(const SensorId& id);

    void addManual(const std::string& name, const std::string& text);

    void registerLog(const LogId& log);
    void log(const LogId& log, LogLevel level, std::string text);
    std::string getLogText(const LogId& id) const;

    void terminalOutput(const std::string& text);
    const std::string& getTerminalOutput() const;
    size_t getTotalTerminalOutputSize() const;
    size_t getTerminalOutputStart() const;

    const std::string& getName() const;

    // We need this, so we have the option to remove any references to objects in the lambdas
    void clearLambdas();

private:
    static std::vector<std::string> allSystems;

    struct Tick {
        std::function<void(void)> handler;
        float interval;
        float lastTick = 0.0f;
    };

    struct Command {
        struct SubCommand {
            std::string name;
            std::vector<std::string> arguments;
            CommandFunc handler;

            std::string getUsage() const;
        };
        std::vector<SubCommand> subCommands;
        std::string name;
        bool internal = false;

        std::optional<size_t> findSubCommand(const std::string& name) const;
    };

    struct Sensor {
        std::function<SensorValue(void)> func;
        SensorId id;
    };

    struct Log {
        struct Line {
            time_t time;
            LogLevel level;
            std::string text;

            Line(LogLevel level, std::string text);
        };

        std::deque<Line> lines;
        std::string id;
    };

    static bool isValidSystemName(const std::string& name);

    std::string getUsage(const Command& command, const Command::SubCommand& subCommand) const;
    bool isValidSensorName(const std::string& name) const;

    std::optional<size_t> findCommand(const std::string& name) const;
    std::optional<size_t> findSensor(const SensorId& id) const;
    std::optional<size_t> findLog(const LogId& id) const;

    std::optional<std::vector<CommandArg>> parseCommandArgs(const Command& command,
        const Command::SubCommand& subCommand, const std::vector<std::string>& args,
        size_t argsStart);
    void sensorShowCommand(const std::vector<CommandArg>& arg);
    void manCommand();
    void executeCommand(
        size_t commandIndex, size_t subCommandIndex, const std::vector<CommandArg>& args);

    static std::string_view getLogLevelString(LogLevel level);

    void truncateTerminalOutput();

    std::unordered_map<std::string, std::string> manuals_;
    std::vector<Tick> ticks_;
    std::vector<Sensor> sensors_;
    std::vector<Command> commands_;
    std::vector<Log> logs_;
    std::string terminalOutput_;
    // Tuple of commands_ and subCommands indices. Yes, I don't know wtf I am doing.
    std::optional<std::pair<size_t, size_t>> currentCommand_ = std::nullopt;
    size_t totalTerminalOutputSize_ = 0;
    size_t terminalOutputStart_ = 0;
    std::string terminalInput_;
    Name name_;
};

struct LuaShipSystem : public ShipSystem {
    static ShipState shipState;

    sol::state lua;

    LuaShipSystem(const ShipSystem::Name& name, const fs::path& scriptPath);
    ~LuaShipSystem();
};
