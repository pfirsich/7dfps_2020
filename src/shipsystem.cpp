#include "shipsystem.hpp"

#include <ctime>
#include <functional>

#include <fmt/chrono.h>

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

std::vector<std::string> ShipSystem::allSystems;

ShipSystem::ShipSystem(const Name& name)
    : name_(name)
{
    MessageBus::instance().registerEndpoint(name);
    allSystems.push_back(name);
}

ShipSystem::~ShipSystem()
{
    allSystems.erase(std::remove(allSystems.begin(), allSystems.end(), name_), allSystems.end());
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

    if (currentCommand_) {
        const auto [cmdIdx, subCmdIdx] = *currentCommand_;
        const auto resume = commands_[cmdIdx].subCommands[subCmdIdx].handler({});
        if (!resume) {
            currentCommand_ = std::nullopt;
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

bool ShipSystem::isValidSystemName(const std::string& name)
{
    return std::find(allSystems.begin(), allSystems.end(), name) != allSystems.end();
}

bool ShipSystem::isValidSensorName(const std::string& name) const
{
    for (const auto& sensor : sensors_) {
        if (sensor.id == name)
            return true;
    }
    return false;
}

std::optional<std::vector<ShipSystem::CommandArg>> ShipSystem::parseCommandArgs(
    const ShipSystem::Command& command, const ShipSystem::Command::SubCommand& subCommand,
    const std::vector<std::string>& args, size_t argsStart)
{
    if (args.size() - argsStart != subCommand.arguments.size()) {
        terminalOutput(
            fmt::format("Invalid number of arguments\nUsage: {}\n", getUsage(command, subCommand)));
        return std::nullopt;
    }
    std::vector<ShipSystem::CommandArg> parsed;
    for (size_t i = 0; i < subCommand.arguments.size(); ++i) {
        const auto& argDef = subCommand.arguments[i];
        const auto& arg = args[i + argsStart];
        if (argDef == "SENSORNAME") {
            if (!isValidSensorName(arg)) {
                terminalOutput(
                    "Invalid sensor name\nType 'sensor' to see list of available sensors\n");
                return std::nullopt;
            }
            parsed.push_back(arg);
        } else if (argDef == "SYSTEMNAME") {
            if (!isValidSystemName(arg)) {
                terminalOutput("Invalid system name\nValid system names are:\n");
                for (const auto& name : allSystems)
                    terminalOutput(name + "\n");
                return std::nullopt;
            }
            parsed.push_back(arg);
        } else if (argDef == "PERCENTAGE") {
            const auto f = parseFloat(arg);
            if (!f || f < 0.0f || f > 100.0f) {
                terminalOutput("Invalid percentage value\n");
                return std::nullopt;
            }
            parsed.push_back(*f / 100.0f);
        } else if (argDef == "STRING") {
            parsed.push_back(arg);
        } else if (argDef == "FLOAT") {
            const auto f = parseFloat(arg);
            if (!f) {
                terminalOutput("Invalid float value\n");
                return std::nullopt;
            }
            parsed.push_back(*f);
        } else {
            fmt::print(stderr, "UNKNOWN COMMAND ARGUMENT '{}'\n", argDef);
            parsed.push_back(arg);
        }
    }
    return parsed;
}

void ShipSystem::executeCommand(
    size_t commandIndex, size_t subCommandIndex, const std::vector<CommandArg>& args)
{
    const auto resume = commands_[commandIndex].subCommands[subCommandIndex].handler(args);
    if (resume) {
        currentCommand_ = std::make_pair(commandIndex, subCommandIndex);
    } else {
        currentCommand_ = std::nullopt;
    }
}

void ShipSystem::executeCommand(const std::vector<std::string>& args)
{
    if (args.empty()) {
        // do nothing
        terminalOutput("\n");
        return;
    }

    auto cmdName = toLower(args[0]);
    if (cmdName == "man")
        cmdName = "manual";

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
        executeCommand(*idx, *noSubIdx, {});
        return;
    }

    const auto& subName = toLower(args[1]);
    const auto subIdx = command.findSubCommand(subName);
    if (!subIdx) {
        if (noSubIdx) {
            const auto& subCommand = command.subCommands[*noSubIdx];
            const auto parsed = parseCommandArgs(command, subCommand, args, 1);
            if (!parsed)
                return;
            executeCommand(*idx, *noSubIdx, *parsed);
            return;
        } else {
            terminalOutput("Available sub commands:\n");
            for (const auto& sub : command.subCommands) {
                terminalOutput(fmt::format("{} {}\n", command.name, sub.name));
            }
            return;
        }
    }
    const auto& subCommand = command.subCommands[*subIdx];
    const auto parsed = parseCommandArgs(command, subCommand, args, 2);
    if (!parsed)
        return;
    executeCommand(*idx, *subIdx, *parsed);
}

void ShipSystem::executeCommand(const std::string& command)
{
    terminalOutput(fmt::format("root@{}:~# {}\n", name_, command));
    executeCommand(split(command));
}

bool ShipSystem::commandRunning() const
{
    return currentCommand_.has_value();
}

void ShipSystem::sensorShowCommand(const std::vector<CommandArg>& args)
{
    if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
        terminalOutput("Usage: sensor show SENSORNAME\n");
        return;
    }
    const auto& name = std::get<std::string>(args[0]);
    const auto idx = findSensor(name);
    if (!idx) {
        terminalOutput(fmt::format("Unknown sensor '{}'\n", name));
        assert(false);
    }
    terminalOutput(fmt::format("{}\n", sensors_[*idx].func()));
}

void ShipSystem::manCommand()
{
    terminalOutput("Available commands:\n");
    for (const auto& command : commands_) {
        for (const auto& subCommand : command.subCommands) {
            terminalOutput(" * " + getUsage(command, subCommand));
        }
    }
}

void ShipSystem::addBuiltinCommands()
{
    // The refs here are dangerous, but it is what it is
    for (const auto& log : logs_) {
        addCommand("log", log.id, {}, [this, &log](const std::vector<CommandArg>&) {
            terminalOutput(getLogText(log.id));
            return false;
        });
    }

    addCommand("manual", "", {}, [this](const std::vector<CommandArg>&) {
        manCommand();
        return false;
    });
    for (const auto& entry : manuals_) {
        const auto& manual = entry.second;
        addCommand("manual", entry.first, {}, [this, &manual](const std::vector<CommandArg>&) {
            terminalOutput(manual);
            return false;
        });
    }

    addCommand("sensor", "list", {}, [this](const std::vector<CommandArg>&) {
        for (const auto& sensor : sensors_) {
            terminalOutput(sensor.id);
        }
        return false;
    });
    addCommand("sensor", "show", { "SENSORNAME" }, [this](const std::vector<CommandArg>& args) {
        sensorShowCommand(args);
        return false;
    });
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

void ShipSystem::addManual(const std::string& name, const std::string& text)
{
    manuals_.emplace(name, text);
}

void ShipSystem::registerLog(const LogId& id)
{
    const auto idx = findLog(id);
    if (idx) {
        fmt::print(stderr, "Log '{}' already registered\n", id);
        assert(false);
        return;
    }
    logs_.push_back(Log { {}, id });
}

void ShipSystem::log(const LogId& id, LogLevel level, std::string text)
{
    auto idx = findLog(id);
    if (!idx) {
        fmt::print(stderr, "Log '{}' is not registered\n", id);
        assert(false);
        return;
    }
    auto& log = logs_[*idx];
    log.lines.push_back(Log::Line { level, std::move(text) });
    while (log.lines.size() > maxLogLines)
        log.lines.pop_front();
}

std::string_view ShipSystem::getLogLevelString(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Error:
        return "ERROR";
    default:
        return "POOP";
    }
}

std::string ShipSystem::getLogText(const LogId& id) const
{
    auto idx = findLog(id);
    if (!idx) {
        fmt::print(stderr, "Log '{}' is not registered\n", id);
        assert(false);
        return "";
    }

    std::string text;
    text.reserve(16 * 1024);
    for (const auto& line : logs_[*idx].lines) {
        auto tm = *std::localtime(&line.time);
        tm.tm_year += 1000;
        text.append(fmt::format(
            "[{:%Y-%m-%d %H:%M:%S}] [{}] {}\n", tm, getLogLevelString(line.level), line.text));
    }
    return text;
}

void ShipSystem::terminalOutput(const std::string& text)
{
    const auto beforeSize = terminalOutput_.size();
    terminalOutput_.append(text);
    if (text.back() != '\n')
        terminalOutput_.push_back('\n');
    totalTerminalOutputSize_ += terminalOutput_.size() - beforeSize;
    truncateTerminalOutput();
}

const std::string& ShipSystem::getTerminalOutput() const
{
    return terminalOutput_;
}

size_t ShipSystem::getTotalTerminalOutputSize() const
{
    return totalTerminalOutputSize_;
}

size_t ShipSystem::getTerminalOutputStart() const
{
    return terminalOutputStart_;
}

std::string ShipSystem::getUsage(
    const ShipSystem::Command& command, const ShipSystem::Command::SubCommand& subCommand) const
{
    std::string args = "";
    for (const auto& arg : subCommand.arguments) {
        args.push_back(' ');
        args.append(arg);
    }
    return fmt::format("{} {}{}", command.name, subCommand.name, args);
}

std::optional<size_t> ShipSystem::Command::findSubCommand(const std::string& name) const
{
    return findField(subCommands, name, [](const auto& cmd) { return cmd.name; });
}

ShipSystem::Log::Line::Line(ShipSystem::LogLevel level, std::string text)
    : time(std::time(nullptr))
    , level(level)
    , text(std::move(text))
{
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
        while (count < terminalOutput_.size() && terminalOutput_[count - 1] != '\n')
            count++;
        terminalInput_.erase(0, count);
        terminalOutputStart_ += count;
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
    fmt::print(stderr, "sol3 Exception: {}\n", description);
    return sol::stack::push(L, description);
}

sol::protected_function_result&& checkError(sol::protected_function_result&& res)
{
    if (!res.valid()) {
        fmt::print(stderr, "Lua Error: {}\n", res.get<sol::error>().what());
        assert(false);
    }
    return std::move(res);
}

MessageBus::Message getMessage(const std::string& messageId, sol::variadic_args va)
{
    MessageBus::Message msg { messageId, {} };
    for (auto v : va) {
        if (v.is<float>())
            msg.fields.push_back(v.as<float>());
        else if (v.is<std::string>())
            msg.fields.push_back(v.as<std::string>());
    }
    return msg;
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

    lua["tick"].set_function([this](float interval, sol::function func) {
        addTick(interval, [func]() { checkError(func()); });
    });
    lua["sensor"].set_function([this](const std::string& sensorId, sol::function func) {
        addSensor(sensorId, [func]() { return checkError(func()).get<float>(); });
    });
    lua["subscribe"].set_function([this](const std::string& messageId, sol::function func) {
        MessageBus::instance().subscribe(getName(), messageId,
            [func](const std::string& sender, const MessageBus::Message& msg) {
                checkError(func(sender, sol::as_args(msg.fields)));
            });
    });
    lua["send"].set_function([this](const std::string& destination, const std::string& messageId,
                                 sol::variadic_args va) {
        MessageBus::instance().send(getName(), destination, getMessage(messageId, va));
    });
    lua["broadcast"].set_function([this](const std::string& messageId, sol::variadic_args va) {
        MessageBus::instance().broadcast(getName(), getMessage(messageId, va));
    });
    lua["manual"].set_function(
        [this](const std::string& name, const std::string& text) { addManual(name, text); });
    lua["command_"].set_function(
        [this](const std::string& command, const std::optional<std::string>& subCommand,
            sol::table arguments, sol::function func) {
            std::vector<std::string> args;
            for (size_t i = 0; i < arguments.size(); ++i) {
                args.push_back(arguments[i + 1]);
            }
            addCommand(command, subCommand, args, [func](const std::vector<CommandArg>& args) {
                const auto res = checkError(func(sol::as_args(args)));
                return res.get_type() == sol::type::boolean && res.get<bool>() == true;
            });
        });
    lua["terminalOutput"].set_function([this](const std::string& text) { terminalOutput(text); });
    lua["setAlarm"].set_function([this]() { alarm = true; });
    lua["hasAlarm"].set_function([this]() { return alarm; });
    lua["clearAlarm"].set_function([this]() { alarm = false; });
    lua["logs"].set_function([this](sol::variadic_args va) {
        for (auto v : va)
            registerLog(v.as<std::string>());
    });
    lua["log"].set_function([this](const std::string& logId, int level, const std::string& text) {
        log(logId, static_cast<LogLevel>(level), text);
    });
    lua["time"].set_function([]() { return glwx::getTime(); });

    lua.script_file(scriptPath.u8string());
    addBuiltinCommands();
    terminalOutput("Logged in as root.");
    terminalOutput("Type 'manual' to see available commands");
}

LuaShipSystem::~LuaShipSystem()
{
    clearLambdas();
}
