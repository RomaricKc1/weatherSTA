#include "graphics.hpp"

namespace N_Graphics {

    std::unique_ptr<LiquidCrystal_I2C> lcd;

    TaskHandle_t xLed = nullptr;

    RTC_DATA_ATTR int32_t LCD_SPD{LCD_SPEED_MS};
    std::reference_wrapper<int32_t> lcd_speed_refw = std::ref(LCD_SPD);

    std::array<byte, LCD_BYTES_CNT_8> degre{B00110, B01001, B01001, B00110,
                                            B00000, B00000, B00000, B00000};
    std::array<byte, LCD_BYTES_CNT_8> up{B00100, B01110, B10101, B00100,
                                         B00100, B00100, B00100, B00100};
    std::array<byte, LCD_BYTES_CNT_8> down{B00100, B00100, B00100, B00100,
                                           B00100, B10101, B01110, B00100};

    void initLcd() {
        try {
            lcd = std::make_unique<LiquidCrystal_I2C>(
                LCD_I2C_ADDR, LCD_MAX_CHARS, LCD_LINES_MAX);
        } catch (const std::exception &e) {
            assert("abort");
        }
    }

    void vLEDs(void * /*pvParameters*/) {
        /**
         * Event info
         * valid_clk: authorization to run the clock mode
         * conf_active: configuration panel activated, need LCD for the menu
         * wait_clk: got signal to start clock mode, now waiting for validation
         */
        const char *pcTaskName = "vLEDs";

        S_DataOffline my_data_offline{};

        S_TimeVar time_vars{};
        time_vars.ringhh = 1;
        time_vars.ringmm = 1;

        S_VledHelperArgs vled_helper_args{};

        EventBits_t event_result{};
        EventBits_t event_res{};
        TickType_t ticktime{xTaskGetTickCount()};
        TickType_t xLastRainCheck{ticktime};

        S_PrintTimeArgs time_args{};

        S_PrintInArgs indoor{};
        indoor.ticktime_buz_alarm = ticktime;

        S_ClockData clock_data{};

        for (;;) {
            vled_helper_args.wait_clk = false;

            bool busy_led{false};
            clock_data.valid_clk = false;

            // request sensors data
            xQueuePeek(N_Common::xQueueOffline,
                       static_cast<void *>(&my_data_offline), 0);

            // managing the clock mode
            /*await valid_clk, conf_active, wait_clk event*/
            event_result = xEventGroupWaitBits(N_Common::handler_event,
                                               ((1U << CONF_ACTIVE_BIT_LED) |
                                                (1U << V_CLK_STATE_BIT_LED) |
                                                (1U << WAIT_CLK_BIT_LED)),
                                               pdFALSE, pdFALSE, 0);

            ((event_result & (1U << V_CLK_STATE_BIT_LED)) != 0U)
                ? clock_data.valid_clk = true
                : clock_data.valid_clk = false;
            ((event_result & (1U << CONF_ACTIVE_BIT_LED)) != 0U)
                ? clock_data.conf_active = true
                : clock_data.conf_active = false;
            ((event_result & (1U << WAIT_CLK_BIT_LED)) != 0U)
                ? vled_helper_args.wait_clk = true
                : vled_helper_args.wait_clk = false;

            vLedPrintLogic(clock_data, time_args, time_vars, vled_helper_args,
                           event_result, my_data_offline, indoor, pcTaskName);

            // hot / cold
            event_res = xEventGroupWaitBits(
                N_Common::handler_event, (1U << LED_BUSY), pdFALSE, pdFALSE, 0);
            ((event_res & (1U << LED_BUSY)) != 0U) ? busy_led = true
                                                   : busy_led = false;

            if (!busy_led) {
                digitalWrite(
                    LED_HOT,
                    (my_data_offline.temp >= TEMP_HOT_COLD_THRES) ? HIGH : LOW);
                digitalWrite(
                    LED_COLD,
                    (my_data_offline.temp >= TEMP_HOT_COLD_THRES) ? LOW : HIGH);
            }

            rainingLedCheck(time_vars, xLastRainCheck);

            vTaskDelayUntil(&ticktime, T_LED_MS);
        }
    }

    void rainingLedCheck(const S_TimeVar &time_vars,
                         TickType_t &xLastRainCheck) {
        // raining or not (check it every TM_CHECK_RAIN unit(s) of duration)
        if ((xTaskGetTickCount() - xLastRainCheck) >= T_CHECK_RAIN_MS) {
            xLastRainCheck = xTaskGetTickCount();
            N_Sensors::driveRainLed();
        }

        if (time_vars.timehh_24_int >= HOURS_22_PM ||
            time_vars.timehh_24_int < HOURS_6_AM) {
            // extend backlit when at night time
            int val = MAGIC_VALUE_BL;
            xQueueOverwrite(N_Common::xXtend_BL_T, &val);
        }
    }

    void vLedPrintLogic(const S_ClockData &clock_data,
                        S_PrintTimeArgs &time_args, S_TimeVar &time_vars,
                        S_VledHelperArgs &vled_helper_args,
                        EventBits_t &event_result,
                        S_DataOffline &my_data_offline, S_PrintInArgs &indoor,
                        const char *pcTaskName) {
        if (clock_data.valid_clk and !clock_data.conf_active and
            !vled_helper_args.wait_clk) {
            /**
             * Prints hours and minutes, with bliking colon
             * Rings at 12 am / 12 pm
             */
            // Get clock data and ring functionality
            N_Helpers::getClockData(time_vars, pcTaskName);

            // ask the lcd to print
            // print time here
            if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
                /*await clear_one event*/
                event_result = xEventGroupWaitBits(N_Common::handler_event,
                                                   ((1U << CLEAR_ONC_LED)),
                                                   pdFALSE, pdFALSE, 0);
                ((event_result & (1U << CLEAR_ONC_LED)) != 0U)
                    ? time_args.clear_once = true
                    : time_args.clear_once = false;

                /*peek on the queue*/
                xQueueReceive(N_Common::x24h_Mode,
                              static_cast<void *>(&time_args.clk_24h_mode), 0);

                printTimeLCD(time_vars, time_args, vled_helper_args.last_blank);
                xSemaphoreGive(N_Common::xLCDAccess);
            }

            // date, temp, humi, co2
            if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
                printIndoorLCD(my_data_offline, time_vars, indoor, pcTaskName);
                xSemaphoreGive(N_Common::xLCDAccess);
            }

            // print out temp here
            if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
                printOutdoorLCD(vled_helper_args.outTemp);
                xSemaphoreGive(N_Common::xLCDAccess);
            }
        }
    }

    void onlinePrintTemps(S_DataOnline &my_data, S_OnPrintArgs &on_print_args) {
        if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
            if ((static_cast<int>(round(my_data.temp))) < TEMP_NEG_9 ||
                (static_cast<int>(round(my_data.tempMin))) < TEMP_NEG_9 ||
                (static_cast<int>(round(my_data.tempMax))) < TEMP_NEG_9) {

                // critial printig stuff
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
                N_Graphics::lcd->print("T:");
                N_Graphics::lcd->setCursor(LCD_CURSOR_2, LCD_LINE_0);
                N_Graphics::lcd->print(static_cast<int>(round(my_data.temp)));
                N_Graphics::lcd->setCursor(LCD_CURSOR_5, LCD_LINE_0);
                N_Graphics::lcd->print(",");

                N_Graphics::lcd->setCursor(LCD_CURSOR_6, LCD_LINE_0);
                N_Graphics::lcd->print(
                    static_cast<int>(round(my_data.tempMin)));
                N_Graphics::lcd->setCursor(LCD_CURSOR_9, LCD_LINE_0);
                N_Graphics::lcd->write(2);
                N_Graphics::lcd->setCursor(LCD_CURSOR_10, LCD_LINE_0);
                N_Graphics::lcd->print(",");

                N_Graphics::lcd->setCursor(LCD_CURSOR_11, LCD_LINE_0);
                N_Graphics::lcd->print(
                    static_cast<int>(round(my_data.tempMax)));
                N_Graphics::lcd->setCursor(LCD_CURSOR_14, LCD_LINE_0);
                N_Graphics::lcd->write(1);
                N_Graphics::lcd->setCursor(LCD_CURSOR_15, LCD_LINE_0);
                N_Graphics::lcd->print(on_print_args.id);
            } else { // general stuff from -9 to 99 °, above 99 lol I don't
                     // care,
                // below -99, I don't care either lol
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
                N_Graphics::lcd->print("TEMP:");
                N_Graphics::lcd->setCursor(LCD_CURSOR_5, LCD_LINE_0);
                N_Graphics::lcd->print(static_cast<int>(round(my_data.temp)));
                N_Graphics::lcd->setCursor(LCD_CURSOR_7, LCD_LINE_0);
                N_Graphics::lcd->print(",");

                N_Graphics::lcd->setCursor(LCD_CURSOR_8, LCD_LINE_0);
                N_Graphics::lcd->print(
                    static_cast<int>(round(my_data.tempMin)));
                N_Graphics::lcd->setCursor(LCD_CURSOR_10, LCD_LINE_0);
                N_Graphics::lcd->write(2);
                N_Graphics::lcd->setCursor(LCD_CURSOR_11, LCD_LINE_0);
                N_Graphics::lcd->print(",");

                N_Graphics::lcd->setCursor(LCD_CURSOR_12, LCD_LINE_0);
                N_Graphics::lcd->print(
                    static_cast<int>(round(my_data.tempMax)));
                N_Graphics::lcd->setCursor(LCD_CURSOR_14, LCD_LINE_0);
                N_Graphics::lcd->write(1);
                N_Graphics::lcd->setCursor(LCD_CURSOR_15, LCD_LINE_0);
                N_Graphics::lcd->print(on_print_args.id);
            }

            xSemaphoreGive(N_Common::xI2C);
        }
    }

    void onlinePrintStateUpd(S_OnPrintArgs &on_print_args) {
        if (on_print_args.read_now) {
            // now
            bool now_int{
                scrollText("                 weather NOW    ",
                           {.delay = N_Graphics::LCD_SPD, .line = LCD_LINE_0})};
            (now_int) ? on_print_args.validated_state_now = false
                      : on_print_args.validated_state_now = true;

            on_print_args.id = "n";
        } else {
            // fcast
            bool fcast_int{
                scrollText("                FORECAST weather",
                           {.delay = N_Graphics::LCD_SPD, .line = LCD_LINE_0})};
            (fcast_int) ? on_print_args.validated_state_fcast = false
                        : on_print_args.validated_state_fcast = true;

            on_print_args.id = "f";
        }
    }

    void onlineTaskPrint(S_DataOnline &my_data, const bool conf_active,
                         S_OnPrintArgs &on_print_args) {
        BaseType_t rc{pdFALSE};
        S_ScrollTxtArgs scroll_args{};

        if (conf_active) // if there IS setting flag set
        {
            return;
        } // should have used simple queue but

        (on_print_args.read_now)
            ? rc = xQueuePeek(N_Common::xQueueOn_NOW,
                              static_cast<void *>(&my_data), 0)
            : rc = xQueuePeek(N_Common::xQueueOn_FCAST,
                              static_cast<void *>(&my_data), 0);

        if (rc == pdPASS and
            (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS)) {
            clearLcd();

            scroll_args.delay = N_Graphics::LCD_SPD;
            scroll_args.line  = LCD_LINE_0;

            // update state
            onlinePrintStateUpd(on_print_args);

            clearLcd();

            onlinePrintTemps(my_data, on_print_args);

            // others
            String text2{
                "                Feels like: " +
                static_cast<String>(my_data.feels_like) +
                static_cast<String>(static_cast<char>(CHAR_CODE_DEGRE)) +
                ", Humidity: " +
                static_cast<String>(static_cast<int>(my_data.humidity)) +
                "%, Pressure: " +
                static_cast<String>(static_cast<int>(my_data.pressure)) +
                "hPa, Wind Speed: " + static_cast<String>(my_data.windSpeed) +
                "m/s, Cloud coverage: " +
                static_cast<String>(static_cast<int>(my_data.clouds)) + "% " +
                static_cast<String>(my_data.description)};

            scroll_args.line = 1;
            bool interrupted{scrollText(text2, scroll_args)};
            if (on_print_args.id == "n" and interrupted) {
                on_print_args.validated_state_now = false;
            } else if (on_print_args.id == "f" and interrupted) {
                on_print_args.validated_state_fcast = false;
            }

            xSemaphoreGive(N_Common::xLCDAccess);
        }

        if (on_print_args.validated_state_now and on_print_args.id == "n") {
            rc =
                xTaskNotify(N_Sys::xTaskDaemon, (1U << SYSNOTIF_NOW), eSetBits);
            assert(rc == pdPASS);
        }

        if (on_print_args.validated_state_fcast and on_print_args.id == "f") {
            rc = xTaskNotify(N_Sys::xTaskDaemon, (1U << SYSNOTIF_FCAST),
                             eSetBits);
            assert(rc == pdPASS);
        }

        on_print_args.read_now = !on_print_args.read_now;
    }

    void offlineTaskPrint(S_DataOffline &my_data, bool &validated_state,
                          const S_OffPrintArgs &arg) {
        BaseType_t rc{pdFALSE};
        S_ScrollTxtArgs scroll_args{};

        if (arg.clock_data.conf_active) // if there IS setting flag set
        {
            return;
        }

        rc = xQueuePeek(N_Common::xQueueOffline, static_cast<void *>(&my_data),
                        0);
        if (rc == pdPASS and
            (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS)) {
            clearLcd();

            scroll_args.delay = N_Graphics::LCD_SPD;
            scroll_args.line  = LCD_LINE_0;
            bool interrupted_{
                scrollText("                Indoors weather", scroll_args)};
            (interrupted_) ? validated_state = false : validated_state = true;

            clearLcd();

            // print static thing
            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
                N_Graphics::lcd->print("TEMP:");
                N_Graphics::lcd->setCursor(LCD_CURSOR_6, LCD_LINE_0);
                N_Graphics::lcd->print("     ");
                N_Graphics::lcd->setCursor(LCD_CURSOR_6, LCD_LINE_0);
                N_Graphics::lcd->print(my_data.temp);
                N_Graphics::lcd->setCursor(LCD_CURSOR_11, LCD_LINE_0);
                N_Graphics::lcd->write(static_cast<byte>(0));
                N_Graphics::lcd->setCursor(LCD_CURSOR_12, LCD_LINE_0);
                N_Graphics::lcd->print("C");

                xSemaphoreGive(N_Common::xI2C);
            }

            String co2_text{(my_data.co2 == INVALID_CO2_VAL)
                                ? "..."
                                : static_cast<String>(my_data.co2)};
            String text2{
                "                Humidity: " +
                static_cast<String>(my_data.humi) + "%, Co2: " + co2_text +
                "ppm, Altitude: " + static_cast<String>(my_data.altitude) +
                "m, Pressure: " + static_cast<String>(my_data.pressure) +
                "hPa"};

            scroll_args.line = LCD_LINE_1;
            bool interrupted{scrollText(text2, scroll_args)};
            (interrupted) ? validated_state = false : validated_state = true;

            xSemaphoreGive(N_Common::xLCDAccess);
        }

        if (validated_state) {
            rc = xTaskNotify(N_Sys::xTaskDaemon, (1U << SYSNOTIF_IN), eSetBits);
            assert(rc == pdPASS);
        }
    }

    void printTimeLCD(const S_TimeVar &time_vars,
                      const S_PrintTimeArgs time_args, bool &last_blank) {
        if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdFALSE) {
            return;
        }

        /*clear_one event*/
        if (time_args.clear_once) {
            xEventGroupClearBits(N_Common::handler_event,
                                 ((1U << CLEAR_ONC_LED)));

            N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
            N_Graphics::lcd->print("                ");
            N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
            N_Graphics::lcd->print("                ");
        }

        if (!time_args.clk_24h_mode) {
            if (time_vars.timehh_int < HOUR_10) {
                N_Graphics::lcd->setCursor(LCD_CURSOR_4, LCD_LINE_0);
                N_Graphics::lcd->print(" ");
                N_Graphics::lcd->setCursor(LCD_CURSOR_5, LCD_LINE_0);
                N_Graphics::lcd->print(time_vars.timehh_int);
            } else {
                N_Graphics::lcd->setCursor(LCD_CURSOR_4, LCD_LINE_0);
                N_Graphics::lcd->print(time_vars.timehh_int);
            }
        } else {
            if (time_vars.timehh_24_int < HOUR_10) {
                N_Graphics::lcd->setCursor(LCD_CURSOR_4, LCD_LINE_0);
                N_Graphics::lcd->print("0");
                N_Graphics::lcd->setCursor(LCD_CURSOR_5, LCD_LINE_0);
                N_Graphics::lcd->print(time_vars.timehh_24_int);
            } else {
                N_Graphics::lcd->setCursor(LCD_CURSOR_4, LCD_LINE_0);
                N_Graphics::lcd->print(time_vars.timehh_24_int);
            }
        }
        // : separator
        N_Graphics::lcd->setCursor(LCD_CURSOR_6, LCD_LINE_0);
        if (last_blank) {
            N_Graphics::lcd->print(":");
            last_blank = false;
        } else {
            N_Graphics::lcd->print(" ");
            last_blank = true;
        }

        N_Graphics::lcd->setCursor(LCD_CURSOR_7, LCD_LINE_0);
        N_Graphics::lcd->print(time_vars.timemm.data());
        N_Graphics::lcd->setCursor(LCD_CURSOR_9, LCD_LINE_0);

        if (!time_args.clk_24h_mode) {
            (time_vars.pm) ? N_Graphics::lcd->print("pm")
                           : N_Graphics::lcd->print("am");
        } else {
            N_Graphics::lcd->print("  ");
        }

        xSemaphoreGive(N_Common::xI2C);
    }

    void printInDateOrCo2(const S_DataOffline &my_data_offline,
                          const S_TimeVar &time_vars, S_PrintInArgs &in_args,
                          const char *pcTaskName) {
        BaseType_t rc{pdFALSE};

        if (in_args.was_co2) {
            in_args.ticktime_date++;
            if (in_args.ticktime_date >= TICK_CNT_TO_CO2_PRINT) {
                in_args.ticktime_date = 0;
                // pass it to the co2 indicator
                in_args.was_co2 = false;
            }
            // print date
            if (time_vars.timedd_int < DAY_10) {
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
                N_Graphics::lcd->print(" ");
                N_Graphics::lcd->setCursor(LCD_CURSOR_1, LCD_LINE_1);
                N_Graphics::lcd->print(time_vars.timedd_int);
            } else {
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
                N_Graphics::lcd->print(time_vars.timedd_int);
            }

            N_Graphics::lcd->setCursor(LCD_CURSOR_3, LCD_LINE_1);
            N_Graphics::lcd->print(time_vars.timemomo.data());

            auto this_idx = strlen(time_vars.timemomo.data());
            while (this_idx + LCD_CURSOR_3 < LCD_CURSOR_11) {
                N_Graphics::lcd->print(" ");
                this_idx += 1;
            }
        } else {
            in_args.ticktime_server++;
            if (in_args.ticktime_server >= (T_SENSORS_MS / T_LED_MS)) {
                in_args.ticktime_server = 0;
                // every sensor duration, read the queue, peek the data,
                rc = xQueuePeek(N_Common::xInfluxDB_state,
                                static_cast<void *>(&in_args.db_server_state),
                                0);
            }

            if ((N_Common::debug_msk & (1U << DEBUG_DB_SERVER)) != 0 and
                rc == pdPASS) {
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Recvd server state: " +
                             static_cast<String>(
                                 static_cast<int>(in_args.db_server_state))});
            }

            // print dbserver status
            N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
            (in_args.db_server_state) ? N_Graphics::lcd->print(":)")
                                      : N_Graphics::lcd->print(":(");

            in_args.ticktime_co2++;
            if (in_args.ticktime_co2 >= TICKS_CNT_TO_MONTH_PRINT) {
                in_args.ticktime_co2 = 0;
                // pass it to the date indicator
                in_args.was_co2 = true;
            }

            // print co2 level
            uint32_t used_chars{};
            N_Graphics::lcd->setCursor(LCD_CURSOR_3, LCD_LINE_1);
            if (my_data_offline.co2 == INVALID_CO2_VAL) {
                N_Graphics::lcd->print("...");
                used_chars = 3;
            } else {
                N_Graphics::lcd->print(my_data_offline.co2);
                used_chars = static_cast<String>(my_data_offline.co2).length();
            }
            String format_co2_unit{"ppm"};
            for (int i = 0; i < LCD_CURSOR_11 - (LCD_CURSOR_3 + used_chars +
                                                 format_co2_unit.length());
                 i++) {
                format_co2_unit += ' ';
            }
            N_Graphics::lcd->print(format_co2_unit);
        }
    }

    void printIndoorLCD(const S_DataOffline &my_data_offline,
                        const S_TimeVar &time_vars, S_PrintInArgs &in_args,
                        const char *pcTaskName) {
        if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdFALSE) {
            return;
        }

        printInDateOrCo2(my_data_offline, time_vars, in_args, pcTaskName);

        // alert on Co2 high values, during daytime for every
        // CO2_ALARM_REP_MM minutes
        if ((time_vars.timehh_24_int < HOUR_23 &&
             time_vars.timehh_24_int >= HOUR_9) &&
            ((xTaskGetTickCount() - in_args.ticktime_buz_alarm) >=
             T_CO2_ALARM_REP_MS) &&
            (my_data_offline.co2 >= HEALTHY_CO2_THRES)) {
            // buz here
            in_args.ticktime_buz_alarm = xTaskGetTickCount();

            for (int i = 0; i < 2; i++) {
                digitalWrite(BUZZ, 1);
                vTaskDelay(pdMS_TO_TICKS(T_40_MS + (i * TEN_MS_X)));
                digitalWrite(BUZZ, 0);
                vTaskDelay(pdMS_TO_TICKS(T_40_MS + (i * TEN_MS_X)));
            }
        }

        // temp and humi
        N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
        N_Graphics::lcd->print(static_cast<int>(round(my_data_offline.temp)));

        if (static_cast<int>(round(my_data_offline.temp)) >= TEMP_0 &&
            static_cast<int>(round(my_data_offline.temp)) < TEMP_10) {
            N_Graphics::lcd->setCursor(LCD_CURSOR_1, LCD_LINE_0);
            N_Graphics::lcd->write(static_cast<byte>(0));
            N_Graphics::lcd->setCursor(LCD_CURSOR_2, LCD_LINE_0);
            N_Graphics::lcd->print("  ");
        } else if ((static_cast<int>(round(my_data_offline.temp)) < TEMP_0 &&
                    static_cast<int>(round(my_data_offline.temp)) >
                        TEMP_NEG_10) ||
                   (static_cast<int>(round(my_data_offline.temp)) >= TEMP_10)) {
            N_Graphics::lcd->setCursor(LCD_CURSOR_2, LCD_LINE_0);
            N_Graphics::lcd->write(static_cast<byte>(0));
            N_Graphics::lcd->setCursor(LCD_CURSOR_3, LCD_LINE_0);
            N_Graphics::lcd->print(" ");
        } else if (static_cast<int>(round(my_data_offline.temp)) <
                   TEMP_NEG_9) { //-10, -11, ...
            N_Graphics::lcd->setCursor(LCD_CURSOR_3, LCD_LINE_0);
            N_Graphics::lcd->write(static_cast<byte>(0));
        }

        // humi
        N_Graphics::lcd->setCursor(LCD_CURSOR_11, LCD_LINE_0);
        if (static_cast<int>(my_data_offline.humi) > HUMI_FULL) {
            String humi_thing = " err";
            N_Graphics::lcd->print(humi_thing);
        } else if (static_cast<int>(my_data_offline.humi) <= HUMI_FULL - 1) {
            String humi_thing{
                "  " +
                static_cast<String>(static_cast<int>(my_data_offline.humi)) +
                "%"};
            N_Graphics::lcd->print(humi_thing);
        } else {
            String humi_thing{
                " " +
                static_cast<String>(static_cast<int>(my_data_offline.humi)) +
                "%"};
            N_Graphics::lcd->print(humi_thing);
        }

        xSemaphoreGive(N_Common::xI2C);
    }

    void printOutdoorLCD(float &outTemp) {
        if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdFALSE) {
            return;
        }
        if (xQueuePeek(N_Common::xTempOut, (static_cast<void *>(&outTemp)),
                       0) == pdFAIL) {
            // make sure not to take the i2c and not return it before returning
            xSemaphoreGive(N_Common::xI2C);
            return;
        }

        // out temp 12 13 14 15
        //          -  " "  x  °  < 0  - auto
        //          +   y   x  °  > 10 + auto

        N_Graphics::lcd->setCursor(LCD_CURSOR_12, LCD_LINE_1);
        N_Graphics::lcd->print(static_cast<int>(round(outTemp)));

        if ((static_cast<int>(round(outTemp))) >= TEMP_0 and
            (static_cast<int>(round(outTemp)) < TEMP_10)) { // 0, .. 9
            N_Graphics::lcd->setCursor(LCD_CURSOR_13, LCD_LINE_1);
            N_Graphics::lcd->write(static_cast<byte>(0));
            N_Graphics::lcd->setCursor(LCD_CURSOR_14, LCD_LINE_1);
            N_Graphics::lcd->print("  ");
        } else if ((static_cast<int>(round(outTemp)) < TEMP_0 &&
                    static_cast<int>(round(outTemp)) > TEMP_NEG_10) ||
                   (static_cast<int>(round(outTemp)) >=
                    TEMP_10)) { //-9, ..., -1   or   10, 11, 12, ...
            N_Graphics::lcd->setCursor(LCD_CURSOR_14, LCD_LINE_1);
            N_Graphics::lcd->write(static_cast<byte>(0));
            N_Graphics::lcd->setCursor(LCD_CURSOR_15, LCD_LINE_1);
            N_Graphics::lcd->print(" ");
        } else if (static_cast<int>(round(outTemp)) <
                   TEMP_NEG_9) { //-10, -11, ...
            N_Graphics::lcd->setCursor(LCD_CURSOR_15, LCD_LINE_1);
            N_Graphics::lcd->write(static_cast<byte>(0));
        }

        xSemaphoreGive(N_Common::xI2C);
    }

    void dispMenu(const S_Menu &menu_n, const S_ClockData &clock_args) {
        if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdFAIL) {
            return;
        }

        N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
        N_Graphics::lcd->print("                ");
        N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
        N_Graphics::lcd->print("                ");

        N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
        N_Graphics::lcd->print(menu_n.menu.at(menu_n.current_menu).at(0));

        N_Graphics::lcd->setCursor(LCD_CURSOR_4,
                                   LCD_LINE_1); // center a little bit

        // these values has been updated by the exec buttton
        switch (menu_n.current_menu) {
            case MENU_1: { // lcd speed
                N_Graphics::lcd->print(N_Graphics::LCD_SPD);
            } break;

            case MENU_2: { // sleep duration
                N_Graphics::lcd->print(N_Sys::IDLE_TMS);
            } break;

            case MENU_3: { // deep sleep duration
                N_Graphics::lcd->print(N_Sys::SLEEP_DURATION_M);
            } break;

            case MENU_4: { // menu-config 24h mode
                if (clock_args.clk_24_mode) {
                    N_Graphics::lcd->print("24H mode");
                } else {
                    N_Graphics::lcd->print("12H mode");
                }
            } break;

            case MENU_5: { // menu-config change wlan
                if (clock_args.chg_wlan) {
                    N_Graphics::lcd->print("Yes");
                } else {
                    N_Graphics::lcd->print("No");
                }
            } break;

            case MENU_0: { // menu-config clock mode
                if (clock_args.clk_mode) {
                    N_Graphics::lcd->print("ON");
                } else {
                    N_Graphics::lcd->print("OFF");
                }
            } break;

            default: {
            } break;
        }

        N_Graphics::lcd->setCursor(LCD_CURSOR_10,
                                   LCD_LINE_1); // center a little bit
        N_Graphics::lcd->print(menu_n.menu.at(menu_n.current_menu).at(1));

        xSemaphoreGive(N_Common::xI2C);
    }

    auto scrollText(const String &text, const S_ScrollTxtArgs &scroll_args)
        -> bool {
        // scroll the text
        bool conf_active{false};
        EventBits_t event_result{};
        TickType_t xLastWakeTime{};

        event_result = xEventGroupWaitBits(N_Common::handler_event,
                                           ((1U << CONF_ACTIVE_BIT_SCR)),
                                           pdFALSE, pdFALSE, 0);
        ((event_result & (1U << CONF_ACTIVE_BIT_SCR)) != 0U)
            ? conf_active = true
            : conf_active = false;

        if (conf_active) {
            return true;
        }

        // TODO(romaric) {fix logic for efficiency}
        for (int i = 0; i <= (text.length() - LCD_MAX_CHARS); i++) {
            xLastWakeTime = xTaskGetTickCount();

            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {

                N_Graphics::lcd->setCursor(LCD_CURSOR_0, scroll_args.line);

                for (int j = i; j <= i + LCD_MAX_CHARS - 1; j++) {
                    N_Graphics::lcd->print(text[j]);
                }

                N_Graphics::lcd->print(" ");

                xSemaphoreGive(N_Common::xI2C);
            }

            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(scroll_args.delay));
            event_result = xEventGroupWaitBits(N_Common::handler_event,
                                               ((1U << CONF_ACTIVE_BIT_SCR)),
                                               pdFALSE, pdFALSE, 0);
            ((event_result & (1U << CONF_ACTIVE_BIT_SCR)) != 0U)
                ? conf_active = true
                : conf_active = false;

            if (conf_active) {
                return true;
            }
        }

        vTaskDelay(
            pdMS_TO_TICKS(T_500_MS)); // give it 500ms before clearing the text

        return false;
    }

    void lcdSetup() {
        // LCD
        N_Graphics::lcd->init();
        N_Graphics::lcd->backlight();
        N_Graphics::lcd->createChar(0, N_Graphics::degre.data());
        N_Graphics::lcd->createChar(1, N_Graphics::up.data());
        N_Graphics::lcd->createChar(2, N_Graphics::down.data());
    }

    void clearLcd() {
        if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
            N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
            N_Graphics::lcd->print("                ");
            N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
            N_Graphics::lcd->print("                ");

            xSemaphoreGive(N_Common::xI2C);
        }
    }

    void showCountDown(const uint32_t config_msk) {
        // show countdown
        if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
            if (config_msk < COUNT_DOWN_INIT) // 9, 8, ...
            {
                N_Graphics::lcd->setCursor(LCD_CURSOR_14, LCD_LINE_0);
                N_Graphics::lcd->print(0);
                N_Graphics::lcd->setCursor(LCD_CURSOR_15, LCD_LINE_0);
            } else {
                N_Graphics::lcd->setCursor(LCD_CURSOR_14, LCD_LINE_0);
            }
            N_Graphics::lcd->print(config_msk);

            xSemaphoreGive(N_Common::xI2C);
        }
    }

} // namespace N_Graphics
