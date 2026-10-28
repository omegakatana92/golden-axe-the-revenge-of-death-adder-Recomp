local out = io.open("C:/Users/omega/Documents/Default Project/Alien3-TheGun-Recomp/build/ref/mame_probe.log", "w")
local frame = 0
local started = false

emu.register_frame_done(function()
    frame = frame + 1
    if not started then
        started = true
        for tag, dev in pairs(manager.machine.devices) do
            out:write(string.format("DEV %s\n", tag))
        end
        out:flush()
    end
    if frame == 30 or frame == 60 or frame == 120 or frame == 240 or frame == 420 then
        local dev = manager.machine.devices[":mainpcb:maincpu"] or manager.machine.devices[":maincpu"]
        local sp = dev.spaces["program"]
        local st = dev.state
        local pc = 0
        pcall(function() pc = st["PC"].value end)
        out:write(string.format("FRAME %d PC %06X 200240 %02X 200000 %02X 200098 %02X 200099 %02X\n",
            frame, pc, sp:read_u8(0x200240), sp:read_u8(0x200000),
            sp:read_u8(0x200098), sp:read_u8(0x200099)))
        out:flush()
    end
end)
