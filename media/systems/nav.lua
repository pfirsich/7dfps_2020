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
