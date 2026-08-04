/*
 * main.c - example application: a scrolling rainbow test pattern.
 *
 * All panel wiring, timing and protocol detail lives in the dp3364s
 * component; this file only decides what to draw. Swap draw_frame() (and
 * everything above it) to display something else instead - e.g. images
 * fetched from the network - without touching the driver.
 */

#include <math.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dp3364s.h"
#include "gfx_prim.h"

static const char *TAG = "test_pattern";



// /* Convert a hue in degrees to 8-bit RGB at full saturation and value */
// static void hue_to_rgb8(float hue_deg, uint8_t *r, uint8_t *g, uint8_t *b) {
//     float h = fmodf(hue_deg, 360.0f) / 60.0f;
//     int i = (int)h;
//     float f = h - (float)i;
//     float q = 1.0f - f;
//     float rr, gg, bb;

//     switch (i) {
//         case 0:
//             rr = 1;
//             gg = f;
//             bb = 0;
//             break;
//         case 1:
//             rr = q;
//             gg = 1;
//             bb = 0;
//             break;
//         case 2:
//             rr = 0;
//             gg = 1;
//             bb = f;
//             break;
//         case 3:
//             rr = 0;
//             gg = q;
//             bb = 1;
//             break;
//         case 4:
//             rr = f;
//             gg = 0;
//             bb = 1;
//             break;
//         default:
//             rr = 1;
//             gg = 0;
//             bb = q;
//             break;
//     }

//     *r = (uint8_t)(rr * 255.0f);
//     *g = (uint8_t)(gg * 255.0f);
//     *b = (uint8_t)(bb * 255.0f);
// }

// /* Colour table for the example pattern. The rainbow repeats every HUE_STEPS
//  * diagonal steps, so it is built once at startup and looked up per pixel. */
// #define HUE_STEPS 64
// static uint8_t hue_lut[HUE_STEPS][3];
// static int hue_phase; // scroll position, in table steps

// static void build_hue_lut(void) {
//     for (int i = 0; i < HUE_STEPS; i++) {
//         hue_to_rgb8(i * (360.0f / HUE_STEPS), &hue_lut[i][0], &hue_lut[i][1], &hue_lut[i][2]);
//     }
// }

// /* Draw one frame. This is the only place the picture is decided; replace the
//  * body to display something else. The example is a diagonal rainbow scrolled
//  * by hue_phase. */
// static void draw_frame(void) {
//     for (int y = 0; y < DP3364S_HEIGHT; y++) {
//         for (int x = 0; x < DP3364S_WIDTH; x++) {
//             const uint8_t *c = hue_lut[(x + y + hue_phase) & (HUE_STEPS - 1)];

//             dp3364s_set_pixel(x, y, c[0], c[1], c[2]);
//         }
//     }
// }



void app_main(void) {
    ESP_ERROR_CHECK(dp3364s_init());

    const gfx_context_t ctx = {
        .width = DP3364S_WIDTH,
        .height = DP3364S_HEIGHT,
        .set_pixel = dp3364s_set_pixel,
    };

    gfx_draw_line(&ctx, (gfx_point_t) { 0, -20 }, (gfx_color_t) { 255, 0, 0 }, (gfx_point_t) { 500, 500 }, (gfx_color_t) { 0, 0, 255 });

    dp3364s_update();

    // build_hue_lut();

    // ESP_LOGI(TAG, "running - scrolling rainbow test pattern");

    // /*
    //  * Animation loop. The driver replays the presented frame by itself, so
    //  * each pass only updates the drawing state, draws into the framebuffer,
    //  * and presents it.
    //  *
    //  * A present takes effect when the DMA reaches the end of the frame it is
    //  * playing, so a partly drawn buffer is never shown. The delay afterwards
    //  * covers a full frame period, long enough for the DMA to move across
    //  * before the next pass overwrites the buffer it just left.
    //  */
    // int64_t last_report = esp_timer_get_time();
    // int frames = 0;
    // uint32_t build_us = 0;
    // uint32_t period_ms = dp3364s_frame_period_ms();

    // while (1) {
    //     int64_t t0 = esp_timer_get_time();

    //     hue_phase = (hue_phase + 1) & (HUE_STEPS - 1);

    //     draw_frame();
    //     dp3364s_update();

    //     build_us = (uint32_t)(esp_timer_get_time() - t0);
    //     frames++;

    //     /* pdMS_TO_TICKS rounds down and reaches zero at the default 100 Hz
    //      * tick rate. vTaskDelay(0) only yields, which starves the idle task
    //      * and trips the watchdog, so always block for at least one tick. */
    //     TickType_t wait = pdMS_TO_TICKS(period_ms);
    //     vTaskDelay(wait ? wait : 1);

    //     if (esp_timer_get_time() - last_report >= 5000000) {
    //         ESP_LOGI(TAG, "%.1f fps drawn, %u us per frame build", frames * 1e6 / (double)(esp_timer_get_time() - last_report), (unsigned) build_us);
    //         last_report = esp_timer_get_time();
    //         frames = 0;
    //     }
    // }
}
