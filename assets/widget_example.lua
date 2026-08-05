local box = gfx.container({ direction = HORIZONTAL, alignment = CENTER, justification = CENTER })

box:add_child(gfx.text({ text = "Hello", appearance = ACCENT }))
box:add_child(gfx.separator({ direction = VERTICAL }))
box:add_child(gfx.text({ text = "World" }))

root(box)
