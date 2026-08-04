local y = 0

function render()
    gfx.draw_line(0, y, WIDTH - 1, HEIGHT - y - 1, 255, 255, 255)
    y = (y + 1) % HEIGHT
end
