#ifndef WM_HPP
#define WM_HPP

#include "commons.hpp"
#include "graphics.hpp"
#include "influx_db.hpp"
#include "weather_api.hpp"

namespace N_WM {

    extern TaskHandle_t xWM;

    extern std::unique_ptr<WiFiManager> wm;

    extern const char *AP_NAME;
    extern const char *AP_PASWD;

    extern int64_t user_gmt_offset;
    extern int32_t user_daylight_offset;

    extern String user_timezone;
    extern String user_ntp_server;
    extern String user_ntp_server2;

    // ////////////////////////////////////////////////////////////////////////
    void vTaskWManager(void * /*pvParameters*/);

    void initWM();
    void saveParamCallback();
    void readSysConfig();
    auto getParam(String name) -> String;

} // namespace N_WM

#endif // WM_HPP
