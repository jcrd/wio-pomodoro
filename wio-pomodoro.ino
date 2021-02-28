// This project is licensed under the MIT License (see LICENSE).

#include <TFT_eSPI.h>
#include <Seeed_FS.h>
#include <SD/Seeed_SD.h>
#include <SparkFunBQ27441.h>

#include "Free_Fonts.h"
#include "RawImage.h"

#include "lib/lcd_backlight.hpp"

TFT_eSPI tft = TFT_eSPI();
LCDBackLight backlight;

enum {
    TOGGLE_RUNNING,
    RESET,
    BACKLIGHT_UP,
    BACKLIGHT_DOWN,
    INPUT_N,
};

enum {
    STOPPED,
    WORKING,
    SHORT_BREAK,
    LONG_BREAK,
};

const int BATTERY_CAPACITY = 650;

const int DEFAULT_BRIGHTNESS = 10;
const int BRIGHTNESS_STEP = 2;

const int input_map[] = {
    [TOGGLE_RUNNING] = WIO_KEY_C,
    [RESET] = WIO_KEY_A,
    [BACKLIGHT_UP] = WIO_5S_UP,
    [BACKLIGHT_DOWN] = WIO_5S_DOWN,
};

const int WORKING_SEC = 25 * 60;
const int SHORT_BREAK_SEC = 5 * 60;
const int LONG_BREAK_SEC = 15 * 60;
const int SET_LENGTH = 4;

const GFXfont *LARGE_FONT = FMB24;

const int SCREEN_W = 320;
const int SCREEN_H = 240;
const int IMAGE_SIZE = 64;
const int FONT_SIZE = 32;
const int CLOCK_LEN = FONT_SIZE * 5;
const int PAUSED_W = 4;
const int PAUSED_H = FONT_SIZE / 2;
const int BATTERY_BAR_H = 4;
const int REP_BAR_H = 8;
const int REP_BAR_DIV = 2;

const unsigned long UPDATE_MS = 1000;
const unsigned long DEBOUNCE_MS = 200;

unsigned long last_update = 0;
int running = 0;
int state = STOPPED;
int countdown = 0;
int rep = 0;

unsigned long last_input[INPUT_N] = {0};
int input_state[INPUT_N] = {HIGH};

char clock_buf[6];
char rep_buf[2];
Raw8 *images[4];

int has_battery = 0;
int last_battery_soc = 0;

int max_brightness;
int brightness = DEFAULT_BRIGHTNESS;

void load_images() {
    images[STOPPED] = newImage<uint8_t>("stopped.bmp");
    images[WORKING] = newImage<uint8_t>("working.bmp");
    images[SHORT_BREAK] = newImage<uint8_t>("short_break.bmp");
    images[LONG_BREAK] = newImage<uint8_t>("long_break.bmp");
}

void inc_brightness(int i) {
    brightness += i;
    if (brightness > max_brightness || brightness < 1)
        brightness = 1;
    backlight.setBrightness(brightness);
}

void draw_paused(int x, int y) {
    uint32_t color = TFT_BLACK;
    if (!running && state != STOPPED)
        color = TFT_WHITE;
    tft.fillRect(x, y, PAUSED_W, PAUSED_H, color);
    tft.fillRect(x + PAUSED_W * 2, y, PAUSED_W, PAUSED_H, color);
}

void draw_battery_bar() {
    int soc = lipo.soc();
    if (soc == last_battery_soc)
        return;
    last_battery_soc = soc;
    tft.fillRect(0, SCREEN_H - BATTERY_BAR_H, SCREEN_W, SCREEN_H,
            TFT_BLACK);
    tft.fillRect(0,
            SCREEN_H - BATTERY_BAR_H,
            SCREEN_W * soc / 100,
            SCREEN_H,
            TFT_WHITE);
}

void draw_rep_bar(int x, int y, int w) {
    if (rep == 0) {
        tft.fillRect(x, y, w, REP_BAR_H, TFT_BLACK);
        return;
    }
    int rep_div = w / SET_LENGTH;
    int rep_w = rep_div - REP_BAR_DIV;
    for (int i = 0; i < rep; i++)
        tft.fillRect(x + i * rep_div, y, rep_w, REP_BAR_H, TFT_WHITE);
}

void update(int state_changed) {
    int image_x = SCREEN_W / 2 - CLOCK_LEN / 2 - IMAGE_SIZE / 2;
    int image_y = SCREEN_H / 2 - IMAGE_SIZE / 2;
    int clock_x = image_x + IMAGE_SIZE;
    int clock_y = SCREEN_H / 2 - FONT_SIZE / 2;
    int rep_x = clock_x + CLOCK_LEN - FONT_SIZE / 2;
    int rep_y = SCREEN_H / 2 - FONT_SIZE / 2 - 4;
    int paused_x = clock_x + CLOCK_LEN;
    int paused_y = SCREEN_H / 2 - PAUSED_H / 2 + 4;
    int m = countdown / 60;
    int s = countdown % 60;

    if (state_changed) {
        tft.fillRect(image_x, image_y, IMAGE_SIZE, IMAGE_SIZE, TFT_BLACK);
        draw_rep_bar(image_x, image_y + IMAGE_SIZE, IMAGE_SIZE + CLOCK_LEN);
    }
    images[state]->draw(image_x, image_y);

    snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", m, s);
    tft.drawString(clock_buf, clock_x, clock_y);

    draw_paused(paused_x, paused_y);

    if (has_battery)
        draw_battery_bar();

    last_update = millis();
}

void setup() {
    Serial.begin(115200);
    pinMode(WIO_KEY_A, INPUT_PULLUP);
    pinMode(WIO_KEY_C, INPUT_PULLUP);
    pinMode(WIO_5S_UP, INPUT_PULLUP);
    pinMode(WIO_5S_DOWN, INPUT_PULLUP);

    backlight.initialize();
    backlight.setBrightness(brightness);
    max_brightness = backlight.getMaxBrightness();

    if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
        Serial.println("SDCARD initialization failed");
        while (1);
    }

    if (lipo.begin()) {
        lipo.setCapacity(BATTERY_CAPACITY);
        has_battery = 1;
    }

    load_images();

    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setFreeFont(LARGE_FONT);
    update(0);
}

void loop() {
    int last_state = state;

    for (int i = 0; i < INPUT_N; i++) {
        if (digitalRead(input_map[i]) == LOW && input_state[i] != LOW) {
            input_state[i] = LOW;
            last_input[i] = millis();
        }
        if (input_state[i] != LOW || millis() - last_input[i] < DEBOUNCE_MS)
            continue;
        switch (i) {
            case RESET:
                running = 0;
                rep = 0;
                state = STOPPED;
                countdown = 0;
                update(1);
                break;
            case TOGGLE_RUNNING:
                if (state == STOPPED) {
                    running = 1;
                    rep = 1;
                    state = WORKING;
                    countdown = WORKING_SEC;
                    update(1);
                } else {
                    running = !running;
                    update(0);
                }
                break;
            case BACKLIGHT_UP:
                inc_brightness(BRIGHTNESS_STEP);
                break;
            case BACKLIGHT_DOWN:
                inc_brightness(-BRIGHTNESS_STEP);
                break;
        }
        input_state[i] = HIGH;
    }

    if (running && millis() - last_update >= UPDATE_MS) {
        if (--countdown == 0) {
            switch (state) {
                case WORKING:
                    if (rep == SET_LENGTH) {
                        state = LONG_BREAK;
                        countdown = LONG_BREAK_SEC;
                    } else {
                        state = SHORT_BREAK;
                        countdown = SHORT_BREAK_SEC;
                    }
                    break;
                case SHORT_BREAK:
                    rep++;
                    state = WORKING;
                    countdown = WORKING_SEC;
                    break;
                case LONG_BREAK:
                    running = 0;
                    rep = 0;
                    state = STOPPED;
                    countdown = 0;
                    break;
            }
        }

        update(state != last_state);
    }
}
