terminalOutput([[
__    __         .__   __.   _______  __   __.
|  |  |  |        |  \ |  |  /  _____||  \ |  |
|  |__|  |  ______|   \|  | |  |  __  |   \|  |
|   __   | |______|  . `  | |  | |_ | |  . `  |
|  |  |  |        |  |\   | |  |__| | |  |\   |
|__|  |__|        |__| \__|  \______| |__| \__|

]])

-- consts
local temperatureMaxFine = 1200
local powerUsageMax = 2000
local powerUsageMin = 50

-- state
local temperature = 26
local throttle = 0
local powerBalance = 0

local state = {
    name = "off",
}

logs("")

-- update
tick(1.0, function()
    if state.name == 'booting' then
        state.progress = state.progress + 0.03
        if state.progress < 1 then
            log("", logLevel.INFO, ("Boot Progress: %d%%"):format(state.progress * 100))
        else
            log("", logLevel.INFO, "Engine booted")
            throttle = 0.2
            log("", logLevel.INFO, ("Throttle set to: %d%%"):format(throttle * 100))
            state = {
                name = "running"
            }
            return
        end
    elseif state.name == "running" then
        local tempTarget = rescale(throttle, 0, 1, 800, temperatureMaxFine)
        temperature = approachExp(temperature, tempTarget, 0.3)

        if temperature > temperatureMaxFine * 1.2 then
            log("", logLevel.ERROR, "Engine over-heated")
            state = {
                name = "off"
            }
            return
        end

        local powerUsage = lerp(powerUsageMin, powerUsageMax, throttle);
        send("reactor", "requestEnergy", powerUsage)
        log("", logLevel.INFO, ("Request power: %f %f"):format(powerBalance, powerUsage))
        powerBalance = powerBalance - powerUsage

        if powerBalance < powerUsage * 4 then
            log("", logLevel.WARNING, "Power critically low")
        elseif powerBalance < powerUsage * 2 then
            log("", logLevel.WARNING, "Low power")
        end

        -- after a couple seconds
        if powerBalance < powerUsage * 6 then
            log("", logLevel.ERROR, "Engine shutdown")
            state = {
                name = "off"
            }
            powerBalance = 0
            return
        end
    end
end)

-- trigger alarm
tick(5.0, function()
    if temperature > temperatureMaxFine then
        setAlarm()
        log("", logLevel.WARNING, ("Temperature critical: %d%%"):format(temperature * 100))
    else
        if hasAlarm() then
            log("", logLevel.INFO, ("Temperature nominal: %d%%"):format(temperature * 100))
        end
        clearAlarm()
    end
end)

sensor("temperature", function()
    return jitter(temperature, 0.02, 1)
end)

command("boot", "", {}, function()
    if state.name == "booting" then
        terminalOutput(("Boot progress: %d%%"):format(state.progress * 100))
        log("", logLevel.INFO, "Boot progress queried by user root")
        return
    end

    if state.name ~= "off" then
        terminalOutput("Boot completed")
        return
    end

    log("", logLevel.INFO, "Booting engine..")

    terminalOutput("Boot initiated")
    terminalOutput("Check logs to see progress")

    state = {
        name = "booting",
        progress = 0,
    }
end)

command("throttle", "set", {"PERCENTAGE"}, function(percentage)
    if state.name ~= "running" then
        terminalOutput("Engine is not running")
        log("", logLevel.WARNING, ("Failed attempt to set throttle to: %d%%"):format(percentage * 100))
        return
    end

    log("", logLevel.INFO, ("Throttle set to: %d%%"):format(percentage * 100))
    throttle = percentage
end)

command("throttle", "show", {}, function()
    terminalOutput(("Throttle level: %d%%"):format(throttle * 100))
    log("", logLevel.INO, "Throttle progress queried by user root")
end)

command("throttle", "override", {"FLOAT"}, function(percentage)
    if percentage < 0 or percentage > 2 then
        terminalOutput("Invalid value")
        return
    end

    if state.name ~= "running" then
        terminalOutput("Engine is not running")
        log("", logLevel.WARNING, ("Failed attempt to set throttle to: %d%%"):format(percentage * 100))
    end

    log("", logLevel.INFO, ("Throttle set to: %d%%"):format(percentage * 100))
    throttle = percentage
end)

subscribe("provideEnergy", function(sender, amount)
    log("", logLevel.INFO, ("%s provided %f KW/S"):format(sender, amount))
    powerBalance = powerBalance + amount
end)

subscribe("healthCheck", function(sender)
    log("", logLevel.INFO, ("Health check triggered by '%s'"):format(sender))

    if state.name == 'running' and throttle > 0.2 then
        log("", logLevel.INFO, "Health check successful")
        send(sender, "systemStatus", "ok")
    else
        log("", logLevel.INFO, "Health check failed")
        send(sender, "systemStatus", "err")
    end
end)

-- joel: add details
manual("engine", [[
The engine is the hyperdrive of your ship and allows jumping to other systems.

For the engine health check to succeed the engine must be online and the throttle not below 20%.
]])

-- joel: details
manual("throttle", [[
Throttle controls power intake and possible jump distance.
Regular operation allows values from 0 to 1. Using a manual override allows setting the value from 0 to 2.
]])
