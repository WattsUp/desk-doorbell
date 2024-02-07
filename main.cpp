#include <stdio.h>

#include <Adafruit_NeoPixel.hpp>

#include "hardware/adc.h"

constexpr size_t LED_PIN = 25;
constexpr size_t TEMP_ADC = 4;
constexpr size_t NEOPIXEL_LED_PIN = 0;

constexpr size_t LED_COUNT_BTN = 12;
constexpr size_t LED_COUNT_MIRROR = 24;
constexpr size_t LED_COUNT = LED_COUNT_MIRROR + LED_COUNT_BTN;

constexpr size_t BRIGHTNESS = 10;

constexpr float conversion_factor = 3.3f / (1 << 12);

Adafruit_NeoPixel strip(LED_COUNT, NEOPIXEL_LED_PIN, NEO_GRBW + NEO_KHZ800);

void colorWipe(uint32_t color, int wait);
void rainbow(int wait);

int main() {
  stdio_init_all();

  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  adc_init();
  adc_gpio_init(26);
  adc_set_temp_sensor_enabled(true);

  adc_select_input(TEMP_ADC);

  strip.begin();
  strip.show();
  strip.setBrightness(BRIGHTNESS);

  while (true) {
    gpio_put(LED_PIN, true);
    sleep_ms(1000);

    colorWipe(strip.Color(255, 0, 0), 50);
    colorWipe(strip.Color(0, 0, 0, 255), 50);

    gpio_put(LED_PIN, false);
    sleep_ms(1000);

    rainbow(10);

    const float voltage = adc_read() * conversion_factor;
    const float temperature = 27 - (voltage - 0.706) / 0.001721;

    printf("Hello, world! The temperature is: %fc\n", temperature);
  }
}

void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); ++i) {
    strip.setPixelColor(i, color);
    strip.show();
    sleep_ms(wait);
  }
}

void rainbow(int wait) {
  for (long firstHue = 0; firstHue < 65536; firstHue += 256) {
    for (int i = 0; i < strip.numPixels(); ++i) {
      int pixelHue = firstHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    sleep_ms(wait);
  }
}
