#include <stdint.h>
#include <stdio.h>

#include <Adafruit_NeoPixel.hpp>
#include <cstdlib>
#include <cstring>
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/error.h"
#include "pico/stdio.h"
#include "pico/time.h"
#include "pico/types.h"

constexpr uint PIN_LED      = 25;
constexpr uint PIN_NEOPIXEL = 0;
constexpr uint PIN_BTN      = 1;

constexpr int LED_COUNT_BTN     = 12;
constexpr int LED_COUNT_MIRROR  = 24;
constexpr int LED_COUNT         = LED_COUNT_MIRROR + LED_COUNT_BTN;
constexpr int LED_MIRROR_OFFSET = 8;

constexpr uint8_t BRIGHTNESS = 20;

constexpr float FRAME_RATE            = 20.0f;
constexpr uint64_t FRAME_PERIOD_US    = 1e6 / FRAME_RATE;
constexpr uint64_t DURATION_RING_US   = 5e6;
constexpr uint64_t DURATION_NOTIFY_US = 15e6;

constexpr size_t BUF_LEN = 16;  // #RGBW = 9 bytes

Adafruit_NeoPixel strip(LED_COUNT, PIN_NEOPIXEL, NEO_GRBW + NEO_KHZ800);

// Offset indices since the strips are daisy-chained
#define setColorBtn(i, c) \
    { strip.setPixelColor(LED_COUNT_MIRROR + i, c); }
#define setColorMirror(i, c) \
    { strip.setPixelColor(i, c); }

uint32_t WHITE = strip.Color(0, 0, 0, 255);
uint32_t colorWheel(uint16_t degrees);

// Animation functions
typedef void (*animation_t)(uint16_t);

void rainbow(uint16_t frame);
void theatreChase(uint16_t frame);
void theatreChaseRainbow(uint16_t frame);
void pulseWhite(uint16_t frame);

constexpr animation_t ANIMATIONS[] = {
    rainbow,
    theatreChase,
    theatreChaseRainbow,
    pulseWhite,
};
constexpr size_t N_ANIMATIONS = 4;

void notification(uint16_t frame);

uint8_t hexToInt(char c);
uint32_t parseColor(const char* cmd);

enum class State : uint8_t {
    IDLE   = 0,
    RING   = 1,
    NOTIFY = 2,
};

int main() {
    stdio_init_all();

    bool act_led_on = false;
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, act_led_on);

    bool btn_pressed_latched = false;
    gpio_init(PIN_BTN);
    gpio_set_dir(PIN_BTN, GPIO_IN);
    gpio_set_pulls(PIN_BTN, true, false);

    strip.begin();
    strip.show();
    strip.setBrightness(BRIGHTNESS);

    // Sleep for a bit to stablize pull-ups
    sleep_ms(100);

    uint32_t idle_color = strip.Color(0, 0, 0);
    State state         = State::IDLE;

    uint64_t now          = time_us_64();
    uint64_t next_frame   = now;
    uint64_t next_timeout = 0;

    uint16_t current_frame   = 0;
    size_t current_animation = 0;

    char buf[BUF_LEN + 1] = {0};
    size_t buf_i          = 0;

    bool last_btn = true;

    while (true) {
        // Default is no lights at all
        // If button is pressed, launch a random 5s animation
        // Keep checking serial for commands
        // #RGBW<cr> will change button to that color
        // !<cr> will start a notification on the mirror
        now = time_us_64();

        // Check serial buffer
        int c = getchar_timeout_us(0);
        switch (c) {
            case PICO_ERROR_TIMEOUT:
                break;
            case '\n':
            case '\r': {
                // End of command check if buffer has a valid command
                // printf("Buffer is: '%s'\n", static_cast<char*>(buf));
                switch (buf[0]) {
                    case '#': {
                        idle_color = parseColor(buf);
                        state      = State::IDLE;
                        // printf("idle_color: 0x%08X\n", idle_color);
                    } break;
                    case '!': {
                        state         = State::NOTIFY;
                        current_frame = 0;
                        next_timeout  = now + DURATION_NOTIFY_US;
                        // printf("NOTIFY\n");
                    } break;
                    default:
                        break;
                }
                buf_i = 0;
                for (size_t i = 0; i < BUF_LEN; ++i) {
                    buf[i] = 0;
                }
            } break;
            default: {
                if (buf_i < BUF_LEN) {
                    // If buffer has room, add c; else ignore
                    buf[buf_i] = static_cast<char>(c);
                    ++buf_i;
                }
            } break;
        }

        // Keep a consistant frame rate
        if (now < next_frame) {
            // No interrupts so poll button if not ringing
            bool current_btn = gpio_get(PIN_BTN);
            if (!current_btn && last_btn) {
                btn_pressed_latched = true;
            }
            last_btn = current_btn;
            sleep_us(100);
            continue;
        }
        next_frame += FRAME_PERIOD_US;

        gpio_put(PIN_LED, act_led_on);
        act_led_on = !act_led_on;

        if (btn_pressed_latched) {
            // Actually handle button by starting ring animation
            state               = State::RING;
            current_frame       = 0;
            btn_pressed_latched = false;
            next_timeout        = now + DURATION_RING_US;
            current_animation   = (current_animation + 1) % N_ANIMATIONS;
        } else if (now > next_timeout) {
            // Once timeout expires, go back to idle
            state = State::IDLE;
        }

        switch (state) {
            case State::IDLE: {
                // Set idle color on btn
                for (uint16_t i = 0; i < LED_COUNT_BTN; ++i) {
                    setColorBtn(i, idle_color);
                }
                // Turn mirror off
                for (uint16_t i = 0; i < LED_COUNT_MIRROR; ++i) {
                    setColorMirror(i, 0);
                }
            } break;
            case State::RING: {
                // Set white on btn
                for (uint16_t i = 0; i < LED_COUNT_BTN; ++i) {
                    setColorBtn(i, strip.Color(0, 0, 0, 255));
                }
                // Run animation
                ANIMATIONS[current_animation](current_frame);
            } break;
            case State::NOTIFY: {
                // Set idle color on btn
                for (uint16_t i = 0; i < LED_COUNT_BTN; ++i) {
                    setColorBtn(i, idle_color);
                }
                notification(current_frame);

            } break;
        }
        strip.show();

        ++current_frame;
    }
}

/**
 * Convert hex character to integer
 *
 * @param c character to parse
 * @return uint8_t number [0, 15] or 0xFF if failure
 */
uint8_t hexToInt(char c) {
    if (c >= '0' && c <= '9')
        return static_cast<uint8_t>(c - '0');
    if (c >= 'A' && c <= 'F')
        return static_cast<uint8_t>(c - 'A' + 10);
    if (c >= 'a' && c <= 'f')
        return static_cast<uint8_t>(c - 'a' + 10);
    return 0xFF;
}

/**
 * @brief Parse color
 *
 * @param buf string buffer of #RGB, #RGBW, #RRGGBB, #RRGGBBWW
 * @returns NeoPixel color
 */
uint32_t parseColor(const char* buf) {
    // Trim off pound
    if (buf[0] == '#') {
        ++buf;
    }
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t w = 0;

    uint8_t t = 0;

    switch (strlen(buf)) {
        case 4: {
            // RGBW
            w = hexToInt(buf[3]);
        }
        // Fall through
        case 3: {
            // RGB
            r = hexToInt(buf[0]);
            g = hexToInt(buf[1]);
            b = hexToInt(buf[2]);

            // If any failed to parse, return dark
            if (r == 0xFF || g == 0xFF || b == 0xFF || w == 0xFF)
                return 0;

            // Expand RGB to RRGGBB
            r = (r << 4) | r;
            g = (g << 4) | g;
            b = (b << 4) | b;
            w = (w << 4) | w;
        } break;
        case 8: {
            // RRGGBBWW
            w = hexToInt(buf[6]);
            t = hexToInt(buf[7]);
            if (w == 0xFF || t == 0xFF)
                return 0;
            w = (w << 4) | t;
        }
        // Fall through
        case 6: {
            // RRGGBB
            r = hexToInt(buf[0]);
            t = hexToInt(buf[1]);
            if (r == 0xFF || t == 0xFF)
                return 0;
            r = (r << 4) | t;

            g = hexToInt(buf[2]);
            t = hexToInt(buf[3]);
            if (g == 0xFF || t == 0xFF)
                return 0;
            g = (g << 4) | t;

            b = hexToInt(buf[4]);
            t = hexToInt(buf[5]);
            if (b == 0xFF || t == 0xFF)
                return 0;
            b = (b << 4) | t;
        } break;
    }

    return strip.Color(r, g, b, w);
}

/**
 * @brief Spin a rainbow color wheel around
 *
 * @param frame index of animation frame
 */
void rainbow(uint16_t frame) {
    uint32_t firstHue = frame * 0x0400;
    for (int i = 0; i < LED_COUNT_MIRROR; ++i) {
        int pixelHue = firstHue + (i * 65536L / LED_COUNT_MIRROR);
        setColorMirror(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
}

/**
 * @brief Theatre-marquee-style chasing lights
 *
 * @param frame index of animation frame
 */
void theatreChase(uint16_t frame) {
    frame = (frame / 2) % 3;
    for (int i = 0; i < LED_COUNT_MIRROR; ++i) {
        if ((i % 3) == frame) {
            setColorMirror(i, WHITE);
        } else {
            setColorMirror(i, 0);
        }
    }
}

/**
 * @brief Theatre-marquee-style chasing lights
 *
 * @param frame index of animation frame
 */
void theatreChaseRainbow(uint16_t frame) {
    uint32_t firstHue = frame * 0x0400;
    frame             = (frame / 2) % 3;

    for (int i = 0; i < LED_COUNT_MIRROR; ++i) {
        if ((i % 3) == frame) {
            int pixelHue = firstHue + (i * 65536L / LED_COUNT_MIRROR);
            setColorMirror(i, strip.gamma32(strip.ColorHSV(pixelHue)));
        } else {
            setColorMirror(i, 0);
        }
    }
}

/**
 * @brief Pulse white white
 *
 * @param frame index of animation frame
 */
void pulseWhite(uint16_t frame) {
    constexpr uint16_t N_UP   = FRAME_RATE / 2;
    constexpr uint16_t N_DOWN = FRAME_RATE;
    constexpr uint16_t N_LOOP = N_UP + N_DOWN;
    frame                     = frame % N_LOOP;

    constexpr uint8_t STEP_UP   = 255 / N_UP;
    constexpr uint8_t STEP_DOWN = 255 / N_DOWN;

    uint8_t b = STEP_UP * frame;
    if (frame >= N_UP) {
        frame -= N_UP;
        b = 255 - (STEP_DOWN * frame);
    }
    uint32_t c = strip.Color(0, 0, 0, strip.gamma8(b));
    for (int i = 0; i < LED_COUNT_MIRROR; ++i) {
        setColorMirror(i, c);
    }
}

/**
 * @brief notification animation
 *
 * @param frame index of animation frame
 */
void notification(uint16_t frame) {
    static float pixels[LED_COUNT_MIRROR] = {0.0f};
    static int x                          = 0;
    static int v                          = 0;
    static int prev_led                   = 0;
    static bool circle_mode               = false;

    if (frame == 0) {
        x           = 0;
        v           = 1;
        prev_led    = LED_MIRROR_OFFSET;
        circle_mode = false;
    }

    // Decay LEDS to form a tail
    for (int i = 0; i < LED_COUNT_MIRROR; ++i) {
        if (frame == 0)
            pixels[i] = 0.0f;
        else
            pixels[i] *= 0.95f;
    }

    uint32_t firstHue = frame * 0x0400;

    int prev_x = x;
    int prev_v = v;
    x += v;

    v += (prev_x > 0) ? -1 : 1;

    // Align 0 = bottom
    int current = x * LED_COUNT_MIRROR / 400;
    if (abs(current) > (LED_COUNT_MIRROR / 2)) {
        x = -prev_x + prev_v;
    }
    current = x * LED_COUNT_MIRROR / 400;
    current =
        (current + LED_MIRROR_OFFSET + LED_COUNT_MIRROR) % LED_COUNT_MIRROR;
    pixels[current] = 1.0f;
    int delta =
        (current - prev_led + (LED_COUNT_MIRROR / 2)) % LED_COUNT_MIRROR -
        (LED_COUNT_MIRROR / 2);
    for (int i = 0; i < delta; ++i) {
        pixels[(current - i + LED_COUNT_MIRROR) % LED_COUNT_MIRROR] = 1.0f;
    }
    for (int i = 0; i > delta; i--) {
        pixels[(current - i + LED_COUNT_MIRROR) % LED_COUNT_MIRROR] = 1.0f;
    }
    prev_led = current;

    for (int i = 0; i < LED_COUNT_MIRROR; ++i) {
        if (i == current) {
            setColorMirror(i, WHITE);
        } else {
            uint16_t h = firstHue + (i * 65536L / LED_COUNT_MIRROR);
            uint8_t v  = (pixels[i] * 255);
            setColorMirror(i, strip.gamma32(strip.ColorHSV(h, 0xFF, v)));
        }
    }
}
