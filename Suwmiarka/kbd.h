#ifndef SUW_KBD_H
#define SUW_KBD_H

extern int initKeyboardSys(void);
extern int getKey(void);
extern void waitKeyUp();
#define BUTTON_PIN 32

enum {
    key_None = 0,
    key_Pressed,
    key_Short,
    key_Long,
    key_Double};
    

enum {
    DME_NONE=0,
    DME_WAIT,
    DME_FORGET
};

#endif
