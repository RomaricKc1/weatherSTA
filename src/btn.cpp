#include "btn.hpp"

namespace N_Btn {
    Btn::Btn() : pin{PIN_INVALID} {}

    Btn::Btn(const int8_t pin, bool mode) : pin(pin) {
        if (mode == PU) {
            pinMode(pin, INPUT_PULLUP);
            btnPressed = 0;
        } else if (mode == PD) {
            pinMode(pin, INPUT_PULLDOWN);
            btnPressed = 1;
        }
        flag = NA;
    }

    auto Btn::getBtnState() const -> uint32_t { return flag; }

    void Btn::clearBtnState() { flag = NA; }

    void Btn::newState(E_FSM new_state) {
        prev_state = state;
        state      = new_state;
    }

    void Btn::tick() {
        if (pin > 0) {
            tick(digitalRead(pin) == btnPressed);
        }
    }

    void Btn::tick(bool activeLevel) {
        tick_args.now      = millis();
        tick_args.waitTime = tick_args.now - start_T;

        switch (state) {
            case INIT: {
                // waiting for level to become active.
                if (activeLevel) {
                    newState(DOWN);
                    start_T = tick_args.now;
                    n_click = 0;
                }
            } break;

            case DOWN: {
                // waiting for level to become inactive.
                if (!activeLevel and (tick_args.waitTime < debT)) {
                    // bouncing.
                    newState(prev_state);
                } else if (!activeLevel) {
                    newState(UP);
                    start_T = tick_args.now;
                } else if (activeLevel and (tick_args.waitTime > pressT)) {
                    newState(PRESS);
                }

            } break;

            case COUNT: {
                // debounce time is over, count clicks
                if (activeLevel) {
                    // button is down again
                    newState(DOWN);
                    start_T = tick_args.now;
                } else if ((tick_args.waitTime > clickT) or
                           (n_click == max_click)) {
                    // now we know how many clicks have been made.
                    switch (n_click) {
                        case 1: {
                            // this was 1 click only.
                            flag = BTN_CLICK;
                        } break;
                        case 2: {
                            // this was a 2 click sequence.
                            flag = BTN_X2_CLICK;
                        } break;
                        case MAX_CLICKS_ALLOWED: {
                            // this was a multi click sequence.
                            flag = BTN_X3_CLICK;
                        } break;
                        default: {
                            break;
                        }
                    }
                    reset();
                }
            } break;

            case UP: {
                // btn not pushed
                tickUp();
            } break;

            case PRESS: {
                // waiting for pin being release after long press.
                tickPress();
            } break;

            case RELEASED: {
                // button was released.
                tickReleased();
            } break;

            default: {
                // unknownreset state machine
                newState(INIT);
            } break;
        }
    }

    void Btn::tickUp() {
        if (activeLevel and (tick_args.waitTime < debT)) {
            // bouncing.
            newState(prev_state);
        } else if (tick_args.waitTime >= debT) {
            // count as a short button down
            n_click++;
            newState(COUNT);
        }
    }

    void Btn::tickPress() {
        if (!activeLevel) {
            newState(RELEASED);
            start_T = tick_args.now;
        }
        // else, still the button is pressed
    }

    void Btn::tickReleased() {
        if (activeLevel and (tick_args.waitTime < debT)) {
            // bouncing.
            newState(prev_state); // go back
        } else if (tick_args.waitTime >= debT) {
            flag = BTN_PRESS;
            reset();
        }
    }

    void Btn::reset() {
        state      = INIT;
        prev_state = INIT;
        n_click    = 0;
        start_T    = 0;
    }
} // namespace N_Btn
