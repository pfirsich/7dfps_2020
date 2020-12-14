terminalOutput([[
.__   __.      ___   ____    ____ ____    ____
|  \ |  |     /   \  \   \  /   / \   \  /   /
|   \|  |    /  ^  \  \   \/   /   \   \/   /
|  . `  |   /  /_\  \  \      /     \      /
|  |\   |  /  _____  \  \    /       \    /
|__| \__| /__/     \__\  \__/         \__/

]])

local state = {
    name = "off",
}

logs("")

log("", logLevel.INFO, "Fuling complete")
log("", logLevel.INFO, "Ship detached from omega station")
log("", logLevel.INFO, "All systems in stand by")
log("", logLevel.INFO, "System status check complete: systems nominal")
log("", logLevel.INFO, "Current destiation: Veros ")

-- update
tick(1.0, function()
    if state.name == 'booting' then
        state.progress = state.progress + 0.025
        if state.progress < 1 then
            log("", logLevel.INFO, ("Boot Progress: %d%%"):format(state.progress * 100))
        else
            log("", logLevel.INFO, "Nav booted")
            state = {
                name = "running"
            }
            log("", logLevel.INFO, "Ship overview:")
            log("", logLevel.INFO, "* Engine: off")
            log("", logLevel.INFO, "* Reactor: off")
            log("", logLevel.INFO, "* Shields: off")
            return
        end
    end
end)

command("boot", "", {}, function()
    if state.name == "booting" then
        terminalOutput(("Boot progress: %d%%"):format(state.progress * 100))
        log("", logLevel.INFO, "Nav boot progress queried by user root")
        return
    end

    if state.name ~= "off" then
        terminalOutput("Navigation operational")
        return
    end

    log("", logLevel.INFO, "Booting navigation systems..")

    terminalOutput("Boot initiated")
    terminalOutput("Check logs to see progress")

    state = {
        name = "booting",
        progress = 0,
    }
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
