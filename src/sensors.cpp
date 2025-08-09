#include "sensors.hpp"

namespace N_Sensors {
    std::unique_ptr<Adafruit_BME280> bme;
    std::unique_ptr<SensirionI2cScd4x> scd40;

    TaskHandle_t xSensor = nullptr;

    void initSensors() {
        try {
            bme   = std::make_unique<Adafruit_BME280>();
            scd40 = std::make_unique<SensirionI2cScd4x>();
        } catch (const std::exception &e) {
            assert("abort");
        }
    }

    void vTaskSensors(void * /*pvParameters*/) {
        const char *pcTaskName = "vTaskSensors";

        S_DataOffline data;
        S_InfluxTaskRunnerArgs influx_runner_args{};

        N_InfluxDb::getServerState(influx_runner_args.is_server_up,
                                   pcTaskName); // try connecting to the server

        // await scd calibration, do not clear on exit
        xEventGroupWaitBits(N_Common::handler_event, (1U << SCD_CAL_DONE),
                            pdFALSE, pdFALSE, portMAX_DELAY);
        if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
            N_Helpers::idAndMsg(
                {.id = pcTaskName, .word = "SCD41 calibration terminated"});
        }

        N_InfluxDb::getServerState(influx_runner_args.is_server_up,
                                   pcTaskName); // try connecting to the server
        for (;;) {
            vSensorsPushData(data, pcTaskName);

            // Regularly check if the influx db server is up? default: T = 5
            // mins
            if (!influx_runner_args.is_server_up and
                (xTaskGetTickCount() - influx_runner_args.ticktime >
                 T_CHECK_SERVER_STATUS_MS)) {
                influx_runner_args.ticktime = xTaskGetTickCount();
                N_InfluxDb::getServerState(
                    influx_runner_args.is_server_up,
                    pcTaskName); // try connecting to the server
            }

            //  And start sending the sensor data to the server if it's up
            vSensorsInfluxRunner(influx_runner_args, pcTaskName);

            vTaskDelay(pdMS_TO_TICKS(T_SENSORS_MS));
        }
    }

    void vSensorsPushData(S_DataOffline &data, const char *pcTaskName) {
        bool data_ready{false};
        uint16_t error{};

        std::array<char, SCD_ERR_MSG_LEN> errorMessage{};

        error = N_Sensors::scd40->getDataReadyStatus(data_ready);
        if (data_ready and
            xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
            uint16_t co2_val{};
            float temp_val{};
            float humi_val{};
            error =
                N_Sensors::scd40->readMeasurement(co2_val, temp_val, humi_val);

            data.co2   = co2_val;
            data.temp2 = temp_val;
            data.humi  = humi_val;

            // humi of bme
            data.temp     = N_Sensors::bme->readTemperature() - TEMP_OFFSET;
            data.humi_bme = N_Sensors::bme->readHumidity();
            data.pressure = N_Sensors::bme->readPressure() / 100.0F;
            data.altitude = N_Sensors::bme->readAltitude(
                N_WeatherAPI::SEALEVELPRESSURE_HPA_2);

            xSemaphoreGive(N_Common::xI2C);
        }

        if (error == 0 && data.co2 != 0) {
            // send it to the queue
            xQueueOverwrite(N_Common::xQueueOffline, &data);
        } else if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
            errorToString(error, errorMessage.data(), SCD_ERR_MSG_LEN);
            N_Helpers::idAndMsg(
                {.id   = pcTaskName,
                 .word = "SCD bad measurement: " +
                         static_cast<String>(errorMessage.data())});
        }
    }

    void pirLogic(bool &pir_state, bool &last_pir, S_BlMgmt &that_mgmt,
                  TickType_t &last_detected_pir, const bool valid_clk) {
        if (!valid_clk) {
            return;
        }

        // pir needs about 5s to re-arm the system, how to counter this?
        ((digitalRead(PIR_BL)) != 0U) ? pir_state = HIGH : pir_state = LOW;
        if ((pir_state != last_pir) && pir_state &&
            (xSemaphoreTake(N_Common::xLCDAccess, T_WAIT_PIR_I2C_LOCK_MS) ==
             pdPASS)) {
            // this helps counter the fact of writing true true or false
            // false repeatedly also, only write true, the false will be
            // taken care
            if (xSemaphoreTake(N_Common::xI2C, T_WAIT_PIR_I2C_LOCK_MS) ==
                pdPASS) {
                N_Graphics::lcd->backlight();
                xSemaphoreGive(N_Common::xI2C);

                that_mgmt.turned_on  = true;
                that_mgmt.extended_T = 0;
            }
            xSemaphoreGive(N_Common::xLCDAccess);

            // keep it lit until the end of (detection + 5s) time
        }
        last_pir = pir_state;

        if (last_pir) {
            last_detected_pir =
                xTaskGetTickCount(); // record last time it stopped
            return;
        }

        // aka stopped detecting, it need 5s to restart reading new state
        // turn on the lit and keep it for 5s
        if (((xTaskGetTickCount() - last_detected_pir) >
             T_PIR_COUNTER_DELAY_MS) and
            xSemaphoreTake(N_Common::xLCDAccess, T_WAIT_PIR_I2C_LOCK_MS) ==
                pdPASS) // added 1 s to it
        {
            if (that_mgmt.turned_on and
                xSemaphoreTake(N_Common::xI2C, T_WAIT_PIR_I2C_LOCK_MS) ==
                    pdPASS) {
                if (!that_mgmt.extended_bl or
                    that_mgmt.extended_T > pdMS_TO_TICKS(that_mgmt.up_to_T)) {
                    N_Graphics::lcd->noBacklight();

                    that_mgmt.extended_T = 0;
                    that_mgmt.turned_on  = false;
                }
                that_mgmt.extended_T++;
                xSemaphoreGive(N_Common::xI2C);
            }
            xSemaphoreGive(N_Common::xLCDAccess);
        }
    }

    auto bmeSetup(S_DataOffline &data) -> bool {
        // BME CONFIG weather mode
        if (!N_Sensors::bme->begin(BME_I2C_ADDR, &Wire)) {
            if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
                const char *pcTaskName = "bmeSetup";
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Error reading from bme sensor, rebooting"});
            }
            return true;
        }

        // humi of bme
        data.temp = N_Sensors::bme->readTemperature() - TEMP_OFFSET;
        data.humi = N_Sensors::bme->readHumidity();
        data.pressure =
            N_Sensors::bme->readPressure() / PRESSURE_CALC_100_FLOAT;
        data.altitude =
            N_Sensors::bme->readAltitude(N_WeatherAPI::SEALEVELPRESSURE_HPA_2);

        // co2
        data.co2 = INVALID_CO2_VAL;

        return false;
    }

    auto scdSetup() -> bool {
        uint16_t error{};
        std::array<char, SCD_ERR_MSG_LEN> errorMessage{};

        Wire.begin();
        N_Sensors::scd40->begin(Wire, SCD41_I2C_ADDR_62);
        const char *pcTaskName = "scdSetup";

        // wake up sensor
        error = N_Sensors::scd40->wakeUp();
        if (error != 0) {
            if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
                errorToString(error, errorMessage.data(),
                              sizeof errorMessage.data());
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Can't wakeup(), dead? lol: " +
                             static_cast<String>(errorMessage.data())});
            }

            return true;
        }

        // stop potentially previously started measurement
        error = N_Sensors::scd40->stopPeriodicMeasurement();
        if (error != 0) {
            if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
                errorToString(error, errorMessage.data(),
                              sizeof errorMessage.data());
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Can't stopPeriodicMeasurement(): " +
                             static_cast<String>(errorMessage.data())});
            }

            return true;
        }

        // re-init
        error = N_Sensors::scd40->reinit();
        if (error != 0) {
            if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
                errorToString(error, errorMessage.data(),
                              sizeof errorMessage.data());
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Can't reinit(): " +
                             static_cast<String>(errorMessage.data())});
            }

            return true;
        }

        // Start Measurement
        error = N_Sensors::scd40->startPeriodicMeasurement();
        if (error != 0) {
            if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
                errorToString(error, errorMessage.data(),
                              sizeof errorMessage.data());
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Unable to startPeriodicMeasurement(): " +
                             static_cast<String>(errorMessage.data())});
            }

            return true;
        }

        // will call scdCalDaemon() when done
        N_Common::scd_cal_one_x_timer =
            xTimerCreate("scd_calibration_timer", pdMS_TO_TICKS(T_SCD_CAL_MS),
                         pdFALSE, static_cast<void *>(nullptr), scdCalDaemon);
        xEventGroupClearBits(
            N_Common::handler_event,
            (1U << SCD_CAL_DONE)); // set the calibration bit to 0

        // start the timer now
        if (N_Common::scd_cal_one_x_timer == nullptr) {
            if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Error creating timer, rebooting"});
            }
            return true;
        }

        xTimerStart(N_Common::scd_cal_one_x_timer, portMAX_DELAY);
        if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
            N_Helpers::idAndMsg({.id = pcTaskName, .word = "Timer spawned"});
        }

        return false;
    }

    void setIOConfig() {
        pinMode(LED_RAIN_NOW, OUTPUT);
        pinMode(LED_HOT, OUTPUT);
        pinMode(LED_COLD, OUTPUT);
        pinMode(LED_OPERATING, OUTPUT);

        pinMode(BUZZ, OUTPUT);
        pinMode(PIR_BL, INPUT);

        digitalWrite(LED_OPERATING, LOW);
        digitalWrite(LED_COLD, LOW);
        digitalWrite(LED_HOT, LOW);
    }

    auto rainingStrMatch(const String &rain_desc) -> bool {
        const bool raining_now = std::any_of(
            RAINING_MATCH_STRINGS.begin(), RAINING_MATCH_STRINGS.end(),
            [&rain_desc](const auto &this_raining_symbol) {
                return rain_desc.indexOf(this_raining_symbol.data(), 0) != -1;
            });
        return raining_now;
    }

    void driveRainLed() {
        String rain{};
        if (xQueuePeek(N_Common::xQueueRain, static_cast<void *>(&rain), 0) ==
            pdFAIL) {
            return;
        }

        const bool raining_now = rainingStrMatch(rain);
        digitalWrite(LED_RAIN_NOW, static_cast<uint8_t>(raining_now));
    }

    void scdCalDaemon(TimerHandle_t xTimer) {
        // called when timer expires
        xEventGroupSetBits(
            N_Common::handler_event,
            (1U << SCD_CAL_DONE)); // set the calibration bit to 1
        if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
            N_Helpers::idAndMsg(
                {.id = "timer cal scd", .word = "timer expired, bit set"});
        }
    }

    void readBtnState(N_Btn::Btn &myBtn, S_BtnState &S_BtnState) {
        myBtn.tick(); // btn state cleared each time

        switch ((myBtn.getBtnState())) {
            case N_Btn::Btn::BTN_CLICK: {
                if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
                    N_Helpers::idAndMsg(
                        {.id = "vSetup", .word = "that's a click"});
                }
                myBtn.clearBtnState();
                S_BtnState.flag.at(0) = true;
            } break;

            case N_Btn::Btn::BTN_X2_CLICK: {
                myBtn.clearBtnState();
                S_BtnState.flag.at(1) = true;
            } break;

            case N_Btn::Btn::BTN_X3_CLICK: {
                myBtn.clearBtnState();
                S_BtnState.flag.at(2) = true;
                break;
            }

            case N_Btn::Btn::BTN_PRESS: {
                if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
                    N_Helpers::idAndMsg(
                        {.id = "vSetup", .word = "that's a long press"});
                }
                myBtn.clearBtnState();
                S_BtnState.flag.at(3) = true;
            } break;

            default: {
            } break;
        }
    }

    void vSensorsInfluxRunner(S_InfluxTaskRunnerArgs &influx_runner_args,
                              const char *pcTaskName) {
        int app_cpu = xPortGetCoreID();

        if (influx_runner_args.is_server_up) {
            // wake up sender task
            if (!influx_runner_args.writer_task_running) {
                // create writer task (idle)
                xTaskCreatePinnedToCore(N_InfluxDb::vTaskInfluxSender,
                                        "vTaskInfluxSender", TASK_STAKE_4K,
                                        nullptr, tskIDLE_PRIORITY,
                                        &N_InfluxDb::xTaskInfluxDb, app_cpu);
                if (N_InfluxDb::xTaskInfluxDb != nullptr) {
                    vTaskSuspend(N_InfluxDb::xTaskInfluxDb);
                    vTaskPrioritySet(N_InfluxDb::xTaskInfluxDb, P_SENSORS);

                    if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
                        N_Helpers::idAndMsg(
                            {.id   = pcTaskName,
                             .word = "resuming the child task"});
                    }
                    vTaskResume(N_InfluxDb::xTaskInfluxDb);
                    influx_runner_args.writer_task_running = true;
                }
            } else {
                // it was running, just kill it
                if (N_InfluxDb::xTaskInfluxDb != nullptr) {
                    if ((N_Common::debug_msk & (1U << DEBUG_SENSORS)) != 0U) {
                        N_Helpers::idAndMsg(
                            {.id   = pcTaskName,
                             .word = "suspending and deleting the child "
                                     "task\n"});
                    }
                    vTaskSuspend(N_InfluxDb::xTaskInfluxDb);
                    vTaskDelete(N_InfluxDb::xTaskInfluxDb);
                    influx_runner_args.writer_task_running = false;

                    int send_failed{};
                    xQueuePeek(N_Common::xInfluxFailedAtt,
                               static_cast<void *>(&send_failed), 0);
                    influx_runner_args.failed_try += send_failed;
                }
            }

            // if the number of successive fails are high, signal that
            // the server is gone offline
            if (influx_runner_args.failed_try >=
                INFLUX_DB_INFLUX_DB_FAILED_ATT_THRES) {
                influx_runner_args.ticktime =
                    xTaskGetTickCount(); // wait another
                                         // T_CHECK_SERVER_STATUS_5_MIN_MS
                                         // minutes before checking
                                         // again
                influx_runner_args.is_server_up = false;
                // the T is 5s, with 6 failed attemps, we have 30s of
                // unresponsivness
            }
        }

        if (!influx_runner_args.is_server_up and
            (N_Common::debug_msk & (1U << DEBUG_DB_SERVER)) != 0U and
            (influx_runner_args.failed_try >=
             INFLUX_DB_INFLUX_DB_FAILED_ATT_THRES)) {
            N_Helpers::idAndMsg(
                {.id   = pcTaskName,
                 .word = "Sever not responding. Idling db function"});
        }
        xQueueOverwrite(N_Common::xInfluxDB_state,
                        &influx_runner_args.is_server_up);
    }

    void callbackTouchWakeup() {}
} // namespace N_Sensors
