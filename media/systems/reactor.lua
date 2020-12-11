-- constants
local alarmLevel = 0.2

-- state
-- fuel
local battery = 0.1

local coreCount = 4 * 3
local cores = {}

for i = 1, coreCount do
    cores[i] = {
        integrity = 1.0, -- drives power output
    }
end

-- config variables
powerCutoff = {}

-- fill battery
tick(1.0, function()
    print("fill battery")
    for i = 1, coreCount do
        cores[i] = {
            integrity = 1.0, -- drives power output
        }
    end
end)

-- trigger alarm
tick(5.0, function()
    print("trigger alarm")
    if battery < alarmLevel then
        setAlarm()
        log("battery", "WARNING", ("Battery level critical (%d%)"):format(math.floor(battery * 100)))
    else
        if hasAlarm() then
            log("battery", "INFO", ("Battery level normal (%d)"):format(math.floor(battery * 100)))
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
        return jitter(lerp(0.6, 1.0, cores[i].integrity), 0.1, 0.05);
    end)
    sensor(("core%d-fuel-consumption"):format(i), function()
        return jitter(lerp(1.5, 1.0, cores[i].integrity), 0.1, 0.05);
    end)
end

subscribe("requestEnergy", function(sender, amount)
    print(("request %f from %s"):format(amount, sender))
end)

manual(nil, [[default manual]])

command("power-output", "show", {}, function(args)
    print("call power-output show")
end)

command("power-cutoff", "show", {"system-name"}, function(systemName)
    --terminalOutput("Power-cutoff for %s: %f\n")
end)

command("power-cutoff", "set", {"system-name", "percentage"}, function(systemName, percentage)

end)