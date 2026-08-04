local y = 0

function render()
    gfx.draw_rect(1, y, WIDTH - 2, HEIGHT - y - 1, 255, 128, 0)
    y = (y + 1) % HEIGHT
end
