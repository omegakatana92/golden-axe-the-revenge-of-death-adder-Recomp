local out = io.open("C:/Users/omega/Documents/Default Project/Alien3-TheGun-Recomp/build/ref/mame_ports.log", "w")
local frame = 0
local pressed = false

emu.register_frame_done(function()
    frame = frame + 1
    if frame == 1 then
        for tag, port in pairs(manager.machine.ioport.ports) do
            out:write(string.format("PORT %s\n", tag))
            for name, field in pairs(port.fields) do
                out:write(string.format("  FIELD %s\n", name))
            end
        end
        out:flush()
    end
    if frame == 600 and not pressed then
        pressed = true
        local p = manager.machine.ioport.ports[":mainpcb:SERVICE12_A"]
        if p then
            for name, field in pairs(p.fields) do
                out:write(string.format("COIN TRY %s\n", name))
                pcall(function() field:set_value(1) end)
            end
        end
        out:flush()
    end
    if frame == 606 and pressed then
        local p = manager.machine.ioport.ports[":mainpcb:SERVICE12_A"]
        if p then
            for name, field in pairs(p.fields) do pcall(function() field:set_value(0) end) end
        end
    end
    if frame == 660 and pressed then
        local p = manager.machine.ioport.ports[":mainpcb:SERVICE12_A"]
        if p then
            -- start is bit 4 of the modified alien3 port; set field named Start 1
            for name, field in pairs(p.fields) do
                if name:find("Start") then pcall(function() field:set_value(1) end) end
            end
        end
    end
end)
