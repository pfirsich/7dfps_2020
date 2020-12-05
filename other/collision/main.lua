local lg = love.graphics
local lk = love.keyboard

local winW, winH = love.graphics.getDimensions()

local rad = 40
local colliders = {
    -- player
    {type = "circle", x = winW/2, y = winH/2, r = rad},

    -- level
    {type = "circle", x = 600, y = 450, r = rad},
    -- x, y for rectangle is the center and w, h are half-extents
    {type = "rectangle", x = 225, y = 125, w = 75, h = 125},
    {type = "rectangle", x = 350, y = 75, w = 200, h = 75},
    {type = "rectangle", x = 650, y = 75, w = 200, h = 75},
    {type = "rectangle", x = 225, y = 375, w = 75, h = 125},
    {type = "rectangle", x = 150, y = 425, w = 150, h = 75},
}

local player = {
    collider = colliders[1],
    vel = {x = 0, y = 0},
    accell = 1000,
    maxSpeed = 250,
    friction = 1000,
}

local function clamp(x, lo, hi)
    return math.min(math.max(x, lo), hi)
end

local function intersectCircleRect(circle, rect)
    -- clamped (into rect) circle position
    local cx = clamp(circle.x, rect.x - rect.w, rect.x + rect.w)
    local cy = clamp(circle.y, rect.y - rect.h, rect.y + rect.h)

    local relX = circle.x - cx
    local relY = circle.y - cy
    local relLenSqr = relX * relX + relY * relY
    if relLenSqr >= circle.r * circle.r then
        return nil
    end
    local relLen = math.sqrt(relLenSqr)
    local relNormX = relX / relLen
    local relNormY = relY / relLen

    local penDepth = circle.r - relLen
    -- it's possible that the condition above is not true, but penDepth is still 0
    if penDepth == 0 then
        return nil
    end

    return {
        penDepth = penDepth,
        normal = {x = relNormX, y = relNormY},
    }
end

local function intersectCircleCircle(circle, other)
    local relX = circle.x - other.x
    local relY = circle.y - other.y
    local distSqr = relX * relX + relY * relY
    local radiusSum = circle.r + other.r
    if distSqr >= radiusSum * radiusSum then
        return nil
    end
    local dist = math.sqrt(distSqr)
    local relNormX = relX / dist
    local relNormY = relY / dist

    local penDepth = radiusSum - dist
    if penDepth == 0 then
        return nil
    end

    return {
        penDepth = penDepth,
        normal = {x = relNormX, y = relNormY},
    }
end

local intersect = {
    rectangle = intersectCircleRect,
    circle = intersectCircleCircle,
}

local function getAllCollisions(collider)
    assert(collider.type == "circle")
    local collisions = {}
    for i, other in ipairs(colliders) do
        if collider ~= other then
            local coll = intersect[other.type](collider, other)
            if coll then
                table.insert(collisions, coll)
            end
        end
    end
    return collisions
end

local function move(collider, dx, dy)
    assert(collider.type == "circle")
    local startX = collider.x
    local startY = collider.y
    collider.x = collider.x + dx
    collider.y = collider.y + dy

    local maxCollisionCount = 10
    local collisionCount = 0
    while collisionCount < maxCollisionCount do
        local collisions = getAllCollisions(collider)
        if #collisions == 0 then
            return
        end

        -- We guess the first collision (in time) as the one with the maximum penetration depth
        local maxDepth = 0
        local maxDepthIndex = nil
        for i, c in ipairs(collisions) do
            if c.penDepth > maxDepth then
                maxDepth = c.penDepth
                maxDepthIndex = i
            end
        end

        assert(maxDepthIndex)
        local coll = collisions[maxDepthIndex]
        collider.x = collider.x + coll.normal.x * coll.penDepth
        collider.y = collider.y + coll.normal.y * coll.penDepth
        collisionCount = collisionCount + 1

        -- project velocity (remove normal component)
        -- v = a * normal + b * tangent
        local tangentX = -coll.normal.y
        local tangentY = coll.normal.x
        -- v . tangent = b * |tangent|^2 = b
        local dot = dx * tangentX + dy * tangentY
        dx = dot * tangentX
        dy = dot * tangentY
    end
end

local function accelAxis(axis, accell, dt)
    if math.abs(accell) > 0 then
        -- increase when different signs
        local factor = player.vel[axis] * accell < 0.0 and 2.0 or 1.0
        player.vel[axis] = player.vel[axis] + accell * factor * player.accell * dt
    else
        if player.vel[axis] > 0.0 then
            player.vel[axis] = math.max(0, player.vel[axis] - player.friction * dt)
        elseif player.vel[axis] < 0.0 then
            player.vel[axis] = math.min(0, player.vel[axis] + player.friction * dt)
        end
    end
end

function love.update(dt)
    local key = function(k) return lk.isDown(k) and 1 or 0 end
    local lr = key("right") - key("left")
    local ud = key("down") - key("up")
    accelAxis("x", lr, dt)
    accelAxis("y", ud, dt)
    local speed = math.sqrt(player.vel.x * player.vel.x + player.vel.y * player.vel.y)
    if speed > player.maxSpeed then
        local scale = player.maxSpeed / speed
        player.vel.x = player.vel.x * scale
        player.vel.y = player.vel.y * scale
    end
    move(player.collider, player.vel.x * dt, player.vel.y * dt)
end

function love.draw()
    local keys = {}
    for _, key in ipairs({"up", "left", "down", "right"}) do
        if lk.isDown(key) then
            table.insert(keys, key)
        end
    end
    lg.setColor(1, 1, 1)
    lg.print(table.concat(keys, ", "), 5, 5)
    for i, collider in ipairs(colliders) do
        local mode = "line"
        if i == 1 then
            lg.setColor(0, 0, 1)
            mode = "fill"
        else
            lg.setColor(1, 0, 0)
        end
        if collider.type == "rectangle" then
            lg.rectangle("line", collider.x - collider.w, collider.y - collider.h,
                collider.w * 2, collider.h * 2)
        elseif collider.type == "circle" then
            lg.circle(mode, collider.x, collider.y, collider.r)
        end
    end
end
