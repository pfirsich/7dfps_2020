terminalOutput([[
.__   __.      ___   ____    ____ ____    ____
|  \ |  |     /   \  \   \  /   / \   \  /   /
|   \|  |    /  ^  \  \   \/   /   \   \/   /
|  . `  |   /  /_\  \  \      /     \      /
|  |\   |  /  _____  \  \    /       \    /
|__| \__| /__/     \__\  \__/         \__/

]])

terminalOutput("System log updated. See: 'log'");
terminalOutput("");

local state = {
    name = "off",
}

logs("")

log("", logLevel.INFO, "Fueling complete")
log("", logLevel.INFO, "Ship docked to intergalactic space port Garzikulon Prime")
log("", logLevel.INFO, "System status check complete: systems nominal")
log("", logLevel.INFO, "Current destination: Veros jump gate")
log("", logLevel.INFO, "Jump not ready: Navigation computer offline")

-- update
tick(1, function()
    if state.name == 'booting' then
        state.progress = state.progress + jitter(0.14, 1);
        if state.progress < 1 then
            log("", logLevel.INFO, ("Boot Progress: %d%%"):format(state.progress * 100))
        else
            log("", logLevel.INFO, "Navigation computer booted")
            state = {
                name = "running"
            }
            return
        end
    elseif state.name == 'jumpSequence' then
        if state.engineStatus == 'unknown' then
            log("", logLevel.INFO, "Asserting engine operationality")
            send("engine", "healthCheck");
            state.engineStatus = "waiting"
            return;
        elseif state.reactorStatus == 'unknown' then
            log("", logLevel.INFO, "Asserting reactor operationality")
            send("reactor", "healthCheck");
            state.reactorStatus = "waiting"
            return
        end

        if state.engineStatus == 'err' or state.reactorStatus == 'err' then
            log("", logLevel.ERROR, "Jump aborted: one or more health checks failed")
            state = {
                name = "running"
            }
            return
        end
    end
end)

command("boot", "", {}, function()
    if state.name == "booting" then
        terminalOutput(("Boot progress: %d%%"):format(state.progress * 100))
        log("", logLevel.INFO, "Boot progress queried by user root")
        return
    end

    if state.name ~= "off" then
        terminalOutput("Navigation operational")
        return
    end

    log("", logLevel.INFO, "Booting navigation systems..")

    terminalOutput("Boot initiated")
    terminalOutput("Check 'log' to see progress")

    state = {
        name = "booting",
        progress = 0,
    }
end)

command("jump", "", {}, function()
    if state.name ~= "running" then
        terminalOutput("Jump not ready: Navigation computer offline")
        log("", logLevel.WARNING, "Jump failed: Navigation computer offline")
        return
    end

    if state.name == "running" then
        terminalOutput("Jump sequence initiated")
        terminalOutput("Check 'log' to see progress")
        log("", logLevel.INFO, "Starting jump sequence..")
        state = {
            name = "jumpSequence",
            engineStatus = 'unknown',
            reactorStatus = 'unknown',
        }
        return
    end
end)

command("longcommand", "", {"STRING"}, function(str)
    terminalOutput(("Doing %s.."):format(str))
    asleep(1)
    terminalOutput("25%")
    asleep(1)
    terminalOutput("50%")
    asleep(1)
    terminalOutput("75%")
    asleep(1)
    terminalOutput("Complete")
end)

subscribe("systemStatus", function(sender, value)
    log("", logLevel.INFO, ("Received health check result from %s"):format(sender))

    if value == "ok" then
        log("", logLevel.INFO, "Health check successful")
    else
        log("", logLevel.WARNING, "Health check failed")
    end

    if state.name == "jumpSequence" then
        if sender == "engine" then
            state.engineStatus = value
        elseif sender == "reactor" then
            state.reactorStatus = value
        end
    end
end)


-- TODO Joel: Add some shit here
-- manual("engine", [[
-- The engine is the hyperdrive of your ship and allows jumping to other systems.
-- ]])
--
-- manual("throttle", [[
-- Throttle controls power intake and possible jump distance.
-- Regular operation allows values from 0 to 1. Using a manual override allows setting the value from 0 to 2.
-- ]])
