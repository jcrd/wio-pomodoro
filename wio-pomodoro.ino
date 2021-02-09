// This project is licensed under the MIT License (see LICENSE).

#include <TFT_eSPI.h>

#include "Free_Fonts.h"

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

void update() {
    int m = countdown / 60;
    int s = countdown % 60;
    snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", m, s);
    tft.drawString(clock_buf, 0, 0);
    last_update = millis();
}

void setup() {
    Serial.begin(115200);
    pinMode(WIO_KEY_A, INPUT_PULLUP);
    pinMode(WIO_KEY_C, INPUT_PULLUP);

    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setFreeFont(FM24);
    update();
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

    for (int i = 0; i < 2; i++) {
        if (key_state[i] != LOW || millis() - last_keypress[i] < DEBOUNCE_MS)
            continue;
        switch (i) {
            case RIGHT_KEY:
                running = 0;
                state = STOPPED;
                countdown = 0;
                update();
                break;
            case LEFT_KEY:
                if (state == STOPPED) {
                    running = 1;
                    state = WORKING;
                    countdown = WORKING_SEC;
                } else {
                    running = !running;
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

        update();
    }
}
