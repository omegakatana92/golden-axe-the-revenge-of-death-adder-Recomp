local root = "C:/Users/omega/Documents/Default Project/Alien3-TheGun-Recomp/build/ref/mame_multi/"
local frames = {300,320,340,360,380,400,450,500,550,600}
local frame = 0
local wanted = {}
for _, f in ipairs(frames) do wanted[f] = true end

emu.register_frame_done(function()
    frame = frame + 1
    if wanted[frame] then
        local sp = manager.machine.devices[":mainpcb:maincpu"].spaces["program"]
        local f = io.open(root .. "work_" .. frame .. ".bin", "wb")
        for a = 0x200000, 0x20FFFF, 4096 do
            local bytes = {}
            for i = 0, 4095 do bytes[#bytes + 1] = string.char(sp:read_u8(a + i)) end
            f:write(table.concat(bytes))
        end
        f:close()
    end
end)
