local dir = "C:/Users/omega/Documents/Default Project/Alien3-TheGun-Recomp/build/ref/mame_nodump/"
local frame = 0

local function dump(name, sp, first, size)
    local f = io.open(dir .. name, "wb")
    for a = first, first + size - 1, 4096 do
        local n = math.min(4096, first + size - a)
        local bytes = {}
        for i = 0, n - 1 do bytes[#bytes + 1] = string.char(sp:read_u8(a + i)) end
        f:write(table.concat(bytes))
    end
    f:close()
end

emu.register_frame_done(function()
    frame = frame + 1
    if frame == 1100 then
        local sp = manager.machine.devices[":mainpcb:maincpu"].spaces["program"]
        dump("work_ram.bin", sp, 0x200000, 0x10000)
        dump("video_ram.bin", sp, 0x300000, 0x20000)
        dump("sprite_ram.bin", sp, 0x400000, 0x20000)
        dump("palette.bin", sp, 0x600000, 0x10000)
        dump("mixer.bin", sp, 0x610000, 0x80)
        dump("shared_ram.bin", sp, 0x700000, 0x2000)
    end
end)
