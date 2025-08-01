#ifndef BTN_H
#define BTN_H

#include <Arduino.h>

#define PU 1
#define PD 0

extern "C" {
typedef void (*callbackFunction)(void);
}

class Btn {
  private:
    /* data */
    int8_t pin = 0;
    uint8_t mode = 0;
    uint8_t activeLevel = 0;

    uint32_t debT = 50;
    uint32_t clickT = 300;
    uint32_t pressT = 400;
    uint32_t flag = NA;

    int btnPressed = 0;

    enum FSM_t : int { INIT = 0, DOWN, UP, COUNT, PRESS, RELEASED, WHAT };

    FSM_t state = INIT;
    FSM_t prev_state = INIT;

    uint8_t n_click = 0, max_click = 3;
    unsigned long start_T = 0;

    void newState(FSM_t new_state);

    callbackFunction click_foo = NULL;
    callbackFunction x2_click_foo = NULL;
    callbackFunction x_click_foo = NULL;
    callbackFunction press_foo = NULL;

  public:
    Btn(/* args */);
    Btn(const uint8_t pin, uint8_t mode);
    ~Btn();

    typedef enum btn_state {
        BTN_CLICK = 0,
        BTN_X2_CLICK,
        BTN_X3_CLICK,
        BTN_PRESS,
        NA
    } btn_state_t;

    bool isIdle() const { return state == INIT; }
    bool isLongPressed() const { return state == PRESS; };

    int getBtn_state(void);
    void clearBtn_state(void);

    void tick(void);
    void tick(uint8_t activeLevel);
    void reset(void);
    uint8_t getNumberClicks(void);
};

#endif // !BTN_H