#include "weather_api.hpp"

namespace N_WeatherAPI {
    RTC_DATA_ATTR float SEALEVELPRESSURE_HPA_2 = SEA_LEVEL_PRESSURE_INIT;
    TaskHandle_t xAPI                          = nullptr;

    std::unique_ptr<String> user_api_key;
    std::unique_ptr<String> user_location_coordinate;

    void initApiStr() {
        try {
            user_api_key             = std::make_unique<String>("");
            user_location_coordinate = std::make_unique<String>("");
        } catch (const std::exception &e) {
            assert("abort");
        }
    }

    void vTaskAPI(void * /*pvParameters*/) {
        /**
         * Now: current time weather data
         * Fcast for forecast
         * api should be get from openweathermap
         * and location addr from the gps terrain stuff
         */
        const char *pcTaskName = "vAPI";

        S_DataOnline data_n{};
        S_DataOnline data_f{};
        data_n.apiKey     = *N_WeatherAPI::user_api_key;
        data_n.nameOfCity = *N_WeatherAPI::user_location_coordinate;

        String serverPath_now =
            "http://api.openweathermap.org/data/2.5/weather?" +
            data_n.nameOfCity + "&APPID=" + data_n.apiKey +
            "&mode=json&units=metric";
        String serverPath_forecast{
            "http://api.openweathermap.org/data/2.5/forecast?" +
            data_n.nameOfCity + "&APPID=" + data_n.apiKey +
            "&mode=json&units=metric&cnt=1"};

        float last_press{INIT_PRESSURE_VALUE};
        uint32_t msk_state{};
        msk_state |= (1U << NOW);

        for (;;) {
            // await wifi connected, do not clear on exit
            xEventGroupWaitBits(N_Common::handler_event, (1U << WLAN_CONTD_BIT),
                                pdFALSE, pdFALSE, portMAX_DELAY);

            if ((msk_state & (1U << NOW)) != 0U) {
                msk_state &= ~((1U << NOW)); // set the mask state to disable
                                             // now and activate fcast
                msk_state |= (1U << FCAST);

                nowLogic(pcTaskName, serverPath_now, data_n, last_press);
            } else if ((msk_state & (1U << FCAST)) != 0U) {
                msk_state &= ~((1U << FCAST));
                msk_state |= (1U << NOW);

                fcastLogic(pcTaskName, serverPath_forecast, data_f);
            }

            vTaskDelay(pdMS_TO_TICKS(T_API_MS));
        }
    }

    void parseResponeNow(const JsonDocument &doc, S_DataOnline &data_n) {
        JsonObjectConst weather_0{doc[str_weather][0]};
        JsonObjectConst main{doc[str_main]};

        String weather_0_description = weather_0[str_desc];
        data_n.description           = weather_0_description;

        data_n.temp       = main[str_temp];
        data_n.feels_like = main[str_f_like];
        data_n.tempMin    = main[str_temp_min];
        data_n.tempMax    = main[str_temp_max];
        data_n.windSpeed  = doc[str_wind][str_speed];
        data_n.pressure   = main[str_pressure];
        data_n.humidity   = main[str_humi];
        data_n.clouds     = doc[str_clouds][str_all];

        String name = doc[str_name]; // e.g. "Nantes;
        data_n.city = name;

        data_n.forecast                      = false;
        data_n.relative                      = "NOW";
        N_WeatherAPI::SEALEVELPRESSURE_HPA_2 = data_n.pressure;
    }

    void parseResponeFcast(const JsonDocument &doc, S_DataOnline &data_f) {
        JsonObjectConst list_0{doc[str_list][0]};
        JsonObjectConst list_0_main{list_0[str_main]};
        JsonObjectConst list_0_wind{list_0[str_wind]};
        JsonObjectConst list_0_weather_0{list_0[str_weather][0]};
        JsonObjectConst city{doc[str_city]};

        data_f.temp       = list_0_main[str_temp];
        data_f.feels_like = list_0_main[str_f_like];
        data_f.tempMin    = list_0_main[str_temp_min];
        data_f.tempMax    = list_0_main[str_temp_max];
        data_f.pressure   = list_0_main[str_pressure];
        data_f.humidity   = list_0_main[str_humi];

        String list_0_weather_0_description = list_0_weather_0[str_desc];
        data_f.description                  = list_0_weather_0_description;

        data_f.clouds    = list_0[str_clouds][str_all];
        data_f.windSpeed = list_0_wind[str_speed];

        String list_0_dt_txt = list_0[str_d_txt];
        data_f.date_text     = list_0_dt_txt;

        String name = city[str_name]; // "Nantes"
        data_f.city = name;

        data_f.forecast = true;
        data_f.relative = "FORECAST";
    }

    void fcastLogic(const char *pcTaskName, const String &serverPath_forecast,
                    S_DataOnline &data_f) {
        HTTPClient http{};
        WiFiClient client{};

        http.begin(client, serverPath_forecast.c_str());
        int httpResponseCode = http.GET();

        if (!(httpResponseCode > 0)) {
            if ((N_Common::debug_msk & (1U << DEBUG_API)) != 0U) {
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Forecast Error code: " +
                             static_cast<String>(httpResponseCode)});
            }
            return;
        }

        JsonDocument doc{};
        DeserializationError error{deserializeJson(doc, http.getStream())};
        http.end();

        if (error) {
            if ((N_Common::debug_msk & (1U << DEBUG_API)) != 0U) {
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Forecast Error code:deserializeJson() failed: " +
                             static_cast<String>(error.c_str())});
            }
            return;
        }

        parseResponeFcast(doc, data_f);

        xQueueOverwrite(N_Common::xQueueOn_FCAST, &data_f);

        if ((N_Common::debug_msk & (1U << DEBUG_API)) != 0U) {
            N_Helpers::idAndMsg(
                {.id   = pcTaskName,
                 .word = "Sent fcast, f like: " +
                         static_cast<String>(data_f.feels_like)});
        }
    }

    void nowLogic(const char *pcTaskName, const String &serverPath_now,
                  S_DataOnline &data_n, float &last_press) {
        HTTPClient http{};
        WiFiClient client{};

        http.begin(client, serverPath_now.c_str());
        int httpResponseCode = http.GET();

        if (!(httpResponseCode > 0)) {
            if ((N_Common::debug_msk & (1U << DEBUG_API)) != 0U) {
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Now Error code: " +
                             static_cast<String>(httpResponseCode)});
            }
            return;
        }

        JsonDocument doc{};
        DeserializationError error{deserializeJson(doc, http.getStream())};
        http.end();

        if (error) {
            if ((N_Common::debug_msk & (1U << DEBUG_API)) != 0U) {
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Now Error code:deserializeJson() failed: " +
                             static_cast<String>(error.c_str())});
            }
            return;
        }

        parseResponeNow(doc, data_n);

        (N_WeatherAPI::SEALEVELPRESSURE_HPA_2 == 0)
            ? N_WeatherAPI::SEALEVELPRESSURE_HPA_2 = last_press
            : last_press = N_WeatherAPI::SEALEVELPRESSURE_HPA_2;

        xQueueOverwrite(N_Common::xQueueOn_NOW, &data_n);

        String rain_desc_queue{data_n.description};
        float temp_queue{data_n.temp};
        xQueueOverwrite(N_Common::xQueueRain, &rain_desc_queue);
        xQueueOverwrite(N_Common::xTempOut, &temp_queue);

        if ((N_Common::debug_msk & (1U << DEBUG_API)) != 0U) {
            N_Helpers::idAndMsg(
                {.id   = pcTaskName,
                 .word = "Sent now, f like: " +
                         static_cast<String>(data_n.feels_like)});
        }
    }

} // namespace N_WeatherAPI
