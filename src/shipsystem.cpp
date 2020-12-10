#include "shipsystem.hpp"

#include <functional>

#include <glwx.hpp>

#include "constants.hpp"
#include "util.hpp"

void MessageBus::registerEndpoint(const EndpointId& id)
{
    assert(!findEndpoint(id));
    endpoints_.emplace_back(Endpoint { id, {} });
}

void MessageBus::subscribe(
    const EndpointId& subscriber, const MessageId& messageId, MessageHandler func)
{
    const auto idx = findEndpoint(subscriber);
    if (!idx) {
        fmt::print(stderr, "Unknown endpoint '{}'\n", subscriber);
        assert(false);
        return;
    }
    endpoints_[*idx].subscriptions.emplace_back(Subscription { messageId, std::move(func) });
}

void MessageBus::send(
    const EndpointId& sender, const EndpointId& destination, const Message& message)
{
    if (!findEndpoint(sender)) {
        fmt::print(stderr, "Unkown endpoint '{}' for sender\n", sender);
        assert(false);
        return;
    }
    const auto idx = findEndpoint(destination);
    if (!idx) {
        fmt::print(stderr, "Unknown endpoint '{}'\n", destination);
        assert(false);
        return;
    }
    send(sender, endpoints_[*idx], message);
}

void MessageBus::broadcast(const EndpointId& sender, const Message& message)
{
    if (!findEndpoint(sender)) {
        fmt::print(stderr, "Unkown endpoint '{}' for sender\n", sender);
        assert(false);
        return;
    }
    for (const auto& ep : endpoints_) {
        const auto idx = ep.findSubscription(message.id);
        if (idx)
            send(sender, endpoints_[*idx], message);
    }
}

void MessageBus::clearEndpoints()
{
    endpoints_.clear();
}

std::optional<size_t> MessageBus::Endpoint::findSubscription(const MessageId& messageId) const
{
    return findField(subscriptions, messageId, [](const auto& sub) { return sub.messageId; });
}

std::optional<size_t> MessageBus::findEndpoint(const EndpointId& id) const
{
    return findField(endpoints_, id, [](const auto& ep) { return ep.id; });
}

void MessageBus::send(const EndpointId& sender, const Endpoint& destination, const Message& message)
{
    if (!findEndpoint(sender)) {
        fmt::print(stderr, "Unkown endpoint '{}' for sender\n", sender);
        assert(false);
        return;
    }
    const auto idx = destination.findSubscription(message.id);
    if (!idx) {
        fmt::print(stderr, "Endpoint '{}' is not subscribed to '{}'\n", destination.id, message.id);
        assert(false);
        return;
    }
    destination.subscriptions[*idx].messageHandler(sender, message);
}

ShipSystem::ShipSystem(const Name& name)
    : name_(name)
{
    MessageBus::instance().registerEndpoint(name);
}

ShipSystem::~ShipSystem()
{
}

void ShipSystem::addTick(float interval, TickFunction func)
{
    // jitter so they don't step in sync
    ticks_.emplace_back(Tick { std::move(func), interval, rand<float>() });
}

void ShipSystem::update()
{
    const auto now = glwx::getTime();
    for (auto& tick : ticks_) {
        if (tick.lastTick + tick.interval <= now) {
            tick.handler();
            tick.lastTick = now;
        }
    }
}

void ShipSystem::addCommand(const std::string& name, const std::optional<std::string>& subCommand,
    const std::vector<std::string>& arguments, CommandFunc func)
{
    auto idx = findCommand(name);
    if (!idx) {
        commands_.push_back(Command { {}, name });
        idx = commands_.size() - 1;
    }
    auto& command = commands_[*idx];
    const auto& subName = subCommand.value_or("");
    const auto sidx = command.findSubCommand(subName);
    if (sidx) {
        fmt::print(stderr, "Command '{} {}' already registered.\n", name, subName);
        assert(false);
        return;
    }
    command.subCommands.push_back(Command::SubCommand { subName, arguments, func });
}

void ShipSystem::executeCommand(const std::vector<std::string>& args)
{
    if (args.empty()) {
        // do nothing
        terminalOutput("\n");
        return;
    }

    const auto& cmdName = toLower(args[0]);

    /*if (cmdName == "man" || cmdName == "manual") {
        executeManCommand(args);
        return;
    } else if (cmdName == "sensor") {
        executeSensorCommand(args);
        return;
    } else if (cmdName == "")*/

    const auto idx = findCommand(cmdName);
    if (!idx) {
        terminalOutput(fmt::format("Command '{}' not found.\nTry: manual\n", args[0]));
        return;
    }
    auto& command = commands_[*idx];

    const auto noSubIdx = command.findSubCommand("");

    if (args.size() == 1) {
        if (!noSubIdx) {
            terminalOutput("Available sub commands:\n");
            for (const auto& sub : command.subCommands) {
                terminalOutput(fmt::format("{} {}\n", command.name, sub.name));
            }
            return;
        }
        command.subCommands[*noSubIdx].handler({});
        return;
    }

    const auto& subName = toLower(args[1]);
    const auto subIdx = command.findSubCommand(subName);
    if (!subIdx) {
        if (noSubIdx) {
            command.subCommands[*noSubIdx].handler({});
            return;
        } else {
            terminalOutput("Available sub commands:\n");
            for (const auto& sub : command.subCommands) {
                terminalOutput(fmt::format("{} {}\n", command.name, sub.name));
            }
            return;
        }
    }
    command.subCommands[*subIdx].handler({});
}

void ShipSystem::executeCommand(const std::string& command)
{
    executeCommand(split(command));
}

void ShipSystem::addSensor(const ShipSystem::SensorId& id, ShipSystem::SensorFunc func)
{
    const auto idx = findSensor(id);
    if (idx) {
        fmt::print(stderr, "Sensor '{}' already exists\n", id);
        assert(false);
        return;
    }
    sensors_.emplace_back(Sensor { std::move(func), id });
}

std::vector<ShipSystem::SensorId> ShipSystem::getSensors() const
{
    std::vector<SensorId> sensors;
    sensors.reserve(sensors_.size());
    for (const auto& sensor : sensors_)
        sensors.emplace_back(sensor.id);
    return sensors;
}

ShipSystem::SensorValue ShipSystem::getSensor(const SensorId& id)
{
    const auto idx = findSensor(id);
    if (!idx) {
        fmt::print(stderr, "Unknown sensor '{}'\n", id);
        assert(false);
        return SensorValue {};
    }
    return sensors_[*idx].func();
}

void ShipSystem::addManual(const std::optional<std::string>& name, const std::string& text)
{
    manuals_.emplace(name ? *name : DefaultManual, text);
}

void ShipSystem::log(const LogId& id, LogLevel level, std::string text)
{
    auto idx = findLog(id);
    if (!idx) {
        logs_.push_back(Log { {}, id });
        idx = logs_.size() - 1;
    }
    auto& log = logs_[*idx];
    log.lines.push_back(Log::Line { level, std::move(text) });
    while (log.lines.size() > maxLogLines)
        log.lines.pop_front();
}

void ShipSystem::terminalOutput(const std::string& text)
{
    terminalOutput_.append(text);
    truncateTerminalOutput();
}

std::optional<size_t> ShipSystem::Command::findSubCommand(const std::string& name) const
{
    return findField(subCommands, name, [](const auto& cmd) { return cmd.name; });
}

std::optional<size_t> ShipSystem::findCommand(const std::string& name) const
{
    return findField(commands_, name, [](const auto& cmd) { return cmd.name; });
}

std::optional<size_t> ShipSystem::findSensor(const SensorId& id) const
{
    return findField(sensors_, id, [](const auto& sens) { return sens.id; });
}

std::optional<size_t> ShipSystem::findLog(const LogId& id) const
{
    return findField(logs_, id, [](const auto& log) { return log.id; });
}

void ShipSystem::truncateTerminalOutput()
{
    if (terminalOutput_.size() > maxTerminalOutputSize) {
        auto count = terminalTruncateAmount;
        while (count < terminalOutput_.size() && terminalOutput_[count] != '\n')
            count++;
        terminalInput_.erase(0, count + 1);
    }
}

const std::string& ShipSystem::getName() const
{
    return name_;
}

void ShipSystem::clearLambdas()
{
    ticks_.clear();
    sensors_.clear();
    commands_.clear();
}

constexpr auto luaLib =
#include "lib.lua"
    ;

namespace {
int solExceptionHandler(lua_State* L, sol::optional<const std::exception&> /*maybeException*/,
    sol::string_view description)
{
    fmt::print(stderr, "YO SOL Exception: {}\n", description);
    return sol::stack::push(L, description);
}
}

LuaShipSystem::LuaShipSystem(const ShipSystem::Name& name, const fs::path& scriptPath)
    : ShipSystem(name)
{
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::coroutine, sol::lib::string,
        sol::lib::os, sol::lib::math, sol::lib::table, sol::lib::bit32, sol::lib::io, sol::lib::ffi,
        sol::lib::jit, sol::lib::utf8);
    lua.set_exception_handler(&solExceptionHandler);

    lua.script(luaLib);

    lua["tick"].set_function(
        [this](float interval, sol::function func) { addTick(interval, [func]() { func(); }); });
    lua["sensor"].set_function([this](const std::string& sensorId, sol::function func) {
        addSensor(sensorId, [func]() {
            const float val = func();
            return val;
        });
    });
    lua["subscribe"].set_function([this](const std::string& messageId, sol::function func) {
        MessageBus::instance().subscribe(getName(), messageId,
            [func](const std::string& sender, const MessageBus::Message& msg) {
                func(sender, sol::as_args(msg.fields));
            });
    });
    lua["manual"].set_function([this](const std::optional<std::string>& name,
                                   const std::string& text) { addManual(name, text); });
    lua["command"].set_function(
        [this](const std::string& command, const std::optional<std::string>& subCommand,
            sol::table arguments, sol::function func) {
            std::vector<std::string> args;
            for (size_t i = 0; i < arguments.size(); ++i) {
                args.push_back(arguments[i + 1]);
            }
            addCommand(command, subCommand, args,
                [func](const std::vector<CommandArg>& args) { func(sol::as_args(args)); });
        });

    lua.script_file(scriptPath.u8string());
}

LuaShipSystem::~LuaShipSystem()
{
    clearLambdas();
}
