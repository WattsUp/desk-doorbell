#include <stdint.h>
#include <stdio.h>

#include <Adafruit_NeoPixel.hpp>
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/time.h"
#include "pico/types.h"

constexpr uint PIN_LED      = 25;
constexpr uint PIN_NEOPIXEL = 0;
constexpr uint PIN_BTN      = 1;

constexpr size_t LED_COUNT_BTN    = 12;
constexpr size_t LED_COUNT_MIRROR = 24;
constexpr size_t LED_COUNT        = LED_COUNT_MIRROR + LED_COUNT_BTN;

constexpr uint8_t BRIGHTNESS = 10;

constexpr float FRAME_RATE            = 20.0f;
constexpr uint64_t FRAME_PERIOD_US    = 1e6 / FRAME_RATE;
constexpr uint64_t DURATION_RING_US   = 5e6;
constexpr uint64_t DURATION_NOTIFY_US = 15e6;

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

    while (true) {
        // Default is no lights at all
        // If button is pressed, launch a random 5s animation
        // Keep checking serial for commands
        // #RGB<cr> will change button to that color
        // !<cr> will start a notification on the mirror

        // Keep a consistant frame rate
        now = time_us_64();
        if (now < next_frame) {
            // No interrupts so poll button if not ringing
            if (state != State::RING && gpio_get(PIN_BTN) == 0) {
                btn_pressed_latched = true;
            }
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
            case State::IDLE:
                // Set idle color on btn
                for (uint16_t i = 0; i < LED_COUNT_BTN; ++i) {
                    setColorBtn(i, idle_color);
                }
                // Turn mirror off
                for (uint16_t i = 0; i < LED_COUNT_MIRROR; ++i) {
                    setColorMirror(i, 0);
                }
                break;
            case State::RING:
                // Set white on btn
                for (uint16_t i = 0; i < LED_COUNT_BTN; ++i) {
                    setColorBtn(i, strip.Color(0, 0, 0, 255));
                }
                // Run animation
                ANIMATIONS[current_animation](current_frame);
                break;
            case State::NOTIFY:
                // Set idle color on btn
                for (uint16_t i = 0; i < LED_COUNT_BTN; ++i) {
                    setColorBtn(i, idle_color);
                }
                break;
        }
        strip.show();

        ++current_frame;
    }
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
