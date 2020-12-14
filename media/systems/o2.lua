terminalOutput([[
,88~-_     /~~88b
d888   \   |   888
88888   |  `  d88P
88888   |    d88P
Y888   /    d88P
`88_-~    d88P___


]])

logs("")

tick(4.0, function()
    log("", logLevel.INFO, ("Water Consumption: %f L/min"):format(jitter(0.12, 0.1, 0.3)))
end)

manual("o2", [[
The life support of your system
]])
