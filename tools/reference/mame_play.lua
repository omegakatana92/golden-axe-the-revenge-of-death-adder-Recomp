local out = io.open("C:/Users/omega/Documents/Default Project/Alien3-TheGun-Recomp/build/ref/mame_play.log", "w")
local frame = 0

local function setfield(port, field, value)
    local p = manager.machine.ioport.ports[port]
    if p and p.fields[field] then p.fields[field]:set_value(value) end
end

emu.register_frame_done(function()
    frame = frame + 1
    if frame == 600 then setfield(":mainpcb:SERVICE12_A", "Coin 1", 1) end
    if frame == 606 then setfield(":mainpcb:SERVICE12_A", "Coin 1", 0) end
    if frame == 660 then setfield(":mainpcb:P1_A", "P1 Button 1", 1) end
    if frame == 666 then setfield(":mainpcb:P1_A", "P1 Button 1", 0) end
    if frame % 60 == 0 then
        local dev = manager.machine.devices[":mainpcb:maincpu"]
        local st = dev.state
        local sp = dev.spaces["program"]
        local pc = 0
        pcall(function() pc = st["PC"].value end)
        out:write(string.format("FRAME %d PC %06X 200099 %02X 200240 %02X 200000 %02X credits? %02X\n",
            frame, pc, sp:read_u8(0x200099), sp:read_u8(0x200240), sp:read_u8(0x200000), sp:read_u8(0x2000e8)))
        out:flush()
    end
end)
