#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include "commons.hpp"
#include "sensors.hpp"
#include "sys.hpp"

namespace N_Graphics {
    extern std::unique_ptr<LiquidCrystal_I2C> lcd;

    extern TaskHandle_t xLed;

    extern RTC_DATA_ATTR int32_t LCD_SPD;
    extern std::array<byte, LCD_BYTES_CNT_8> degre;
    extern std::array<byte, LCD_BYTES_CNT_8> up;
    extern std::array<byte, LCD_BYTES_CNT_8> down;

    extern std::reference_wrapper<int32_t> lcd_speed_refw;

    // ////////////////////////////////////////////////////////////////////////:w
    void vLEDs(void * /*pvParameters*/);

    void initLcd();
    void lcdSetup();
    void clearLcd();
    void printOutdoorLCD(float &outTemp);
    void showCountDown(uint32_t config_msk);
    void printIndoorLCD(const S_DataOffline &my_data_offline,
                        const S_TimeVar &time_vars, S_PrintInArgs &args,
                        const char *pcTaskName);

    void printTimeLCD(const S_TimeVar &time_vars, S_PrintTimeArgs args,
                      bool &last_blank);

    void offlineTaskPrint(S_DataOffline &my_data, bool &validated_state,
                          const S_OffPrintArgs &arg);

    void onlineTaskPrint(S_DataOnline &my_data, bool conf_active,
                         S_OnPrintArgs &args);

    auto scrollText(const String &text, const S_ScrollTxtArgs &scroll_args)
        -> bool;

    void dispMenu(const S_Menu &menu_n, const S_ClockData &clock_args);

    void vLedPrintLogic(const S_ClockData &clock_data,
                        S_PrintTimeArgs &time_args, S_TimeVar &time_vars,
                        S_VledHelperArgs &vled_helper_args,
                        EventBits_t &event_result,
                        S_DataOffline &my_data_offline, S_PrintInArgs &indoor,
                        const char *pcTaskName);
    void rainingLedCheck(const S_TimeVar &time_vars,
                         TickType_t &xLastRainCheck);

    void printInDateOrCo2(const S_DataOffline &my_data_offline,
                          const S_TimeVar &time_vars, S_PrintInArgs &in_args,
                          const char *pcTaskName);

    void onlinePrintTemps(S_DataOnline &my_data, S_OnPrintArgs &on_print_args);

    void onlinePrintStateUpd(S_OnPrintArgs &on_print_args);

} // namespace N_Graphics

#endif // GRAPHICS_HPP
