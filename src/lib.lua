R"luastring"--(
function symrand()
    return math.random() * 2.0 - 1.0
end

function jitter(val, rel, abs)
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

logLevel = {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
}
--)luastring"--"
