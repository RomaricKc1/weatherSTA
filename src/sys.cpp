#include "sys.hpp"

namespace N_Sys {
    RTC_DATA_ATTR int boot_times{};
    RTC_DATA_ATTR int64_t SLEEP_DURATION_M{T_ESP_SLEEP_MIN}; // 15 mins
    RTC_DATA_ATTR bool RebootOnClock{false};
    int64_t IDLE_TMS{T_15_S_MS};

    std::reference_wrapper<int64_t> sleep_duration_m_refw =
        std::ref(SLEEP_DURATION_M);

    TaskHandle_t xTaskOnline  = nullptr;
    TaskHandle_t xTaskOffline = nullptr;
    TaskHandle_t xTaskDaemon  = nullptr;
    TaskHandle_t xWelcome     = nullptr;

    std::array<std::array<int32_t, MENU_INNER_6>, MENUS> config_set = {{
        {1, 0, 1, 0, 1, 0}, // clock 0's
        {T_250_MS, T_200_MS, T_300_MS, T_400_MS, T_450_MS,
         T_500_MS},         // lcd_speed 1st
        {T_30_S_MS, T_20_S_MS, T_25_S_MS, T_40_S_MS, T_50_S_MS,
         T_60_S_MS},        // sleep_T 2nd
        {T_15_MIN, T_20_MIN, T_30_MIN, T_35_MIN, T_45_MIN,
         T_60_MIN},         // deep_sleep_T
        {0, 1, 0, 1, 0, 1}, // clock AM/PM
        {0, 1, 0, 1, 0, 1}, // wlan portal ondemand 5th
    }};

    void vTaskOffline(void * /*pvParameters*/) {
        S_DataOffline my_data{};

        bool validated_state{false};
        EventBits_t event_result{};
        S_OffPrintArgs offprint{};

        vTaskDelay(pdMS_TO_TICKS(T_100_MS));

        for (;;) {
            offprint.clock_data.conf_active = false;
            bool idle{};
            /**
             * recvd data from queue read offline data
             * rcv and clear the bit CONF_ACTIVE_BIT and SYSSTATE_IDLE_BIT, if
             * idle, suspend yo self, if CONF_ACTIVE_BIT == 1, don't print smth
             * if idle state expire, someone will wake you up lol
             */
            // event listener

            event_result = xEventGroupWaitBits(N_Common::handler_event,
                                               ((1U << SYSSTATE_IDLE_BIT_OFF)),
                                               pdFALSE, pdFALSE, 0);
            ((event_result & (1U << SYSSTATE_IDLE_BIT_OFF)) != 0U)
                ? idle = true
                : idle = false;

            if (idle) {
                vTaskSuspend(nullptr);
            }

            event_result = xEventGroupWaitBits(N_Common::handler_event,
                                               ((1U << CONF_ACTIVE_BIT_OFF)),
                                               pdFALSE, pdFALSE, 0);
            ((event_result & (1U << CONF_ACTIVE_BIT_OFF)) != 0U)
                ? offprint.clock_data.conf_active = true
                : offprint.clock_data.conf_active = false;
            N_Graphics::offlineTaskPrint(my_data, validated_state, offprint);

            vTaskDelay(pdMS_TO_TICKS(T_OFFLINE_MS));
        }
    }

    void vTaskOnline(void * /*pvParameters*/) {
        S_DataOnline my_data{};

        S_OnPrintArgs online_print{};
        online_print.id       = "n";
        online_print.read_now = true;

        EventBits_t event_result{};

        vTaskDelay(pdMS_TO_TICKS(T_500_MS));

        for (;;) {
            bool conf_active{};
            bool idle{};
            /**
             * recvd data from queue read offline data
             * rcv and clear the bit CONF_ACTIVE_BIT and SYSSTATE_IDLE_BIT, if
             * idle, suspend yo self, if CONF_ACTIVE_BIT == 1, don't print smth
             * if idle state expire, someone will wake you up lol
             */
            // event listener

            event_result = xEventGroupWaitBits(N_Common::handler_event,
                                               ((1U << SYSSTATE_IDLE_BIT_ON)),
                                               pdFALSE, pdFALSE, 0);
            ((event_result & (1U << SYSSTATE_IDLE_BIT_ON)) != 0U)
                ? idle = true
                : idle = false;

            if (idle) {
                vTaskSuspend(nullptr);
            }

            event_result = xEventGroupWaitBits(N_Common::handler_event,
                                               ((1U << CONF_ACTIVE_BIT_ON)),
                                               pdFALSE, pdFALSE, 0);
            ((event_result & (1U << CONF_ACTIVE_BIT_ON)) != 0U)
                ? conf_active = true
                : conf_active = false;

            N_Graphics::onlineTaskPrint(my_data, conf_active, online_print);

            vTaskDelay(pdMS_TO_TICKS(T_ONLINE_MS));
        }
    }

    void vDaemon(void * /*pvParameters*/) {
        int nb_loop{};
        bool idle{};

        EventBits_t event_result{};
        const char *pcTaskName = "vDaemon";

        S_IdleArgs idle_args{};

        S_IdCreateTimer timer_id{};
        timer_id.pcTaskName = pcTaskName;
        timer_id.timerName  = "one-x-timer";

        /**
         * Notification stuff
         */
        S_TaskNotifArgs notif_args{};

        for (;;) {
            // event initializer
            xTaskNotifyWait(0xFF, pdFALSE, &notif_args.rv, 0);
            taskNotifMan(notif_args, idle);

            vDaemonInitiateSleepCheck(idle_args, pcTaskName);

            vDaemonInitiateIdleCheck(idle_args, event_result, idle);

            if (idle_args.check_go_idle_now) {
                idle_args.check_go_idle_now = false;
                // go idle mode
                xQueueReceive(N_Common::xIdle_T,
                              static_cast<void *>(&N_Sys::IDLE_TMS), 0);

                if ((!(idle_args.rep > 1)) and
                    (static_cast<int>(createTimer(N_Sys::IDLE_TMS, timerDaemon,
                                                  timer_id)) == pdPASS)) {
                    // timer started and announce
                    xEventGroupSetBits(
                        N_Common::handler_event,
                        ((1U << BTN_WAIT_MODE_BIT))); // going idle mode, don't
                                                      // check input state
                }
            }
            nb_loop++;
            if (nb_loop >= DEAMON_REP_CNT_THRES) {
                if (getLocalTime(&N_Common::STimeInfo)) {
                    size_t result = strftime(
                        N_Common::formated_time.data(), MAX_CHAR_BUF_TIME,
                        "Last known time: %I:%M%p.", &N_Common::STimeInfo);

                    if (result == 0) {
                        if ((N_Common::debug_msk & (1U << DEBUG_DAEMON)) != 0) {
                            N_Helpers::idAndMsg(
                                {.id = pcTaskName, .word = "strftime failed"});
                        }
                    }
                }
                nb_loop = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(T_DAEMON_MS));
        }
    }

    void vTaskWelcome(void * /*pvParameters*/) {
        // shows welcome message while esp is connecting
        S_ScrollTxtArgs scroll_args{};

        scroll_args.delay = N_Graphics::LCD_SPD;
        scroll_args.line  = LCD_LINE_0;

        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
            String msg{"    Weather Station/Clock v"};
            N_Graphics::scrollText(
                msg + static_cast<String>(FIRMWARE_VERSION.data()),
                scroll_args);

            N_Graphics::clearLcd();
            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
                N_Graphics::lcd->print("SSID:");
                N_Graphics::lcd->setCursor(LCD_CURSOR_6, LCD_LINE_0);
                N_Graphics::lcd->print(N_WM::AP_NAME);

                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
                N_Graphics::lcd->print("PSWD:");
                N_Graphics::lcd->setCursor(LCD_CURSOR_6, LCD_LINE_1);
                N_Graphics::lcd->print(N_WM::AP_PASWD);

                xSemaphoreGive(N_Common::xI2C);
            }
            xSemaphoreGive(N_Common::xLCDAccess);
        }

        // kill himself
        vTaskSuspend(nullptr);
        vTaskDelete(nullptr);
    }

    void vDaemonInitiateSleepCheck(S_IdleArgs &idle_args,
                                   const char *pcTaskName) {
        if (idle_args.rep > 1 and
            (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS)) {
            idle_args.rep = 0;

            // deep sleep with wake from specific time
            if ((N_Common::debug_msk & (1U << DEBUG_DAEMON)) != 0) {
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Going to sleep for " +
                             static_cast<String>(N_Sys::SLEEP_DURATION_M) +
                             "mins. LOL cya.."});
            }

            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->display();
                N_Graphics::lcd->backlight();

                N_Graphics::lcd->setCursor(0, 0);
                N_Graphics::lcd->print("                ");
                N_Graphics::lcd->setCursor(0, 1);
                N_Graphics::lcd->print("                ");

                N_Graphics::lcd->setCursor(0, 0);
                N_Graphics::lcd->print("going to sleep..");
                N_Graphics::lcd->setCursor(LCD_CURSOR_6, 1);
                N_Graphics::lcd->print("bye!"); // 6	4	 6

                xSemaphoreGive(N_Common::xI2C);
            }

            vTaskDelay(pdMS_TO_TICKS(T_3_S_MS));

            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->noDisplay();
                N_Graphics::lcd->noBacklight();

                xSemaphoreGive(N_Common::xI2C);
            }

            digitalWrite(BUZZ, 1);
            vTaskDelay(pdMS_TO_TICKS(T_50_MS));
            digitalWrite(BUZZ, 0);
            Serial.flush();

            esp_deep_sleep_start();
            // no further execution until reboot
        }
    }

    void vDaemonInitiateIdleCheck(S_IdleArgs &idle_args,
                                  EventBits_t &event_result, const bool &idle) {
        if (idle or idle_args.hard_resume) {
            // every thing done once, idle on for every mode
            xEventGroupSetBits(
                N_Common::handler_event,
                ((1U << SYSSTATE_IDLE_BIT_OFF) | (1U << SYSSTATE_IDLE_BIT_ON)));

            event_result = xEventGroupWaitBits(N_Common::handler_event,
                                               ((1U << CONF_ACTIVE_BIT_DAE)),
                                               pdFALSE, pdFALSE, 0);
            ((event_result & (1U << CONF_ACTIVE_BIT_DAE)) != 0U)
                ? idle_args.clock_data.conf_active = true
                : idle_args.clock_data.conf_active = false;

            idleSTAMode(idle_args);
        }
    }

    void releaserDaemon(TimerHandle_t xTimer) {
        bool clk_mode{false};
        bool chg_config{false};
        xEventGroupClearBits(N_Common::handler_event,
                             ((1U << WAIT_CLK_BIT_LED) |
                              (1U << TIMER_FLAG_BIT) |
                              (1U << BTN_WAIT_MODE_BIT)));

        xEventGroupSetBits(N_Common::handler_event, ((1U << CLEAR_ONC_LED)));

        xQueueReceive(N_Common::xClockMode, static_cast<void *>(&clk_mode), 0);
        xQueueReceive(N_Common::xPortalMode, static_cast<void *>(&chg_config),
                      0);

        if (clk_mode) {
            // aka clock mode
            // dont release anybody lol, tmr_flag cleared, nice okay
            xEventGroupSetBits(
                N_Common::handler_event,
                ((1U << V_CLK_STATE_BIT_SET) |
                 (1U << V_CLK_STATE_BIT_LED))); // validate the mode now
        } else {
            xEventGroupClearBits(
                N_Common::handler_event,
                ((1U << V_CLK_STATE_BIT_SET) | (1U << V_CLK_STATE_BIT_LED) |
                 (1U << CONF_ACTIVE_BIT_ON) | (1U << CONF_ACTIVE_BIT_OFF)));
            xEventGroupClearBits(
                N_Common::handler_event,
                ((1U << CONF_ACTIVE_BIT_LED) | (1U << CONF_ACTIVE_BIT_SCR) |
                 (1U << CONF_ACTIVE_BIT_DAE) | (1U << CONF_ACTIVE_BIT_SET)));

            vTaskResume(N_Sys::xTaskDaemon);
            vTaskResume(N_Sys::xTaskOnline);
            vTaskResume(N_Sys::xTaskOffline);
        }

        if (chg_config) {
            xEventGroupSetBits(
                N_Common::handler_event,
                ((1U << V_START_PORTAL_BIT_SET))); // validate the portal mode
                                                   // now stop others ta ,
                                                   // cause, we won't need them
                                                   // until next reboot
        } else {
            xEventGroupClearBits(N_Common::handler_event,
                                 ((1U << V_START_PORTAL_BIT_SET)));
            if (!clk_mode) {
                // prevent this unchanged parameter (false by default) to start
                // tasks that should be stopped in clock mode
                vTaskResume(N_Sys::xTaskDaemon);
                vTaskResume(N_Sys::xTaskOnline);
                vTaskResume(N_Sys::xTaskOffline);
                // else, do not resume any
            }
        }
    }

    void timerDaemon(TimerHandle_t xTimer) {
        // called when timer expires
        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
            xEventGroupClearBits(N_Common::handler_event,
                                 ((1U << SYSSTATE_IDLE_BIT_OFF) |
                                  (1U << SYSSTATE_IDLE_BIT_ON) |
                                  (1U << BTN_WAIT_MODE_BIT)));

            vTaskResume(N_Sys::xTaskOnline);
            vTaskResume(N_Sys::xTaskOffline);

            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->clear();
                N_Graphics::lcd->display();
                N_Graphics::lcd->backlight();
                xSemaphoreGive(N_Common::xI2C);
            }
            xSemaphoreGive(N_Common::xLCDAccess);
        }
    }

    void checkCExecPortal(const bool valid_portal) {
        if (!valid_portal) {
            return;
        }

        vTaskSuspend(N_Sys::xTaskOffline);
        vTaskSuspend(N_Sys::xTaskOnline);
        vTaskSuspend(N_Sys::xTaskDaemon);

        N_WebServer::server->end();
        // we need LCD permission

        S_ScrollTxtArgs scroll_args{};
        scroll_args.delay = N_Graphics::LCD_SPD;
        scroll_args.line  = LCD_LINE_0;

        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
            N_Graphics::clearLcd();
            N_Graphics::scrollText("        Launching config portal...degre",
                                   scroll_args);

            scroll_args.line = LCD_LINE_1;
            N_Graphics::scrollText("        Hit Restart at the end.",
                                   scroll_args);

            // first notify wifi disconnected and stop the server
            xEventGroupClearBits(
                N_Common::handler_event,
                ((1U << WLAN_CONTD_BIT))); // stop sending http requests

            N_WM::wm->setConfigPortalTimeout(T_PORTAL_S);
            bool res{
                N_WM::wm->startConfigPortal(N_WM::AP_NAME, N_WM::AP_PASWD)};

            scroll_args.line = LCD_LINE_0;
            if (!res) {
                N_Graphics::clearLcd();
                N_Graphics::scrollText("        failed to connect or timed out",
                                       scroll_args);
                vTaskResume(N_Sys::xTaskOffline);
                vTaskResume(N_Sys::xTaskOnline);
                vTaskResume(N_Sys::xTaskDaemon);
            }
            xEventGroupClearBits(N_Common::handler_event,
                                 (1U << V_START_PORTAL_BIT_SET));
            xSemaphoreGive(N_Common::xLCDAccess);
        }
    }

    void checkWifiState(S_WifiCoArgs &co_args) {
        bool go_reboot{};
        if (((xTaskGetTickCount() - co_args.xTick_wlan_state) >= T_60_S_MS) and

            (WiFiClass::status() != WL_CONNECTED)) {
            co_args.xTick_wlan_state = xTaskGetTickCount();

            // notify this event
            xEventGroupClearBits(N_Common::handler_event,
                                 ((1U << WLAN_CONTD_BIT)));
            // N_Helpers::idAndMsg({.id=pcTaskName, .word="Troubles wifi: " +
            // (String)WiFi.status()});

            if (WiFiClass::status() == WL_DISCONNECTED) {
                WiFi.reconnect();
            }

            if (co_args.clock_data.valid_clk) {
                co_args.cnt += 1; // start counting if in clock mode

                xEventGroupSetBits(N_Common::handler_event, ((1U << LED_BUSY)));
                co_args.flicker_led = true;

                digitalWrite(LED_COLD, HIGH);
                digitalWrite(LED_HOT, HIGH);
                co_args.last_flick_state = HIGH;

                if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0) {
                    N_Helpers::idAndMsg(
                        {.id   = "vTaskSetup",
                         .word = "took ctrl of led, starting flickering"});
                }

                // signal that led is now being used to indicat connection
                // issues
                co_args.xLastLedOn    = xTaskGetTickCount();
                co_args.xtick_unknown = xTaskGetTickCount();

                if ((co_args.cnt >
                     (WIFI_CONNECTION_ISSUE_X /
                      static_cast<int>(T_LED_MS * 2 / T_1000_MS))) &&
                    !co_args.reboot_reconfd) {
                    // 10 mins (every minute, 10x) count greater than 10 mins?
                    go_reboot = true;
                }
            }
        } else {
            if (WiFiClass::status() == WL_CONNECTED) {
                co_args.cnt =
                    0; // if wifi gets co again, clear the counting value
                xEventGroupSetBits(N_Common::handler_event,
                                   ((1U << WLAN_CONTD_BIT)));
                xEventGroupClearBits(
                    N_Common::handler_event,
                    ((1U << LED_BUSY))); // also clear this when coed
            }
        }

        if (go_reboot) {
            N_Sys::RebootOnClock = true;
            esp_sleep_enable_timer_wakeup(static_cast<int64_t>(T_10_SEC) *
                                          US_2_S);
            // these will assure deep sleep and wake up to clock mode again
            // after 10
            // s
            co_args.reboot_reconfd = true;

            if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
                if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                    N_Graphics::lcd->noDisplay();
                    N_Graphics::lcd->noBacklight();
                    xSemaphoreGive(N_Common::xI2C);
                }

                xSemaphoreGive(N_Common::xLCDAccess);
            }
            Serial.flush();
            esp_deep_sleep_start();
        }
    }

    void wifiCoIssues(S_WifiCoArgs &co_args) {
        // 60s, check wifi state
        checkWifiState(co_args);

        if (co_args.flicker_led) {
            // apply flickering here :: turn on and off at a speed=100ms
            if ((xTaskGetTickCount() - co_args.xLastLedOn) >= T_100_MS) {
                co_args.xLastLedOn = xTaskGetTickCount();

                co_args.last_flick_state =
                    (((static_cast<int>(co_args.last_flick_state) == HIGH)
                          ? LOW
                          : HIGH) != 0);

                digitalWrite(LED_COLD,
                             static_cast<uint8_t>(co_args.last_flick_state));
                digitalWrite(LED_HOT,
                             static_cast<uint8_t>(co_args.last_flick_state));
            }
        }

        // release led lock. Locked for 5000 each time
        if ((xTaskGetTickCount() - co_args.xtick_unknown) >=
                T_LED_LOCK_RELEASE_MS &&
            co_args.flicker_led) {
            // after 5000 ms, clear the busy state of the leds
            digitalWrite(LED_COLD, LOW);
            digitalWrite(LED_HOT, LOW);

            co_args.xtick_unknown = xTaskGetTickCount();
            xEventGroupClearBits(N_Common::handler_event, ((1U << LED_BUSY)));

            co_args.flicker_led = false; // end flick

            if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0) {
                N_Helpers::idAndMsg(
                    {.id   = "vTaskSetup",
                     .word = "released led, stoping flickering"});
            }
        }
    }

    void idleSTAMode(S_IdleArgs &idle_args) {
        if (idle_args.clock_data.conf_active) {
            // config is active
            idle_args.hard_resume =
                true; // give it a chance to comeback again and go to idle
                      // mode and then unclock the whole sys again
            // since the SYS_STATE_IDLE is active, offline and online shutdown
            // themselves lol waiting for the savor to occurs lol.
            // xSemaphoreGive(N_Common::xLCDAccess); // give it to config thing,
            // and it will block you lol
            return;
        }

        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdFAIL) {
            return;
        }

        // if the thing is not active, then go ahead, otherwise, leave what you
        // are doing and give the mutex. also turn off the lcd disp
        if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
            vTaskDelay(pdMS_TO_TICKS(T_1000_MS));
            N_Graphics::lcd->noDisplay();
            N_Graphics::lcd->noBacklight();
            xSemaphoreGive(N_Common::xI2C);
        }

        if (idle_args.hard_resume) {
            // come here thanks to hard_resume, so don't inc the counter, do
            // nothing
            idle_args.hard_resume = false;
        } else {
            // only idle if state not active, if hard resume, no idle
            // only inc if you did not enter through hard_resume
            idle_args.rep += 1; // and yea, only if you did yo thing nicely you
                                // can count it, not while config was waiting
            idle_args.check_go_idle_now = true;
        }
        xSemaphoreGive(N_Common::xLCDAccess);
    }

    void taskNotifMan(S_TaskNotifArgs &notif_args, bool &idle) {
        if ((notif_args.rv & (1U << SYSNOTIF_NOW)) != 0) {
            notif_args.rv_msk |= (1U << SYSNOTIF_NOW);
        } else if ((notif_args.rv & (1U << SYSNOTIF_FCAST)) != 0) {
            notif_args.rv_msk |= (1U << SYSNOTIF_FCAST);
        } else if ((notif_args.rv & (1U << SYSNOTIF_IN)) != 0) {
            notif_args.rv_msk |= (1U << SYSNOTIF_IN);
        }

        if (notif_args.rv_msk == ((1U << SYSNOTIF_IN) | (1U << SYSNOTIF_FCAST) |
                                  (1U << SYSNOTIF_NOW))) {
            idle = true;                 // all cases met
            notif_args.rv_msk &=
                ~((1U << SYSNOTIF_IN) | (1U << SYSNOTIF_FCAST) |
                  (1U << SYSNOTIF_NOW)); // clear the state flag
        } else {
            idle = false;
        }
    }

    void launchMenuTimout(S_ResolveInMenunArgs &menu_args,
                          const String &pcTaskName) {
        if (menu_args.clock_data.tmr_flag or menu_args.clock_data.ghost_timer) {
            return;
        }

        S_IdCreateTimer timer_id{};
        timer_id.pcTaskName = pcTaskName;
        timer_id.timerName  = "one-x-timer";

        // enter this to create the timer only once, not multiple times #
        // countdown timer
        if (static_cast<int>(createTimer(
                static_cast<int64_t>(menu_args.conf_args.config_T) * T_1000_MS,
                releaserDaemon, timer_id)) == pdPASS) {
            // prevent from launching multiple timers by settin it to 1, so next
            // time the condition check will fail
            xEventGroupSetBits(N_Common::handler_event, (1U << TIMER_FLAG_BIT));
            menu_args.conf_args.config_msk      = menu_args.conf_args.config_T;
            menu_args.conf_args.xLastWakeTime_c = xTaskGetTickCount();

            if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0) {
                N_Helpers::idAndMsg(
                    {.id   = "vSetup",
                     .word = "timeout for " +
                             static_cast<String>(menu_args.conf_args.config_T) +
                             " started"});
            }
        }
    }

    void broadcastConfMode(bool clear) {
        if (!clear) {
            // request all tasks to stop in order to enter the config mode
            xEventGroupSetBits(
                N_Common::handler_event,
                ((1U << CONF_ACTIVE_BIT_ON) | (1U << CONF_ACTIVE_BIT_LED) |
                 (1U << CONF_ACTIVE_BIT_SCR) | (1U << CONF_ACTIVE_BIT_DAE) |
                 (1U << CONF_ACTIVE_BIT_OFF)));
        } else {
            // remove the config mode
            xEventGroupClearBits(
                N_Common::handler_event,
                ((1U << CONF_ACTIVE_BIT_ON) | (1U << CONF_ACTIVE_BIT_LED) |
                 (1U << CONF_ACTIVE_BIT_SCR) | (1U << CONF_ACTIVE_BIT_DAE) |
                 (1U << CONF_ACTIVE_BIT_OFF)));
        }
    }

    void clockOrStaMode() {
        bool bl_on{false};
        // dynamically change the exec mode : station vs clock (from web server)
        switch (N_Init::CLOCK_PENDING) {
            case CLK_OR_STA_CLOCK_MODE: {
                if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
                    N_Helpers::idAndMsg(
                        {.id = "vSetup", .word = "got clock mode"});
                }
                // clock mode
                // cannot just suspend them, lol let them release the mutex
                // before proceding
                broadcastConfMode(false);

                if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) ==
                    pdPASS) {
                    N_Graphics::clearLcd();
                    vTaskSuspend(N_Sys::xTaskOffline);
                    vTaskSuspend(N_Sys::xTaskOnline);
                    vTaskSuspend(N_Sys::xTaskDaemon);

                    broadcastConfMode(true); // clear the config mode
                    xEventGroupSetBits(
                        N_Common::handler_event,
                        ((1U << V_CLK_STATE_BIT_SET) |
                         (1U
                          << V_CLK_STATE_BIT_LED))); // validate clock mode now

                    xSemaphoreGive(N_Common::xLCDAccess);
                }

                // to prevent this from running all the way everytime, putting
                // -1 won't allow the if execution again
                //  user will need to revalidate the state
                N_Init::CLOCK_PENDING = -1;
            } break;

            case CLK_OR_STA_STA_MODE: {
                if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
                    N_Helpers::idAndMsg(
                        {.id = "vSetup", .word = "got weather mode"});
                }
                // weather mode, resume the others
                xEventGroupClearBits(N_Common::handler_event,
                                     ((1U << V_CLK_STATE_BIT_SET) |
                                      (1U << V_CLK_STATE_BIT_LED)));
                /* TaskStatus_t xTaskDetails;

                vTaskGetInfo(N_Sys::xTaskDaemon, &xTaskDetails, pdFALSE,
                eInvalid); if (xTaskDetails.eCurrentState == eSuspended)
                {
                        vTaskResume(N_Sys::xTaskDaemon);
                } */

                // make sure lcd is on
                bl_on = true;

                vTaskResume(N_Sys::xTaskDaemon);
                vTaskResume(N_Sys::xTaskOnline);
                vTaskResume(N_Sys::xTaskOffline);

                N_Init::CLOCK_PENDING =
                    -2; // -2, neither sta nor clock so don't care + didn't ask
            } break;

            case CLK_OR_STA_CLOCK_STA_NONE: {
                bl_on = true;
                if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
                    N_Helpers::idAndMsg(
                        {.id = "vSetup", .word = "WM portal mode"});
                }
                xEventGroupSetBits(N_Common::handler_event,
                                   (1U << V_START_PORTAL_BIT_SET));
                N_Init::CLOCK_PENDING = -2;
            } break;

            default: break;
        }

        if (!bl_on) {
            return;
        }
        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->backlight();

                xSemaphoreGive(N_Common::xI2C);
            }
            xSemaphoreGive(N_Common::xLCDAccess);
        }
    }

    auto createTimer(const int64_t duration,
                     TimerCallbackFunction_t(call_func_ptr),
                     const S_IdCreateTimer &timer_id) -> bool {
        // create the timer for the timeout
        N_Common::one_x_timer_releaser =
            xTimerCreate(timer_id.timerName.c_str(), pdMS_TO_TICKS(duration),
                         pdFALSE, static_cast<void *>(nullptr), call_func_ptr);
        if (N_Common::one_x_timer_releaser != nullptr) {
            xTimerStart(N_Common::one_x_timer_releaser, portMAX_DELAY);
            return true;
        }

        if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
            N_Helpers::idAndMsg(
                {.id = timer_id.pcTaskName, .word = "Error creating timer"});
        }

        return false;
    }

    void resolveInputs(S_Menu &menu_n, S_BtnState &btn_state,
                       S_ResolveInMenunArgs &inputs_args,
                       const char *pcTaskName) {

        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdFAIL) {
            return;
        }

        // mutex acquired
        vTaskSuspend(N_Sys::xTaskOffline);
        vTaskSuspend(
            N_Sys::xTaskOnline); // suspend those who have access to the lcd
                                 // mainly (the led with the clock is already
                                 // cleared, we don't want to stop this task)
        vTaskSuspend(
            N_Sys::xTaskDaemon); // prevent from going to sleep mode here

        // make sure the backlit is on
        N_Graphics::lcd->backlight();

        inputs_args.clock_data.ghost_timer =
            false; // did not started a new timer

        handleMenuLogic(menu_n, inputs_args);
        btn_state.flag.at(inputs_args.menu_args.idx) = false;

        // display the menu here
        N_Graphics::dispMenu(menu_n, inputs_args.clock_data);

        // release the mutex for next task which will wake up
        broadcastConfMode(true);
        xSemaphoreGive(N_Common::xLCDAccess);

        // create the timer for the timeout
        EventBits_t event_result{xEventGroupWaitBits(
            N_Common::handler_event, ((1U << TIMER_FLAG_BIT)), pdFALSE, pdFALSE,
            0)}; /*event await timer_flag*/
        ((event_result & (1U << TIMER_FLAG_BIT)) != 0U)
            ? inputs_args.clock_data.tmr_flag = true
            : inputs_args.clock_data.tmr_flag = false;

        launchMenuTimout(inputs_args, static_cast<String>(pcTaskName));
    }

    void rebootEsp(int secon_) {
        TickType_t ticktoco{xTaskGetTickCount()};
        bool state_led_wl{true};

        while (xTaskGetTickCount() - ticktoco < T_1000_MS) {
            // flashes for x (above) seconds
            // state_led_wl ^= true;
            state_led_wl = !state_led_wl;
            digitalWrite(LED_COLD, static_cast<uint8_t>(state_led_wl));
            digitalWrite(LED_HOT, static_cast<uint8_t>(state_led_wl));
            vTaskDelay(pdMS_TO_TICKS(T_100_MS));
        }

        esp_sleep_enable_timer_wakeup(static_cast<int64_t>(secon_) *
                                      US_2_S); // xs, to reco
        esp_deep_sleep_start();
    }

    void handleMenuLogic(S_Menu &menu_n,
                         S_ResolveInMenunArgs &menu_logic_args) {
        if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0) {
            N_Helpers::idAndMsg({.id = "vSetup", .word = "called handle menu"});
        }

        switch (menu_logic_args.menu_args.idx) {
            case MENU_BACKWARD: { // dec menu (x3 click)
                if (menu_n.current_menu == 0) {
                    menu_n.current_menu =
                        MENUS -
                        1; // allow to cycle the menu e.g. current menu: 5, 4, 3
                } else {
                    menu_n.current_menu--; // current 5? 4, 3, 2
                }
            } break;

            case MENU_FORWARD: { // inc menu (2x click)
                if (menu_n.current_menu == MENU_INNER_6 - 1) {
                    menu_n.current_menu =
                        0; // allow to cycle the menu e.g. current menu: 5, 1, 2
                } else {
                    menu_n.current_menu++;
                }
            } break;

            case MENU_CYCLE_INNER: { // exec aka cycle the current menu page
                                     // variable (1x
                                     // click)
                // actually grab what is displayed and yeah, save it
                if (menu_logic_args.menu_args.data_nexter > MENU_INNER_6 - 1) {
                    // if higher than the allowed elements 0 1 2 3 4 5
                    menu_logic_args.menu_args.data_nexter = 0;
                }
                // pick the value for this exact index and menu page using
                // preset available, and inc the idx
                menu_logic_args.menu_args.current_param =
                    N_Sys::config_set.at(menu_n.current_menu)
                        .at(menu_logic_args.menu_args.data_nexter++);
                // apply the option right away
                applyMenuSave(menu_n, menu_logic_args);
            } break;

            case MENU_VALIDATE_AND_EXIT: { // exit menu before timeout (long
                                           // press)
                // checks the different state, validate the bit, remove the
                // current timer, and create a new one really fast (like 2ms)
                // that will expire right away
                exitBeforeTimeout(menu_logic_args);
            } break;

            default: {
            } break;
        }
    }

    void applyMenuSave(S_Menu &menu_n, S_ResolveInMenunArgs &apply_args) {
        switch (menu_n.current_menu) {
            case MENU_1: { // lcd scroll speed
                if (apply_args.menu_args.current_param != 0) {
                    N_Graphics::LCD_SPD =
                        apply_args.menu_args
                            .current_param; // global include param
                }
            } break;

            case MENU_2: { // sleep param
                if (apply_args.menu_args.current_param != 0) {
                    N_Sys::IDLE_TMS = apply_args.menu_args.current_param;
                    xQueueOverwrite(
                        N_Common::xIdle_T,
                        static_cast<void *>(
                            &N_Sys::IDLE_TMS)); // local so gotta send
                                                // it to the queue
                }
            } break;

            case MENU_3: { // deep sleep duration
                if (apply_args.menu_args.current_param != 0) {
                    N_Sys::SLEEP_DURATION_M =
                        apply_args.menu_args.current_param;
                }
            } break;

            case MENU_4: { // AM / PM mode
                apply_args.clock_data.clk_24_mode =
                    apply_args.menu_args.current_param != 0;
                xQueueOverwrite(
                    N_Common::x24h_Mode,
                    static_cast<void *>(&apply_args.clock_data.clk_24_mode));
            } break;

            case MENU_5: { // WLAN portal ondemand
                apply_args.clock_data.chg_wlan =
                    apply_args.menu_args.current_param != 0;
                xQueueOverwrite(
                    N_Common::xPortalMode,
                    static_cast<void *>(&apply_args.clock_data.chg_wlan));
            } break;

            case MENU_0: { // clock mode
                apply_args.clock_data.clk_mode =
                    apply_args.menu_args.current_param != 0;
                xQueueOverwrite(
                    N_Common::xClockMode,
                    static_cast<void *>(&apply_args.clock_data.clk_mode));
            } break;

            default: {
            } break;
        }
    }

    void exitBeforeTimeout(S_ResolveInMenunArgs &exit_args) {
        EventBits_t event_result{};
        S_IdCreateTimer timer_id{};

        /*event await timer_flag and valid_clk*/
        if (exit_args.clock_data.clk_mode) {
            xEventGroupSetBits(
                N_Common::handler_event,
                ((1U << V_CLK_STATE_BIT_SET) |
                 (1U << V_CLK_STATE_BIT_LED))); // validate the mode now
            xEventGroupClearBits(N_Common::handler_event,
                                 ((1U << WAIT_CLK_BIT_LED)));
        }
        event_result =
            xEventGroupWaitBits(N_Common::handler_event, (1U << TIMER_FLAG_BIT),
                                pdFALSE, pdFALSE, 0);
        ((event_result & (1U << TIMER_FLAG_BIT)) != 0U)
            ? exit_args.clock_data.tmr_flag = true
            : exit_args.clock_data.tmr_flag = false;

        timer_id.pcTaskName = "vSetup";
        timer_id.timerName  = "one-x-timer";

        if (exit_args.clock_data.tmr_flag) {
            // countdown timer were already running: stops it and delete it
            xTimerStop(N_Common::one_x_timer_releaser, portMAX_DELAY);
            xTimerDelete(N_Common::one_x_timer_releaser, portMAX_DELAY);

            // recreate a timer and launch it with a small delay time (2ms)

            bool res_timer{
                createTimer(pdMS_TO_TICKS(2), releaserDaemon, timer_id)};

            if (res_timer) {
                exit_args.clock_data.ghost_timer = true;
            }
        }
    }

} // namespace N_Sys
