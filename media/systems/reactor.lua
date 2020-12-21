init(function()
    sleepLines(initSleep, {
        [[   ___          _                                  _____      ___           __]],
        [[  / __\   _ ___(_) ___  _ __       /\/\     __   _|___ /     / / |_ _ __ ___\ \]],
        [[ / _\| | | / __| |/ _ \| '_ \     /    \    \ \ / / |_ \    | || __| '_ ` _ \| |]],
        [[/ /  | |_| \__ \ | (_) | | | |   / /\/\ \    \ V / ___) |   | || |_| | | | | | |]],
        [[\/    \__,_|___/_|\___/|_| |_|   \/    \/     \_/ |____/    | | \__|_| |_| |_| |]],
        [[                                                             \_\            /_/]],
        "Logged in as root.",
        "Type 'manual' to see available commands",
    })
end)

-- constants
local alarmLevel = 0.2

-- state
-- fuel
local battery = 0.1

local coreCount = 2 * 2
local cores = {}

for i = 1, coreCount do
    cores[i] = {
        integrity = 1.0, -- drives power output
    }
end

-- config variables
powerCutoff = {}

-- helper
local function getCorePowerOutput(core)
    return jitter(lerp(0.6, 1.0, cores[core].integrity), 0.1, 0.05)
end

local function getCoreFuelConsumption(core)
    return jitter(lerp(1.5, 1.0, cores[core].integrity), 0.1, 0.05)
end

logs("")

-- fill battery
tick(1.0, function()
    for i = 1, coreCount do
        cores[i] = {
            integrity = 1.0, -- drives power output
        }
    end
end)

-- trigger alarm
tick(5.0, function()
    if battery < alarmLevel then
        setAlarm()
        log("", logLevel.WARNING, ("Battery level critical (%d%%)"):format(battery * 100))
    else
        if hasAlarm() then
            log("", logLevel.INFO, ("Battery level normal (%d%%)"):format(battery * 100))
        end
        clearAlarm()
    end
end)

sensor("battery-level", function()
    return battery
end)

for i = 1, coreCount do
    sensor(("core%d-integrity"):format(i), function()
        return cores[i].integrity
    end)
    sensor(("core%d-power-output"):format(i), function()
        return getCorePowerOutput(i)
    end)
    sensor(("core%d-fuel-consumption"):format(i), function()
        return getCoreFuelConsumption(i)
    end)
end

subscribe("requestEnergy", function(sender, amount)
    log("", logLevel.INFO, ("%s requested %f KW/S"):format(sender, amount))
    -- TODO: drain battery
    send(sender, "provideEnergy", amount)
end)

command("power-output", "show", {}, function()
    local total = 0
    for i = 1, coreCount do
        total = total + getCorePowerOutput(i)
    end
    terminalOutput(("Total power output: %f"):format(total))
end)

command("power-cutoff", "show", {"SYSTEMNAME"}, function(systemName)
    --terminalOutput("Power-cutoff for %s: %f\n")
end)

command("power-cutoff", "set", {"SYSTEMNAME", "PERCENTAGE"}, function(systemName, percentage)

end)

manual("reactor", [[
The reactor SR-388 is made up a grid of fusion cells.
]])
