#ifndef SENSORS_HPP
#define SENSORS_HPP

#include "btn.hpp"
#include "commons.hpp"
#include "defines.hpp"
#include "graphics.hpp"
#include "influx_db.hpp"
#include "weather_api.hpp"

namespace N_Sensors {
    extern std::unique_ptr<Adafruit_BME280> bme;
    extern std::unique_ptr<SensirionI2cScd4x> scd40;

    extern TaskHandle_t xSensor;

    // //////////////////////////////////////////////////////////////////////
    void vTaskSensors(void * /*pvParameters*/);

    void vSensorsPushData(S_DataOffline &data, const char *pcTaskName);

    auto rainingStrMatch(const String &rain) -> bool;
    void initSensors();
    auto scdSetup() -> bool;
    auto bmeSetup(S_DataOffline &data) -> bool;
    void setIOConfig();
    void scdCalDaemon(TimerHandle_t xTimer);
    void callbackTouchWakeup();
    void driveRainLed();
    void readBtnState(N_Btn::Btn &myBtn, S_BtnState &S_BtnState);

    void pirLogic(bool &pir_state, bool &last_pir, S_BlMgmt &that_mgmt,
                  TickType_t &last_detected_pir, bool valid_clk);

    void vSensorsInfluxRunner(S_InfluxTaskRunnerArgs &influx_runner_args,
                              const char *pcTaskName);

} // namespace N_Sensors

#endif // SENSORS_HPP
