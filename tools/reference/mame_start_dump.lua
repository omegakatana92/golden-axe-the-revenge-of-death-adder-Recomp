local dir = "C:/Users/omega/Documents/Default Project/Alien3-TheGun-Recomp/build/ref/mame_start_dump/"
local frame = 0

local function setfield(port, field, value)
    local p = manager.machine.ioport.ports[port]
    if p and p.fields[field] then p.fields[field]:set_value(value) end
end

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
    if frame == 500 then setfield(":mainpcb:SERVICE12_A", "Coin 1", 1) end
    if frame == 506 then setfield(":mainpcb:SERVICE12_A", "Coin 1", 0) end
    if frame == 850 then setfield(":mainpcb:P1_A", "P1 Button 1", 1) end
    if frame == 950 then setfield(":mainpcb:P1_A", "P1 Button 1", 0) end
    if frame == 900 then
        local sp = manager.machine.devices[":mainpcb:maincpu"].spaces["program"]
        dump("work_ram.bin", sp, 0x200000, 0x10000)
        dump("video_ram.bin", sp, 0x300000, 0x20000)
        dump("palette.bin", sp, 0x600000, 0x10000)
        dump("shared_ram.bin", sp, 0x700000, 0x2000)
    end
end)
