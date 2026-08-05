local box = gfx.container({ direction = HORIZONTAL })

box:add_child(gfx.separator({ direction = HORIZONTAL }))
box:add_child(gfx.separator({ direction = VERTICAL, appearance = ACCENT }))
box:add_child(gfx.separator({ direction = HORIZONTAL, appearance = SECONDARY }))

root(box)
