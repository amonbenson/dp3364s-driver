local box = gfx.container({ direction = HORIZONTAL })

box:add_child(gfx.text({ text = "Hello" }))
box:add_child(gfx.separator({ direction = VERTICAL, appearance = ACCENT }))
box:add_child(gfx.text({ text = "12345", font = "tb-8-bold", appearance = SECONDARY }))

root(box)
