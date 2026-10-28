local dir = "C:/Users/omega/Documents/Default Project/Alien3-TheGun-Recomp/build/ref/mame_dump/"
local frame = 0

local function dump_region(name, region, size)
    local f = io.open(dir .. name, "wb")
    for a = 0, size - 1, 4096 do
        local n = math.min(4096, size - a)
        local bytes = {}
        for i = 0, n - 1 do
            bytes[#bytes + 1] = string.char(region:read_u8(a + i))
        end
        f:write(table.concat(bytes))
    end
    f:close()
end

emu.register_frame_done(function()
    frame = frame + 1
    if frame == 240 then
        for name, region in pairs(manager.machine.memory.regions) do
            local f = io.open(dir .. "regions.txt", "a")
            f:write(string.format("%s %d\n", name, region.size))
            f:close()
            if name == ":mainpcb:tiles" then dump_region("tiles_region.bin", region, region.size) end
            if name == ":mainpcb:sprites" then dump_region("sprites_region.bin", region, region.size) end
        end
    end
end)
