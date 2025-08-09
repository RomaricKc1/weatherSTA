#ifndef BTN_HPP
#define BTN_HPP

#include "commons.hpp"

namespace N_Btn {
    extern "C" {
    using callbackFunction = void (*)();
    }

    class Btn {
      private:
        int8_t pin{};
        bool mode{};
        bool activeLevel{};

        uint32_t debT{T_BTN_DEBOUNCE_MS};
        uint32_t clickT{T_BTN_CLICK_MS};
        uint32_t pressT{T_BTN_PRESS_MS};
        uint32_t flag{E_BtnState::NA};

        int btnPressed{};
        S_TickArgs tick_args{};

        enum E_FSM : uint8_t {
            INIT = 0,
            DOWN,
            UP,
            COUNT,
            PRESS,
            RELEASED,
            WHAT,
        };

        E_FSM state{INIT};
        E_FSM prev_state{INIT};

        uint8_t n_click{};
        uint8_t max_click{MAX_CLICKS_ALLOWED};
        uint64_t start_T{};

        void tickPress();
        void tickReleased();
        void tickUp();
        void newState(E_FSM new_state);
        void reset();

      public:
        Btn();
        Btn(int8_t pin, bool mode);

        using E_BtnState = enum : uint8_t {
            BTN_CLICK = 0,
            BTN_X2_CLICK,
            BTN_X3_CLICK,
            BTN_PRESS,
            NA
        };

        [[nodiscard]] auto isIdle() const -> bool;
        [[nodiscard]] auto getBtnState() const -> uint32_t;
        void clearBtnState();

        void tick();
        void tick(bool activeLevel);
    };

} // namespace N_Btn

#endif // BTN_HPP
