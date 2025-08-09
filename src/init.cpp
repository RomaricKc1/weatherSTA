#include "init.hpp"

namespace N_Init {
#ifdef WAKE_ON_CLOCK_MODE
    int CLOCK_PENDING{1};
#else
    int CLOCK_PENDING{0};
#endif

    // SETUP
    void setupBase() {
        // N_Common::debug_msk |= ((1 << DEBUG_TIME) | (1 << DEBUG_SENSORS));
        // N_Common::debug_msk |= (1 << DEBUG_SETUP);
        const char *pcTaskName = "vSetup";

        Serial.begin(115200);
        vTaskDelay(pdMS_TO_TICKS(T_10_MS));
        Wire.begin();

        // null data to queue imediatly
        S_DataOffline data_offline = {};
        S_DataOnline data_online   = {};

        // EVENTS
        N_Common::defineSynchroElements();

        // INOUTS
        N_Sensors::setIOConfig();
        //(void)scdSetup(); // SCD config
        if (N_Sensors::scdSetup()) {
            N_Sys::rebootEsp(T_5_SEC);
        }
        // BME setup
        if (N_Sensors::bmeSetup(data_offline)) {
            N_Sys::rebootEsp(T_5_SEC);
        }
        N_Graphics::lcdSetup(); // LCD

        vSetupStartInitTasks();

        // wait for WIFIManager done flag
        xEventGroupWaitBits(N_Common::handler_event, (1 << WLAN_CONTD_BIT),
                            pdFALSE, pdFALSE,
                            portMAX_DELAY); // does not clear on exit

        // wireless LAN connected at this point
        // setup mdns
        vSetupStartMdns(pcTaskName);

        touchAttachInterrupt(T0, N_Sensors::callbackTouchWakeup, TOUCH_THRES);

        uint64_t sleep_time_us = N_Sys::SLEEP_DURATION_M * MIN_TO_S * US_2_S;
        esp_sleep_enable_timer_wakeup(sleep_time_us);

        esp_sleep_enable_touchpad_wakeup();

        N_Sys::boot_times += 1;
        // N_Helpers::idAndMsg({.id=pcTaskName, .word="What happened? I were
        // sleeping. Boot times: x"
        // + (String)N_Sys::boot_times});

        xQueueOverwrite(N_Common::xQueueOffline, &data_offline);
        xQueueOverwrite(N_Common::xQueueOn_NOW, &data_online);
        xQueueOverwrite(N_Common::xQueueOn_FCAST, &data_online);

        // SERVER weather values
        S_OnData on_now   = {};
        S_OnData on_fcast = {};

        // WEB SERVER
        N_WebServer::serverInit(on_now, on_fcast, data_offline,
                                N_Init::CLOCK_PENDING);

        digitalWrite(BUZZ, BUZZ_ON);
        vTaskDelay(pdMS_TO_TICKS(T_100_MS));
        digitalWrite(BUZZ, BUZZ_OFF);

        vSetupStartTasks();

        // Other stuff
        N_WebServer::server->begin();

        // ////////////////////////////////////////////////////////////////////////
        // config full setup here
        // ////////////////////////////////////////////////////////////////////////
        N_Btn::Btn myBtn(EXEC_BTN, PD);

        S_BlMgmt bl_mgmt{};
        bl_mgmt.extended_bl = true;

        S_ResolveInMenunArgs in_n_menu{};
        in_n_menu.conf_args.config_msk = T_30_SEC;
        in_n_menu.conf_args.config_T   = T_30_SEC;

        S_WifiCoArgs co_args{};

        // config menu setup
        S_Menu menu_n{};

        // init
        TickType_t xTick_offline{};
        TickType_t xTick_online{};
        TickType_t last_detected_pir{};

        EventBits_t event_result{};

        S_BtnState btn_state{};

        S_CountDownArgs count_args{};

        S_PirSateArgs pir_args{.last_pir = true, .pir_state = false};

        S_DataOnline my_data{};
        S_DataOffline my_data_off{};

        /**
         * after a btn interraction, should show the menu, thus signal all the
         * other tasks that we are in menu config mode
         *
         * Simple click: flip state e.g. on --> off
         * double click: move to next menu screen
         * triple click:  move to prev menu screen
         * press and hold: leave before the timeout
         *
         * CONF_ACTIVE_BIT_ON for online task, CONF_ACTIVE_BIT_LED for led task,
         * ...
         *
         * The menu is 6 pages
         * ranges from 0 to 5
         * 5 last menu, 0 the begining, aka page 1
         * each menu has 6 elements: like on off on off on off for on off mode,
         * so on that page, a click cycle through them
         *
         * data_nexter is the index to cycle throught the page options
         * after timemout, saves everything and exit
         */

        for (;;) {
            // init clock_data here

            bool btn_wait_mode{};
            // reading button state
            // check if the system is idle or not by awaiting the btn_wait_mode
            // bit
            event_result = xEventGroupWaitBits(N_Common::handler_event,
                                               ((1U << BTN_WAIT_MODE_BIT)),
                                               pdFALSE, pdFALSE, 0);
            (event_result & (1U << BTN_WAIT_MODE_BIT)) ? btn_wait_mode = true
                                                       : btn_wait_mode = false;

            if (!btn_wait_mode) {
                // when in idle mode, doesn't care, so only read in not idle
                // mode
                N_Sensors::readBtnState(myBtn, btn_state);
            }

            // resolve inputs
            vSetupResolveInputs(in_n_menu, menu_n, btn_state, pcTaskName);

            // update countdown time (config menu)
            vSetupUpdCntDownMenu(in_n_menu, count_args, event_result);

            // PIR LCD Backlit
            vSetupPirLogicCtrl(in_n_menu, bl_mgmt, event_result, pir_args,
                               last_detected_pir);

            // update current running state on the server side
            N_WebServer::updateServerValues(xTick_offline, my_data_off,
                                            data_offline, xTick_online, my_data,
                                            on_now, on_fcast);

            // dynamically change the exec mode : station vs clock (from web
            // server) 0 sta, 1 : clk
            N_Sys::clockOrStaMode();

            /*warn on wlan troubles*/
            // only needs a const clock_data.valid_clk
            co_args.clock_data.valid_clk = in_n_menu.clock_data.valid_clk;
            N_Sys::wifiCoIssues(co_args);

            /*event await valid_portal*/ // check start portal?

            bool valid_portal{};
            event_result = xEventGroupWaitBits(N_Common::handler_event,
                                               ((1U << V_START_PORTAL_BIT_SET)),
                                               pdFALSE, pdFALSE, 0);
            (event_result & (1U << V_START_PORTAL_BIT_SET))
                ? valid_portal = true
                : valid_portal = false;
            N_Sys::checkCExecPortal(valid_portal);

            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }

    void vSetupStartInitTasks() {
        // welcome task : show welcome message while connecting to wifi at the
        // sametime (almost) these tasks have higher priority than setup, so
        // setup will immedately get preempted

        int app_cpu{xPortGetCoreID()};
        xTaskCreatePinnedToCore(N_Sys::vTaskWelcome, "vTaskWelcome",
                                TASK_STAKE_2K, nullptr, P_WM, &N_Sys::xWelcome,
                                app_cpu);
        assert(N_Sys::xWelcome != nullptr);
        // Wifi manager task
        xTaskCreatePinnedToCore(N_WM::vTaskWManager, "vTaskWManager",
                                TASK_STAKE_8K, nullptr, P_WM, &N_WM::xWM,
                                app_cpu);
        assert(N_WM::xWM != nullptr);
    }

    void vSetupStartTasks() {
        int app_cpu{xPortGetCoreID()};
        // CORE 1
        /*Task_Name		Stack_size	 Task_Priority  Task_handler
         * Cores
         */
        xTaskCreatePinnedToCore(N_Sensors::vTaskSensors, "vTaskSensors",
                                TASK_STAKE_4K, nullptr, P_SENSORS,
                                &N_Sensors::xSensor, app_cpu);
        assert(N_Sensors::xSensor != nullptr);
        xTaskCreatePinnedToCore(N_Graphics::vLEDs, "vLEDs", TASK_STAKE_4K,
                                nullptr, P_LED, &N_Graphics::xLed, app_cpu);
        assert(N_Graphics::xLed != nullptr);
        xTaskCreatePinnedToCore(N_WeatherAPI::vTaskAPI, "vTaskAPI",
                                TASK_STAKE_8K, nullptr, P_API,
                                &N_WeatherAPI::xAPI, app_cpu);
        assert(N_WeatherAPI::xAPI != nullptr);

        vSetupRebootOnClock();

        xTaskCreatePinnedToCore(N_Sys::vDaemon, "vDaemon", TASK_STAKE_4K, NULL,
                                P_DAEMON, &N_Sys::xTaskDaemon, app_cpu);
        assert(N_Sys::xTaskDaemon != nullptr);
    }

    void vSetupRebootOnClock() {
        int app_cpu{xPortGetCoreID()};

        if (N_Sys::RebootOnClock) {
            // from RTC mem
            // create on and off_line with low priority, in this way, setup
            // cannot be preempted by these 2.
            xTaskCreatePinnedToCore(N_Sys::vTaskOffline, "vTaskOffline",
                                    TASK_STAKE_4K, NULL, tskIDLE_PRIORITY,
                                    &N_Sys::xTaskOffline, app_cpu);
            assert(N_Sys::xTaskOffline != nullptr);
            xTaskCreatePinnedToCore(N_Sys::vTaskOnline, "vTaskOnline",
                                    TASK_STAKE_4K, NULL, tskIDLE_PRIORITY,
                                    &N_Sys::xTaskOnline, app_cpu);
            assert(N_Sys::xTaskOnline != nullptr);

            // validate the mode now and tell them to idle
            xEventGroupSetBits(N_Common::handler_event,
                               ((1U << V_CLK_STATE_BIT_SET) |
                                (1U << V_CLK_STATE_BIT_LED) |
                                (1U << SYSSTATE_IDLE_BIT_ON)) |
                                   (1U << SYSSTATE_IDLE_BIT_OFF));

            // we suspend them, they should suspend themselves, just in case the
            // event arrives lately
            vTaskSuspend(N_Sys::xTaskOffline);
            vTaskSuspend(N_Sys::xTaskOnline);

            // change back their priorities
            vTaskPrioritySet(N_Sys::xTaskOnline, P_ONLINE);
            vTaskPrioritySet(N_Sys::xTaskOffline, P_OFFLINE);

            vTaskResume(N_Sys::xTaskOffline);
            vTaskResume(N_Sys::xTaskOnline);

            N_Sys::RebootOnClock =
                false; // clear this, so next launch is normal
        } else {
            // normal exec
            xTaskCreatePinnedToCore(N_Sys::vTaskOffline, "vTaskOffline",
                                    TASK_STAKE_4K, NULL, P_OFFLINE,
                                    &N_Sys::xTaskOffline, app_cpu);
            assert(N_Sys::xTaskOffline != nullptr);
            xTaskCreatePinnedToCore(N_Sys::vTaskOnline, "vTaskOnline",
                                    TASK_STAKE_4K, NULL, P_ONLINE,
                                    &N_Sys::xTaskOnline, app_cpu);
            assert(N_Sys::xTaskOnline != nullptr);
        }
    }

    void vSetupUpdCntDownMenu(S_ResolveInMenunArgs &in_n_menu,
                              S_CountDownArgs &cnt_down_args,
                              EventBits_t &event_result) {
        /*event await tmr_flag, valid_clk*/
        event_result = xEventGroupWaitBits(
            N_Common::handler_event,
            ((1U << TIMER_FLAG_BIT) | (1U << V_CLK_STATE_BIT_SET)), pdFALSE,
            pdFALSE, 0);
        (event_result & (1U << TIMER_FLAG_BIT))
            ? in_n_menu.clock_data.tmr_flag = true
            : in_n_menu.clock_data.tmr_flag = false;
        (event_result & (1U << V_CLK_STATE_BIT_SET))
            ? in_n_menu.clock_data.valid_clk = true
            : in_n_menu.clock_data.valid_clk = false;

        if (in_n_menu.clock_data.tmr_flag and !in_n_menu.clock_data.valid_clk) {
            // namely clk mode has been started
            if (xSemaphoreTake(N_Common::xLCDAccess, 0) == pdPASS) {
                // show countdown
                N_Graphics::showCountDown(in_n_menu.conf_args.config_msk);

                xSemaphoreGive(N_Common::xLCDAccess);
            }

            if (xTaskGetTickCount() - cnt_down_args.xLastWakeTime_c >
                T_1000_MS) {
                // if 1s has passed
                cnt_down_args.xLastWakeTime_c = xTaskGetTickCount();
                in_n_menu.conf_args.config_msk--;
            }
        }
    }

    void vSetupResolveInputs(S_ResolveInMenunArgs &in_n_menu, S_Menu &menu_n,
                             S_BtnState &btn_state, const char *pcTaskName) {
        for (in_n_menu.menu_args.idx = 0;
             in_n_menu.menu_args.idx < INPUTS_STATES;
             ++in_n_menu.menu_args.idx) {
            if (btn_state.flag.at(in_n_menu.menu_args.idx)) {
                // when the click state is active try to get the mutex
                // quickly.
                N_Sys::broadcastConfMode(false);

                // TODO(romaric): right now I have two cond.
                // WAIT_CLK_BIT_LED and V_CLK_STATE_BIT_SET for the same
                // clock, might want to optimize, V_CLK_STATE_BIT_SET seems
                // to be the top now Clear the clock mode bit to disabe
                // clock mode and get the mutex asap to display the menu
                xEventGroupClearBits(N_Common::handler_event,
                                     ((1U << V_CLK_STATE_BIT_SET) |
                                      (1U << V_CLK_STATE_BIT_LED)));
                xEventGroupSetBits(
                    N_Common::handler_event,
                    ((1U << WAIT_CLK_BIT_LED))); // suspend clock mode

                // at this point, other tasks have stopped, so we an get the
                // lcd mutex action, namely lcd showing thing

                N_Sys::resolveInputs(menu_n, btn_state, in_n_menu, pcTaskName);
            }
        }
    }

    void vSetupPirLogicCtrl(S_ResolveInMenunArgs &in_n_menu, S_BlMgmt &bl_mgmt,
                            EventBits_t &event_result, S_PirSateArgs &pir_args,
                            TickType_t &last_detected_pir) {
        /*event await valid_clk*/
        event_result = xEventGroupWaitBits(N_Common::handler_event,
                                           ((1U << V_CLK_STATE_BIT_SET)),
                                           pdFALSE, pdFALSE, 0);
        (event_result & (1U << V_CLK_STATE_BIT_SET))
            ? in_n_menu.clock_data.valid_clk = true
            : in_n_menu.clock_data.valid_clk = false;

        bl_mgmt.up_to_T = T_BL_UP_MS;
        xQueuePeek(N_Common::xXtend_BL_T, static_cast<void *>(&bl_mgmt.up_to_T),
                   0);
        N_Sensors::pirLogic(pir_args.pir_state, pir_args.last_pir, bl_mgmt,
                            last_detected_pir, in_n_menu.clock_data.valid_clk);
    }

    void vSetupStartMdns(const char *pcTaskName) {
        if (!MDNS.begin("weather_sta")) {
            if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
                N_Helpers::idAndMsg(
                    {.id = pcTaskName, .word = "Unable to start mDNS"});
            }
        }
    }

} // namespace N_Init
