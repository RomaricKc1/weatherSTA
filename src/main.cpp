#include "others.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// OBJECTS DEFINITIONs
AsyncWebServer server(server_port);
LiquidCrystal_I2C lcd(lcd_addr, 16, 2); // LCD
Adafruit_BME280 bme;                    // temp+humi+pressure I2C sensor
SensirionI2cScd4x scd40;                // co2+temp+humi I2C sensor

WiFiManager wm; // wifi manager
ESP32Time rtc;
// (INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET,
// 						 INFLUXDB_TOKEN /*,
// InfluxDbCloud2CACert */);

long user_gmt_offset(0), user_daylight_offset(0);
String user_timezone("Europe/Paris"), user_ntp_server("pool.ntp.org"),
    user_ntp_server2("europe.pool.ntp.org");
String user_influx_token(""), user_influx_org(""),
    user_influx_url(INFLUXDB_URL), user_influx_bucket(INFLUXDB_BUCKET);
/////////////////////////////////////////////////////////////////////////////////////

// TASKS
void vTaskAPI(void *pvParameters) {
    /**
     * Now: current time weather data
     * Fcast for forecast
     * api should be get from openweathermap
     * and location addr from the gps terrain stuff
     */
    const char *pcTaskName = "vAPI";

    dataOnline_t data_n{}, data_f{};
    data_n.apiKey = user_api_key;
    data_n.nameOfCity = user_location_coordinate;

    String serverPath_now = "http://api.openweathermap.org/data/2.5/weather?" +
                            data_n.nameOfCity + "&APPID=" + data_n.apiKey +
                            "&mode=json&units=metric";
    String serverPath_forecast =
        "http://api.openweathermap.org/data/2.5/forecast?" + data_n.nameOfCity +
        "&APPID=" + data_n.apiKey + "&mode=json&units=metric&cnt=1";

    float last_press = 1013.0;
    uint32_t msk_state = 0x00;
    msk_state |= (1 << NOW);

    for (;;) {
        // await wifi connected, do not clear on exit
        xEventGroupWaitBits(handler_event, (1 << WLAN_CONTD_BIT), pdFALSE,
                            pdFALSE, portMAX_DELAY);

        if (msk_state & (1 << NOW)) {
            msk_state &= ~((1 << NOW)); // set the mask state to disable now and
                                        // activate fcast
            msk_state |= (1 << FCAST);

            nowLogic(pcTaskName, serverPath_now, data_n, last_press);
        } else if (msk_state & (1 << FCAST)) {
            msk_state &= ~((1 << FCAST));
            msk_state |= (1 << NOW);

            fcastLogic(pcTaskName, serverPath_forecast, data_f);
        }

        vTaskDelay(pdMS_TO_TICKS(API_TMS));
    }
}

void vTaskInfluxSender(void *pvParameters) {
    const char *pcTaskName = "vTaskInfluxSender";
    dataOffline_t data;

    // influx db
    Point sensor("Co2_TH"); // data point
    sensor.addTag("device", DEVICE);
    sensor.addTag("SSID", WiFi.SSID());
    sensor.addTag("Where", "Main");

    // request sensors data and write to the server
    int failed_try{};

    xQueuePeek(xQueueOffline, static_cast<void *>(&data), 0);
    writePointToServer(sensor, data, pcTaskName, failed_try);
    xQueueOverwrite(xInfluxFailedAtt, &failed_try);

    taskYIELD();

    for (;;) {
    }
}

void vTaskSensors(void *pvParameters) {
    const char *pcTaskName = "vTaskSensors";
    int app_cpu = xPortGetCoreID();

    char errorMessage[256];
    dataOffline_t data;
    bool data_ready = false, is_server_up = false, writer_task_running{false};

    int failed_try = 0;
    TickType_t ticktime = 0;

    getServer_State(is_server_up, pcTaskName); // try connecting to the server

    // await scd calibration, do not clear on exit
    xEventGroupWaitBits(handler_event, (1 << SCD_CAL_DONE), pdFALSE, pdFALSE,
                        portMAX_DELAY);
    if (debug_msk & (1 << debug_sensors)) {
        id_n_talk(pcTaskName, "SCD41 calibration terminated");
    }

    getServer_State(is_server_up, pcTaskName); // try connecting to the server
    // TODO: handle reading issues
    for (;;) {
        // SCD & BME :: scd will slow bme down
        uint16_t error = scd40.getDataReadyStatus(data_ready);
        if (data_ready == true and
            xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS) {
            error = scd40.readMeasurement(data.co2, data.temp2, data.humi);

            // humi of bme
            data.temp = bme.readTemperature() - temp_offset;
            data.humi_bme = bme.readHumidity();
            data.pressure = bme.readPressure() / 100.0F;
            data.altitude = bme.readAltitude(SEALEVELPRESSURE_HPA_2);

            xSemaphoreGive(xI2C);
        }

        if (!(error == true or data.co2 == 0)) {
            // send it to the queue
            // printf("sent co2: %d - humi: %f - temp: %f\n", data_to_queue.co2,
            // data_to_queue.humi, data_to_queue.temp);
            xQueueOverwrite(xQueueOffline, &data);
        } else if (debug_msk & (1 << debug_sensors)) {
            errorToString(error, errorMessage, 256);
            id_n_talk(pcTaskName,
                      "SCD bad measurement: " + (String)errorMessage);
        }

        // Regularly check if the influx db server is up? default: T = 5 mins
        if (is_server_up == false and
            (xTaskGetTickCount() - ticktime > CHECK_SERVER_STATUS_T)) {
            ticktime = xTaskGetTickCount();
            getServer_State(is_server_up,
                            pcTaskName); // try connecting to the server
        }

        //  And start sending the sensor data to the server if it's up
        if (is_server_up == true) {
            // wake up sender task
            if (!writer_task_running) {
                // create writer task (idle)
                xTaskCreatePinnedToCore(vTaskInfluxSender, "vTaskInfluxSender",
                                        4096, NULL, tskIDLE_PRIORITY,
                                        &xTaskInfluxDb, app_cpu);
                if (xTaskInfluxDb != nullptr) {
                    vTaskSuspend(xTaskInfluxDb);
                    vTaskPrioritySet(xTaskInfluxDb, P_SENSORS);

                    if (debug_msk & (1 << debug_sensors)) {
                        id_n_talk(pcTaskName, "resuming the child task");
                    }
                    vTaskResume(xTaskInfluxDb);
                    writer_task_running = true;
                }
            } else {
                // it was running, just kill it
                if (xTaskInfluxDb != nullptr) {
                    if (debug_msk & (1 << debug_sensors)) {
                        id_n_talk(pcTaskName,
                                  "suspending and deleting the child task\n");
                    }
                    vTaskSuspend(xTaskInfluxDb);
                    vTaskDelete(xTaskInfluxDb);
                    writer_task_running = false;

                    int send_failed{};
                    xQueuePeek(xInfluxFailedAtt,
                               static_cast<void *>(&send_failed), 0);
                    failed_try += send_failed;
                }
            }

            // if the number of successive fails are high, signal that the
            // server is gone offline
            if (failed_try >= FAILED_ATT_THRES) {
                ticktime =
                    xTaskGetTickCount(); // wait another CHECK_SERVER_STATUS_T
                                         // minutes before checking again
                is_server_up = false;
                // the T is 5s, with 6 failed attemps, we have 30s of
                // unresponsivness
            }
        }

        if (is_server_up == false and (debug_msk & (1 << debug_dbserver)) and
            (failed_try >= FAILED_ATT_THRES)) {
            id_n_talk(pcTaskName, "Sever not responding. Idling db function");
        }
        xQueueOverwrite(xInfluxDB_state, &is_server_up);

        vTaskDelay(pdMS_TO_TICKS(SENSORS_MS));
    }
}

void vLEDs(void *pvParameters) {
    /**
     * Event info
     * valid_clk: authorization to run the clock mode
     * conf_active: configuration panel activated, need LCD for the menu
     * wait_clk: got signal to start clock mode, now waiting for validation
     */
    const char *pcTaskName = "vLEDs";
    String rain = "";
    dataOffline_t my_data_offline;
    my_data_offline.co2 = 0;

    String raining[] = {"rain", "thunderstorm", "drizzle"};

    time_var_t time_vars{};
    time_vars.ringhh = 1;
    time_vars.ringmm = 1;

    int ticktime_date = 0, ticktime_co2 = 0, ticktime_server = 0;
    float outTemp = 0.0;

    bool last_blank = true, was_co2 = false, db_server_state = false;

    EventBits_t event_result, event_res;
    TickType_t ticktime = xTaskGetTickCount();
    TickType_t xLastRainCheck = ticktime;
    TickType_t ticktime_buz_alarm = ticktime;

    bool clk_24h_mode = false;

    for (;;) {
        bool conf_active = false, wait_clk = false, valid_clk = false,
             busy_led = false;

        // request sensors data
        xQueuePeek(xQueueOffline, static_cast<void *>(&my_data_offline), 0);

        // managing the clock mode
        /*await valid_clk, conf_active, wait_clk event*/
        event_result = xEventGroupWaitBits(handler_event,
                                           ((1 << CONF_ACTIVE_BIT_LED) |
                                            (1 << V_CLK_STATE_BIT_LED) |
                                            (1 << WAIT_CLK_BIT_LED)),
                                           pdFALSE, pdFALSE, 0);

        (event_result & (1 << V_CLK_STATE_BIT_LED)) ? valid_clk = true
                                                    : valid_clk = false;
        (event_result & (1 << CONF_ACTIVE_BIT_LED)) ? conf_active = true
                                                    : conf_active = false;
        (event_result & (1 << WAIT_CLK_BIT_LED)) ? wait_clk = true
                                                 : wait_clk = false;

        if (valid_clk == true and conf_active == false and wait_clk == false) {
            /**
             * Prints hours and minutes, with bliking colon
             * Rings at 12 am / 12 pm
             */
            // Get clock data and ring functionality
            getClockData(time_vars);

            // ask the lcd to print
            // print time here
            if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS) {
                /*await clear_one event*/
                bool clear_once{};
                event_result = xEventGroupWaitBits(
                    handler_event, ((1 << CLEAR_ONC_LED)), pdFALSE, pdFALSE, 0);
                (event_result & (1 << CLEAR_ONC_LED)) ? clear_once = true
                                                      : clear_once = false;

                /*peek on the queue*/
                xQueueReceive(x24h_Mode, static_cast<void *>(&clk_24h_mode), 0);
                printTimeLCD(time_vars, clear_once, clk_24h_mode, last_blank);
                xSemaphoreGive(xLCDAccess);
            }

            // date, temp, humi, co2
            if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS) {
                printIndoorLCD(my_data_offline, time_vars, was_co2,
                               ticktime_date, ticktime_co2, ticktime_server,
                               ticktime_buz_alarm, db_server_state, pcTaskName);
                xSemaphoreGive(xLCDAccess);
            }

            // print out temp here
            if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS) {
                printOutdoorLCD(outTemp);
                xSemaphoreGive(xLCDAccess);
            } /* printf("t diff: %d\n", xTaskGetTickCount() - xLastWakeTime); */
        }

        // hot / cold
        event_res = xEventGroupWaitBits(handler_event, (1 << LED_BUSY), pdFALSE,
                                        pdFALSE, 0);
        (event_res & (1 << LED_BUSY)) ? busy_led = true : busy_led = false;

        if (busy_led == false) {
            if (my_data_offline.temp >= 24) {
                digitalWrite(LED_HOT, HIGH);
                digitalWrite(LED_COLD, LOW);
            } else {
                digitalWrite(LED_COLD, HIGH);
                digitalWrite(LED_HOT, LOW);
            }
        }

        // raining or not (check it every TM_CHECK_RAIN unit(s) of duration)
        if ((xTaskGetTickCount() - xLastRainCheck) >= TM_CHECK_RAIN) {
            xLastRainCheck = xTaskGetTickCount();
            driveRainLed(rain, raining, (sizeof(raining) / sizeof(raining[0])));
        }

        if (time_vars.timehh_24_int >= 22 || time_vars.timehh_24_int < 6) {
            // extend backlit when at night time
            int val = 11000;
            xQueueOverwrite(xXtend_BL_T, &val);
        }

        vTaskDelayUntil(&ticktime, LED_MS);
    }
}

void vTaskOffline(void *pvParameters) {
    dataOffline_t my_data;
    bool validated_state = false;
    EventBits_t event_result;

    vTaskDelay(pdMS_TO_TICKS(100));

    for (;;) {
        bool conf_active{};
        bool idle{};
        /**
         * recvd data from queue read offline data
         * rcv and clear the bit CONF_ACTIVE_BIT and SYSSTATE_IDLE_BIT, if idle,
         * suspend yo self, if CONF_ACTIVE_BIT == 1, don't print smth if idle
         * state expire, someone will wake you up lol
         */
        // event listener

        event_result = xEventGroupWaitBits(
            handler_event, ((1 << SYSSTATE_IDLE_BIT_OFF)), pdFALSE, pdFALSE, 0);
        (event_result & (1 << SYSSTATE_IDLE_BIT_OFF)) ? idle = true
                                                      : idle = false;

        if (idle == true) {
            vTaskSuspend(NULL);
        }

        event_result = xEventGroupWaitBits(
            handler_event, ((1 << CONF_ACTIVE_BIT_OFF)), pdFALSE, pdFALSE, 0);
        (event_result & (1 << CONF_ACTIVE_BIT_OFF)) ? conf_active = true
                                                    : conf_active = false;

        offlineTaskPrint(conf_active, my_data, validated_state);

        vTaskDelay(pdMS_TO_TICKS(OFFLINE_MS));
    }
}

void vTaskOnline(void *pvParameters) {
    dataOnline_t my_data;

    static String _id = "n";
    bool read_now = true, validated_state_now = false,
         validated_state_fcast = false;
    EventBits_t event_result;

    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) {
        bool conf_active{};
        bool idle{};
        /**
         * recvd data from queue read offline data
         * rcv and clear the bit CONF_ACTIVE_BIT and SYSSTATE_IDLE_BIT, if idle,
         * suspend yo self, if CONF_ACTIVE_BIT == 1, don't print smth if idle
         * state expire, someone will wake you up lol
         */
        // event listener

        event_result = xEventGroupWaitBits(
            handler_event, ((1 << SYSSTATE_IDLE_BIT_ON)), pdFALSE, pdFALSE, 0);
        (event_result & (1 << SYSSTATE_IDLE_BIT_ON)) ? idle = true
                                                     : idle = false;

        if (idle == true) {
            vTaskSuspend(NULL);
        }

        event_result = xEventGroupWaitBits(
            handler_event, ((1 << CONF_ACTIVE_BIT_ON)), pdFALSE, pdFALSE, 0);
        (event_result & (1 << CONF_ACTIVE_BIT_ON)) ? conf_active = true
                                                   : conf_active = false;

        onlineTaskPrint(conf_active, my_data, validated_state_now,
                        validated_state_fcast, read_now, _id);

        vTaskDelay(pdMS_TO_TICKS(ONLINE_MS));
    }
}

void vDaemon(void *pvParameters) {
    int rep = 0, n = 0;
    bool idle, hard_resume = false;

    bool conf_active, check_go_idle_now = false;
    EventBits_t event_result;
    const char *pcTaskName = "vDaemon";

    /**
     * Notification stuff
     */
    uint32_t rv, rv_msk = 0x00;

    for (;;) {
        // event initializer
        xTaskNotifyWait(0xFF, pdFALSE, &rv, 0);
        taskNotifMan(rv, rv_msk, idle);

        if (rep > 1 and (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)) {
            rep = 0;
            // deep sleep with wake from specific time
            if (debug_msk & (1 << debug_daemon)) {
                id_n_talk(pcTaskName, "Going to sleep for " +
                                          (String)SLEEP_DURATION_M +
                                          "mins. LOL cya..");
            }

            if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS) {
                lcd.display();
                lcd.backlight();

                lcd.setCursor(0, 0);
                lcd.print("                ");
                lcd.setCursor(0, 1);
                lcd.print("                ");

                lcd.setCursor(0, 0);
                lcd.print("going to sleep..");
                lcd.setCursor(6, 1);
                lcd.print("bye!"); // 6	4	 6

                xSemaphoreGive(xI2C);
            }

            vTaskDelay(pdMS_TO_TICKS(3000));

            if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS) {
                lcd.noDisplay();
                lcd.noBacklight();

                xSemaphoreGive(xI2C);
            }

            digitalWrite(BUZZ, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            digitalWrite(BUZZ, 0);
            Serial.flush();

            esp_deep_sleep_start();
            // no further execution until reboot
        }

        if (idle == true or hard_resume == true) {
            // every thing done once, idle on for every mode
            xEventGroupSetBits(handler_event, ((1 << SYSSTATE_IDLE_BIT_OFF) |
                                               (1 << SYSSTATE_IDLE_BIT_ON)));

            event_result =
                xEventGroupWaitBits(handler_event, ((1 << CONF_ACTIVE_BIT_DAE)),
                                    pdFALSE, pdFALSE, 0);
            (event_result & (1 << CONF_ACTIVE_BIT_DAE)) ? conf_active = true
                                                        : conf_active = false;

            idleSTAMode(conf_active, hard_resume, rep, check_go_idle_now);
        }

        if (check_go_idle_now == true) {
            check_go_idle_now = false;
            // go idle mode
            xQueueReceive(xIdle_T, static_cast<void *>(&IDLE_TMS), 0);

            if ((!(rep > 1)) and
                (createTimer(IDLE_TMS, timer_daemon, pcTaskName,
                             "one-x-timer") == pdPASS)) {
                // timer started and announce
                xEventGroupSetBits(
                    handler_event,
                    ((1 << BTN_WAIT_MODE_BIT))); // going idle mode, don't check
                                                 // input state
            }
        }

        n++;
        if (n >= 10) {
            if (getLocalTime(&timeinfo)) {
                strftime(formated_time, 80, "Last known time: %I:%M%p.",
                         &timeinfo);
            }
            n = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(DEM_MS));
    }
}

void vTaskWelcome(void *pvParameters) {
    // shows welcome message while esp is connecting
    if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS) {
        String msg = "    Weather Station/Clock v2.1";
        scroll_text(msg, LCD_SPD, 0);

        clear_lcd();
        if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS) {
            lcd.setCursor(0, 0);
            lcd.print("SSID:");
            lcd.setCursor(6, 0);
            lcd.print(AP_NAME);

            lcd.setCursor(0, 1);
            lcd.print("PSWD:");
            lcd.setCursor(6, 1);
            lcd.print(AP_PASWD);

            xSemaphoreGive(xI2C);
        }
        xSemaphoreGive(xLCDAccess);
    }

    // kill himself
    vTaskSuspend(NULL);
    vTaskDelete(NULL);
}

void vTaskWManager(void *pvParameters) {
    WiFiManagerParameter custom_field_apikey, custom_field_location,
        custom_field_ntp_s, custom_field_ntp_s2;
    WiFiManagerParameter custom_field_tz, custom_field_gmt_off,
        custom_field_dyl_off, custom_field_influx_url;
    WiFiManagerParameter custom_field_influx_token, custom_field_influx_org,
        custom_field_influx_bucket;

    const char *pcTaskName = "vTaskWManager";
    int customFieldLength = 40;

    // SPIFFS
    if (!SPIFFS.begin()) {
        id_n_talk(pcTaskName, "SPIFFS Error, rebooting");
        reboot_it(5);
    }

    // WIFI
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    Serial.setDebugOutput(false);

    new (&custom_field_apikey)
        WiFiManagerParameter("apikey", "Your API Key", "", customFieldLength,
                             "placeholder=\"Enter your API key\"");
    new (&custom_field_location)
        WiFiManagerParameter("location", "Your location", "", customFieldLength,
                             "placeholder=\"eg. lat=xx.xxxx&lon=x.xxxx\"");
    new (&custom_field_tz)
        WiFiManagerParameter("timezone", "Your TimeZone", "", customFieldLength,
                             "placeholder=\"eg. Europe/Paris\"");
    new (&custom_field_gmt_off)
        WiFiManagerParameter("gmt_offset", "Your GMT offset", "",
                             customFieldLength, "placeholder=\"eg. 3600\"");
    new (&custom_field_dyl_off)
        WiFiManagerParameter("dyl_offset", "Your daylight saving offset", "",
                             customFieldLength, "placeholder=\"eg. 3600\"");
    new (&custom_field_ntp_s)
        WiFiManagerParameter("ntp_s", "NTP server", "", customFieldLength,
                             "placeholder=\"eg. pool.ntp.org\"");
    new (&custom_field_ntp_s2)
        WiFiManagerParameter("ntp_s2", "NTP server 2", "", customFieldLength,
                             "placeholder=\"eg. pool.ntp.org\"");
    new (&custom_field_influx_org)
        WiFiManagerParameter("influx_org", "Influx DB org", "",
                             customFieldLength, "placeholder=\"\"");
    new (&custom_field_influx_token) WiFiManagerParameter(
        "influx_token", "Influx token", "", 101, "placeholder=\"\"");
    new (&custom_field_influx_url) WiFiManagerParameter(
        "influx_url", "Influx DB url", "", customFieldLength,
        "placeholder=\"http://192.168.1.17:8086\"");
    new (&custom_field_influx_bucket)
        WiFiManagerParameter("influx_bucket", "Influx DB bucket", "",
                             customFieldLength, "placeholder=\"\"");

    wm.addParameter(&custom_field_apikey);
    wm.addParameter(&custom_field_location);
    wm.addParameter(&custom_field_tz);
    wm.addParameter(&custom_field_gmt_off);
    wm.addParameter(&custom_field_dyl_off);
    wm.addParameter(&custom_field_ntp_s);
    wm.addParameter(&custom_field_ntp_s2);
    wm.addParameter(&custom_field_influx_org);
    wm.addParameter(&custom_field_influx_token);
    wm.addParameter(&custom_field_influx_url);
    wm.addParameter(&custom_field_influx_bucket);

    wm.setSaveParamsCallback(saveParamCallback);

    std::vector<const char *> menu = {"wifi", "info",    "param",
                                      "sep",  "restart", "exit"};
    wm.setMenu(menu);

    wm.setConfigPortalTimeout(
        PORTAL_TMT_MS); // auto close configportal after n seconds

    // Create AP and wait for user to connect to it's own wifi
    bool res = wm.autoConnect(AP_NAME, AP_PASWD); // password protected ap
    if (!res) {
        if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS) {
            clear_lcd();
            scroll_text("         Con to AP Failed. Restarting now", LCD_SPD,
                        0);
            xSemaphoreGive(xLCDAccess);
        }
        // reconnect
        vTaskDelay(pdMS_TO_TICKS(1000));
        reboot_it(5);
    }

    // make sure welcome message task finish just before this line, otherwise,
    // deadlock lol at this point, ESP is connected to user desired wlan
    if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS) {
        clear_lcd();
        if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS) {
            lcd.setCursor(0, 0);
            lcd.print("WLAN CO       OK");
            lcd.setCursor(0, 1);
            lcd.print("ConfigFile    ");
            xSemaphoreGive(xI2C);
        }

        xSemaphoreGive(xLCDAccess);
    }

    // if the ESP wakes up with wlan already set, it will skip the portal step
    // and jump here
    readSysConfig();

    // printf("wm:: %ld -> %ld\n", user_gmt_offset, user_daylight_offset);

    configTime(user_gmt_offset, user_daylight_offset, user_ntp_server.c_str(),
               user_ntp_server2.c_str());
    setenv("TZ", user_timezone.c_str(), 1); // Set timezone
    tzset();

    if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS) {
        if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS) {
            lcd.setCursor(0, 1);
            lcd.print("                ");
            lcd.setCursor(0, 1);
            lcd.print("Time cfg        ");

            xSemaphoreGive(xI2C);
        }
        xSemaphoreGive(xLCDAccess);
    }

    // get time from the NTP server
    while (!getLocalTime(&timeinfo)) {
        if (debug_msk & (1 << debug_time)) {
            id_n_talk(pcTaskName, "Failed to obtain time, re-trying..");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    rtc.setTimeStruct(timeinfo);

    if (debug_msk & (1 << debug_time)) {
        id_n_talk(pcTaskName, "RTC timer set");
    }

    if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS) {
        if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS) {
            lcd.setCursor(14, 1);
            lcd.print("OK");
            xSemaphoreGive(xI2C);
        }
        xSemaphoreGive(xLCDAccess);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    // everything all done
    xEventGroupSetBits(handler_event, ((1 << WM_DONE)));
    xEventGroupSetBits(handler_event, ((1 << WLAN_CONTD_BIT)));

    // kill himself
    vTaskSuspend(NULL);
    vTaskDelete(NULL);

    // setup will take control back to finish the boot process
    for (;;) {
    }
}

// SETUP
void setup() {
    // debug_msk |= ((1 << debug_time) | (1 << debug_sensors));
    // debug_msk |= (1 << debug_setup);
    const char *pcTaskName = "vSetup";

    // put your setup code here, to run once:
    int app_cpu = xPortGetCoreID();

    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(10));
    Wire.begin();

    // null data to queue imediatly
    dataOffline_t data_offline = {};
    dataOnline_t data_online = {};

    // EVENTS
    defineSynchroStuff();

    // INOUTS
    setIOConfig();
    //(void)scdSetup(); // SCD config
    if (scdSetup()) {
        reboot_it(5);
    }
    // BME setup
    if (bmeSetup(data_offline)) {
        reboot_it(5);
    }
    lcdSetup(); // LCD
    // TODO: check if inited well

    // welcome task : show welcome message while connecting to wifi at the
    // sametime (almost) these tasks have higher priority than setup, so setup
    // will immedately get preempted

    xTaskCreatePinnedToCore(vTaskWelcome, "vTaskWelcome", 2048, nullptr, P_WM,
                            &xWelcome, app_cpu);
    assert(xWelcome != nullptr);
    // Wifi manager task
    xTaskCreatePinnedToCore(vTaskWManager, "vTaskWManager", 8092, nullptr, P_WM,
                            &xWM, app_cpu);
    assert(xWM != nullptr);

    // wait for WIFIManager done flag
    xEventGroupWaitBits(handler_event, (1 << WLAN_CONTD_BIT), pdFALSE, pdFALSE,
                        portMAX_DELAY); // does not clear on exit

    // wireless LAN connected at this point
    // setup mdns
    if (!MDNS.begin("weather_sta")) {
        if (debug_msk & (1 << debug_setup)) {
            id_n_talk(pcTaskName, "Unable to start mDNS");
        }
    }
    /* else
    {
                                    MDNS.addService("http", "tcp", 80);
    } */

    touchAttachInterrupt(T0, callback_touch_wakeup, TOUCH_THRES);
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_DURATION_M * 60 * US_2_S);
    esp_sleep_enable_touchpad_wakeup();

    boot_times += 1;
    // id_n_talk(pcTaskName, "What happened? I were sleeping. Boot times: x" +
    // (String)boot_times);

    xQueueOverwrite(xQueueOffline, &data_offline);
    xQueueOverwrite(xQueueOn_NOW, &data_online);
    xQueueOverwrite(xQueueOn_FCAST, &data_online);

    // SERVER weather values
    on_Data_t on_now = {};
    on_Data_t on_fcast = {};

    // WEB SERVER
    server_init_stuff(on_now, on_fcast, data_offline, data_online,
                      CLOCK_PENDING);

    digitalWrite(BUZZ, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    digitalWrite(BUZZ, 0);

    // CORE 1
    /*Task_Name		Stack_size	 Task_Priority  Task_handler  Cores
     */
    xTaskCreatePinnedToCore(vTaskSensors, "vTaskSensors", 4096, nullptr,
                            P_SENSORS, &xSensor, app_cpu);
    assert(xSensor != nullptr);
    xTaskCreatePinnedToCore(vLEDs, "vLEDs", 4096, nullptr, P_LED, &xLed,
                            app_cpu);
    assert(xLed != nullptr);
    xTaskCreatePinnedToCore(vTaskAPI, "vTaskAPI", 8192, nullptr, P_API, &xAPI,
                            app_cpu);
    assert(xAPI != nullptr);

    if (RebootOnClock == true) {
        // from RTC mem
        // create on and off_line with low priority, in this way, setup cannot
        // be preempted by these 2.
        xTaskCreatePinnedToCore(vTaskOffline, "vTaskOffline", 4096, NULL,
                                tskIDLE_PRIORITY, &xTaskOffline, app_cpu);
        assert(xTaskOffline != nullptr);
        xTaskCreatePinnedToCore(vTaskOnline, "vTaskOnline", 4096, NULL,
                                tskIDLE_PRIORITY, &xTaskOnline, app_cpu);
        assert(xTaskOnline != nullptr);

        // validate the mode now and tell them to idle
        xEventGroupSetBits(handler_event, ((1 << V_CLK_STATE_BIT_SET) |
                                           (1 << V_CLK_STATE_BIT_LED) |
                                           (1 << SYSSTATE_IDLE_BIT_ON)) |
                                              (1 << SYSSTATE_IDLE_BIT_OFF));

        // we suspend them, they should suspend themselves, just in case the
        // event arrives lately
        vTaskSuspend(xTaskOffline);
        vTaskSuspend(xTaskOnline);

        // change back their priorities
        vTaskPrioritySet(xTaskOnline, P_ONLINE);
        vTaskPrioritySet(xTaskOffline, P_OFFLINE);

        vTaskResume(xTaskOffline);
        vTaskResume(xTaskOnline);

        RebootOnClock = false; // clear this, so next launch is normal
    } else {
        // normal exec
        xTaskCreatePinnedToCore(vTaskOffline, "vTaskOffline", 4096, NULL,
                                P_OFFLINE, &xTaskOffline, app_cpu);
        assert(xTaskOffline != nullptr);
        xTaskCreatePinnedToCore(vTaskOnline, "vTaskOnline", 4096, NULL,
                                P_ONLINE, &xTaskOnline, app_cpu);
        assert(xTaskOnline != nullptr);
    }

    xTaskCreatePinnedToCore(vDaemon, "vDaemon", 4096, NULL, P_DAEMON,
                            &xTaskDaemon, app_cpu);
    assert(xTaskDaemon != nullptr);

    // Other stuff
    server.begin();

    // config menu setup
    menu_t menu_n{};

    // init
    menu_n.current_menu = 0;
    int data_nexter = 0, config_msk = 30, config_T = 30,
        cnt = 0; // 1st index of the data
    long current_param = 0;

    TickType_t xLastWakeTime_c = 0, xTick_offline = 0, xTick_online = 0,
               xTick_wlan_state = 0, xtick_unknown = 0, last_detected_pir = 0;
    TickType_t xLastLedOn = 0;
    bool flicker_led = false, last_flick_state = LOW;

    Btn myBtn(EXEC_BTN, PD);
    btn_state_t STATES_T = {};
    EventBits_t event_result;
    bool clk_mode = false, clk_24_mode = false, chg_wlan = false,
         last_pir = true, pir_state = false, reboot_reconfd = false;
    bool ghost_timer = 0;

    bl_mgmt_t bl_mgmt = {};
    bl_mgmt.extended_bl = true;

    dataOnline_t my_data;
    dataOffline_t my_data_off;

    /**
     * after a btn interraction, should show the menu, thus signal all the other
     * tasks that we are in menu config mode
     *
     * Simple click: flip state e.g. on --> off
     * double click: move to next menu screen
     * triple click:  move to prev menu screen
     * press and hold: leave before the timeout
     *
     * CONF_ACTIVE_BIT_ON for online task, CONF_ACTIVE_BIT_LED for led task, ...
     *
     * The menu is 6 pages
     * ranges from 0 to 5
     * 5 last menu, 0 the begining, aka page 1
     * each menu has 6 elements: like on off on off on off for on off mode, so
     * on that page, a click cycle through them
     *
     * data_nexter is the index to cycle throught the page options
     * after timemout, saves everything and exit
     */
    for (;;) {
        bool valid_clk = false, valid_portal = false, tmr_flag = false,
             btn_wait_mode = false;

        // reading button state
        // check if the system is idle or not by awaiting the btn_wait_mode bit
        event_result = xEventGroupWaitBits(
            handler_event, ((1 << BTN_WAIT_MODE_BIT)), pdFALSE, pdFALSE, 0);
        (event_result & (1 << BTN_WAIT_MODE_BIT)) ? btn_wait_mode = true
                                                  : btn_wait_mode = false;

        if (btn_wait_mode == false) {
            // when in idle mode, doesn't care, so only read in not idle mode
            readBtnState(myBtn, STATES_T);
        }

        // resolve inputs
        for (int i = 0; i < INPUTS_STATES; ++i) {
            if (STATES_T.flag[i] == 1) {
                // when the click state is active try to get the mutex quickly.
                broadcastConfMode();

                // TODO: right now I have two cond. WAIT_CLK_BIT_LED and
                // V_CLK_STATE_BIT_SET for the same clock, might want to
                // optimize, V_CLK_STATE_BIT_SET seems to be the top now Clear
                // the clock mode bit to disabe clock mode and get the mutex
                // asap to display the menu
                xEventGroupClearBits(
                    handler_event,
                    ((1 << V_CLK_STATE_BIT_SET) | (1 << V_CLK_STATE_BIT_LED)));
                xEventGroupSetBits(
                    handler_event,
                    ((1 << WAIT_CLK_BIT_LED))); // suspend clock mode

                // at this point, other tasks have stopped, so we an get the lcd
                // mutex action, namely lcd showing thing
                resolveInputs(menu_n, i, data_nexter, current_param,
                              clk_24_mode, chg_wlan, clk_mode, valid_clk,
                              ghost_timer, tmr_flag, STATES_T, config_T,
                              pcTaskName, config_msk, xLastWakeTime_c);
            }
        }

        // update countdown time (config menu)
        /*event await tmr_flag, valid_clk*/
        event_result = xEventGroupWaitBits(
            handler_event, ((1 << TIMER_FLAG_BIT) | (1 << V_CLK_STATE_BIT_SET)),
            pdFALSE, pdFALSE, 0);
        (event_result & (1 << TIMER_FLAG_BIT)) ? tmr_flag = true
                                               : tmr_flag = false;
        (event_result & (1 << V_CLK_STATE_BIT_SET)) ? valid_clk = true
                                                    : valid_clk = false;

        if (tmr_flag == true && valid_clk == false) {
            // namely clk mode has been started
            if (xSemaphoreTake(xLCDAccess, 0) == pdPASS) {
                // show countdown
                showCountDown(config_msk);

                xSemaphoreGive(xLCDAccess);
            }

            if (xTaskGetTickCount() - xLastWakeTime_c > 1000) {
                // if 1s has passed
                xLastWakeTime_c = xTaskGetTickCount();
                config_msk--;
            }
        }

        // PIR LCD Backlit
        /*event await valid_clk*/
        event_result = xEventGroupWaitBits(
            handler_event, ((1 << V_CLK_STATE_BIT_SET)), pdFALSE, pdFALSE, 0);
        (event_result & (1 << V_CLK_STATE_BIT_SET)) ? valid_clk = true
                                                    : valid_clk = false;

        bl_mgmt.up_to_T = 510;
        xQueuePeek(xXtend_BL_T, static_cast<void *>(&bl_mgmt.up_to_T), 0);
        pirLogic(pir_state, last_pir, bl_mgmt, last_detected_pir, valid_clk);

        // update current running state on the server side
        updateServerValues(xTick_offline, my_data_off, data_offline,
                           xTick_online, my_data, on_now, on_fcast);

        // dynamically change the exec mode : station vs clock (from web server)
        // 0 sta, 1 : clk
        clockOrStaMode();

        /*warn on wlan troubles*/
        wifiCoIssues(xTick_wlan_state, valid_clk, reboot_reconfd, xtick_unknown,
                     cnt, xLastLedOn, flicker_led, last_flick_state);

        /*event await valid_portal*/ // check start portal?
        event_result =
            xEventGroupWaitBits(handler_event, ((1 << V_START_PORTAL_BIT_SET)),
                                pdFALSE, pdFALSE, 0);
        (event_result & (1 << V_START_PORTAL_BIT_SET)) ? valid_portal = true
                                                       : valid_portal = false;
        checkCExecPortal(valid_portal);

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// LOL
void loop() {
    // put your main code here, to run repeatedly:
    // no lol
    vTaskDelete(nullptr);
}
