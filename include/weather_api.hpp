#ifndef WEATHER_API_HPP
#define WEATHER_API_HPP

#include "commons.hpp"

namespace N_WeatherAPI {
    extern RTC_DATA_ATTR float SEALEVELPRESSURE_HPA_2;
    extern TaskHandle_t xAPI;

    extern std::unique_ptr<String> user_api_key;
    extern std::unique_ptr<String> user_location_coordinate;

    // ////////////////////////////////////////////////////////////////////////
    void vTaskAPI(void * /*pvParameters*/);

    void initApiStr();
    void parseResponeNow(const JsonDocument &doc, S_DataOnline &data_n);
    void parseResponeFcast(const JsonDocument &doc, S_DataOnline &data_f);
    void nowLogic(const char *pcTaskName, const String &serverPath_now,
                  S_DataOnline &data_n, float &last_press);
    void fcastLogic(const char *pcTaskName, const String &serverPath_forecast,
                    S_DataOnline &data_f);
} // namespace N_WeatherAPI

#endif // WEATHER_API_HPP
