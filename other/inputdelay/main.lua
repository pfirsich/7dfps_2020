local inputBuffer = {}
local accum = 0
local frameInterval = 1.0/60.0
local inputDelay = 100
local pos = {x = love.graphics.getWidth() / 2, y = love.graphics.getHeight() / 2}

function love.update(dt)
    accum = accum + dt
    if accum < frameInterval then
        return
    end
    accum = accum - frameInterval

    local time = love.timer.getTime()
    table.insert(inputBuffer, {
        time = time,
        input = {
            up = love.keyboard.isDown("up") and 1 or 0,
            down = love.keyboard.isDown("down") and 1 or 0,
            left = love.keyboard.isDown("left") and 1 or 0,
            right = love.keyboard.isDown("right") and 1 or 0,
        },
    })

    local cutoff = time - inputDelay/1000
    -- only leave one input before the cutoff
    while inputBuffer[1].time < cutoff and inputBuffer[2].time < cutoff do
        table.remove(inputBuffer, 1)
    end
    -- if there is still one input that is due, pop it off and use it
    if inputBuffer[1].time < cutoff then
        input = inputBuffer[1]
        table.remove(inputBuffer, 1)

        local speed = 200
        local ud = input.input.down - input.input.up
        local lr = input.input.right - input.input.left
        pos.x = pos.x + lr * dt * speed
        pos.y = pos.y + ud * dt * speed
    end
end

function love.keypressed(key)
    if key == "+" then
        inputDelay = inputDelay + 25
    elseif key == "-" then
        inputDelay = math.max(0, inputDelay - 25)
    end
end

function love.draw()
    local lg = love.graphics
    lg.print(tostring(inputDelay), 5, 5)
    lg.print(tostring(#inputBuffer), 5, 20)
    lg.circle("fill", pos.x, pos.y, 50)
end
