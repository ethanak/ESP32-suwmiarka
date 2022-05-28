#include <Arduino.h>
#include "kbd.h"


static hw_timer_t * timer = NULL;

static uint32_t keyITimer;
static uint8_t dblIMode;
static uint8_t currentKey, lastKey;
uint32_t pfs_doubleMsec=300, pfs_longMsec=500;
portMUX_TYPE muxk = portMUX_INITIALIZER_UNLOCKED;

static int IRAM_ATTR getIKey()
{
    uint32_t ms = millis();
    uint8_t rc;
    if (ms - keyITimer < 30) return key_None;
    currentKey = digitalRead(BUTTON_PIN);
    
    if (currentKey == lastKey) {
        // klawisz wciśnięty?
        if (!currentKey) {
            if (!dblIMode && ms - keyITimer > pfs_longMsec) {
                dblIMode = DME_FORGET;
                return key_Long;
            }
            return key_None;
        }
        // klawisz puszczony?
        if (dblIMode && ms - keyITimer > pfs_doubleMsec) {
            dblIMode = 0;
            return key_Short;
        }
        return key_None;
    }
    lastKey = currentKey;
    keyITimer = ms;
    if (!currentKey) { // klawisz wciśnięty
        if (dblIMode == DME_WAIT) {
            dblIMode = DME_FORGET;
            return key_Double;
        }
        dblIMode = 0;
        return key_Pressed;
    }
    // klawisz puszczony
    if (dblIMode) {
        dblIMode = 0;
    }
    else {
        dblIMode = DME_WAIT;
    }
    return key_None;
}

static volatile uint8_t kring[8];
static volatile uint8_t kringpos, kringcnt;

static void IRAM_ATTR keyboardInt()
{
    int t = getIKey();
    if (!t) return;
    portENTER_CRITICAL_ISR(&muxk);
    if (kringcnt < 8) {
        kring[(kringpos + kringcnt) & 7] = t;
        kringcnt = (kringcnt +1) & 7;
    }
    portEXIT_CRITICAL_ISR(&muxk);
}

int getKey(void)
{
    int rc = key_None;
    portENTER_CRITICAL(&muxk);
    if (kringcnt) {
        rc = kring[kringpos];
        kringpos = (kringpos + 1) & 7;
        kringcnt--;
    }
    portEXIT_CRITICAL(&muxk);
    return rc;
}




int initKeyboardSys(void)
{
    lastKey=digitalRead(BUTTON_PIN);
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &keyboardInt, true);
    timerAlarmWrite(timer, 10000, true);
    timerAlarmEnable(timer);
    return lastKey;
}

void waitKeyUp()
{
    while(!lastKey) delay(100);
    portENTER_CRITICAL(&muxk);
    kringcnt = 0;
    kringpos = 0;
    dblIMode = 0;
    portEXIT_CRITICAL(&muxk);
}
