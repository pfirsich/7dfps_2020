init(function()
    sleepLines(initSleep, {
        [[,88~-_     /~~88b]],
        [[d888   \   |   888]],
        [[88888   |  `  d88P]],
        [[88888   |    d88P]],
        [[Y888   /    d88P]],
        [[`88_-~    d88P___]],
        "Logged in as root.",
        "Type 'manual' to see available commands",
    })
end)

logs("")

tick(8.0, function()
    log("", logLevel.INFO, ("Water Consumption: %f L/min"):format(jitter(0.12, 0.1, 0.3)))
end)

tick(6.0, function()
    log("", logLevel.INFO, ("Water Collection: %f L/min"):format(jitter(0.12, 0.1, 0.3)))
end)

manual("o2", [[
The life support of your system
]])
