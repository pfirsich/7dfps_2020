R"luastring"--(
function symrand()
    return math.random() * 2.0 - 1.0
end

function jitter(val, rel, abs)
    if abs == nil then
        abs = 0
    end
    return val + symrand() * rel * val + symrand() * abs
end

function clamp(val, lo, hi)
    return math.min(math.max(val, lo), hi)
end

function lerp(a, b, t)
    return a + t * (b - a)
end

function unlerp(val, a, b)
    local cval = clamp(val, math.min(a, b), math.max(a, b))
    return (cval - a) / (b - a)
end

function rescale(val, fromA, fromB, toA, toB)
    return lerp(toA, toB, unlerp(val, fromA, fromB))
end

function approachExp (value, target, speed)
    return value + (target - value) * speed
end

-- This is like coroutine.wrap, but if you call the returned function
-- after the coroutine's status is "dead", it will be recreated.
function cowrap(f)
    local c = coroutine.create(f)
    return function(...)
        local s = coroutine.status(c)
        if s == "dead" then
            c = coroutine.create(f)
        end
        return select(2, coroutine.resume(c, ...))
    end
end

function command(cmd, sub, args, func)
    command_(cmd, sub, args, cowrap(func))
end

function init(func)
    init_(cowrap(func))
end

function asleep(sec)
    local start = time()
    while time() - start < sec do
        coroutine.yield(true)
    end
end

function sleepLines(sec, lines)
    for i = 1, #lines do
        terminalOutput(lines[i])
        asleep(sec)
    end
end

logLevel = {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
}
--)luastring"--"
