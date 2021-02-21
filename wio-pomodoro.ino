// This project is licensed under the MIT License (see LICENSE).

#include <TFT_eSPI.h>
#include <Seeed_FS.h>
#include <SD/Seeed_SD.h>

#include "Free_Fonts.h"
#include "RawImage.h"

TFT_eSPI tft = TFT_eSPI();

enum {
    LEFT_KEY,
    RIGHT_KEY,
};

enum {
    STOPPED,
    WORKING,
    SHORT_BREAK,
    LONG_BREAK,
};

const int WORKING_SEC = 25 * 60;
const int SHORT_BREAK_SEC = 5 * 60;
const int LONG_BREAK_SEC = 15 * 60;
const int SET_LENGTH = 4;

const int SCREEN_W = 320;
const int SCREEN_H = 240;
const int IMAGE_SIZE = 64;
const int FONT_SIZE = 32;
const int CLOCK_LEN = FONT_SIZE * 5;
const int PAUSED_W = 4;
const int PAUSED_H = FONT_SIZE / 2;

const unsigned long UPDATE_MS = 1000;
const unsigned long DEBOUNCE_MS = 200;

unsigned long last_update = 0;
int running = 0;
int state = STOPPED;
int countdown = 0;
int rep = 0;

unsigned long last_keypress[2] = {0};
int key_state[2] = {HIGH};

char clock_buf[6];
Raw8 *images[4];

void load_images() {
    images[STOPPED] = newImage<uint8_t>("stopped.bmp");
    images[WORKING] = newImage<uint8_t>("working.bmp");
    images[SHORT_BREAK] = newImage<uint8_t>("short_break.bmp");
    images[LONG_BREAK] = newImage<uint8_t>("long_break.bmp");
}

void draw_paused(int x, int y) {
    uint32_t color = TFT_BLACK;
    if (!running && state != STOPPED)
        color = TFT_WHITE;
    tft.fillRect(x, y, PAUSED_W, PAUSED_H, color);
    tft.fillRect(x + PAUSED_W * 2, y, PAUSED_W, PAUSED_H, color);
}

void update(int update_image) {
    int clock_x = SCREEN_W / 2 - CLOCK_LEN / 2 + IMAGE_SIZE / 2 + FONT_SIZE / 2;
    int clock_y = SCREEN_H / 2 - FONT_SIZE / 2;
    int image_x = SCREEN_W / 2 - CLOCK_LEN / 2 - IMAGE_SIZE / 2;
    int image_y = SCREEN_H / 2 - IMAGE_SIZE / 2;
    int paused_x = clock_x + CLOCK_LEN;
    int paused_y = SCREEN_H / 2 - PAUSED_H / 2;
    int m = countdown / 60;
    int s = countdown % 60;

    snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", m, s);
    tft.drawString(clock_buf, clock_x, clock_y);
    if (update_image)
        tft.fillRect(image_x, image_y, IMAGE_SIZE, IMAGE_SIZE, TFT_BLACK);
    images[state]->draw(image_x, image_y);
    draw_paused(paused_x, paused_y);

    last_update = millis();
}

void setup() {
    Serial.begin(115200);
    pinMode(WIO_KEY_A, INPUT_PULLUP);
    pinMode(WIO_KEY_C, INPUT_PULLUP);

    if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
        Serial.println("SDCARD initialization failed");
        while (1);
    }

    load_images();

    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setFreeFont(FMB24);
    update(0);
}

void loop() {
    if (digitalRead(WIO_KEY_A) == LOW && key_state[RIGHT_KEY] != LOW) {
        key_state[RIGHT_KEY] = LOW;
        last_keypress[RIGHT_KEY] = millis();
    }

    if (digitalRead(WIO_KEY_C) == LOW && key_state[LEFT_KEY] != LOW) {
        key_state[LEFT_KEY] = LOW;
        last_keypress[LEFT_KEY] = millis();
    }

    int last_state = state;

    for (int i = 0; i < 2; i++) {
        if (key_state[i] != LOW || millis() - last_keypress[i] < DEBOUNCE_MS)
            continue;
        switch (i) {
            case RIGHT_KEY:
                running = 0;
                state = STOPPED;
                countdown = 0;
                update(1);
                break;
            case LEFT_KEY:
                if (state == STOPPED) {
                    running = 1;
                    state = WORKING;
                    countdown = WORKING_SEC;
                } else {
                    running = !running;
                    update(0);
                }
                break;
        }
        key_state[i] = HIGH;
    }

    if (running && millis() - last_update >= UPDATE_MS) {
        if (--countdown == 0) {
            switch (state) {
                case WORKING:
                    if (++rep == SET_LENGTH) {
                        rep = 0;
                        state = LONG_BREAK;
                        countdown = LONG_BREAK_SEC;
                    } else {
                        state = SHORT_BREAK;
                        countdown = SHORT_BREAK_SEC;
                    }
                    break;
                case SHORT_BREAK:
                    state = WORKING;
                    countdown = WORKING_SEC;
                    break;
                case LONG_BREAK:
                    running = 0;
                    state = STOPPED;
                    countdown = 0;
                    break;
            }
        }

        update(state != last_state);
    }
}
