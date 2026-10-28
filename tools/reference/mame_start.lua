local function setfield(port, field, value)
    local p = manager.machine.ioport.ports[port]
    if p and p.fields[field] then p.fields[field]:set_value(value) end
end

local frame = 0
emu.register_frame_done(function()
    frame = frame + 1
    if frame == 500 then setfield(":mainpcb:SERVICE12_A", "Coin 1", 1) end
    if frame == 506 then setfield(":mainpcb:SERVICE12_A", "Coin 1", 0) end
    if frame == 850 then setfield(":mainpcb:P1_A", "P1 Button 1", 1) end
    if frame == 950 then setfield(":mainpcb:P1_A", "P1 Button 1", 0) end
end)
