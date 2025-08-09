#include "wm.hpp"

namespace N_WM {

    std::unique_ptr<WiFiManager> wm;

    void initWM() {
        try {
            wm = std::make_unique<WiFiManager>();
        } catch (const std::exception &e) {
            assert("abort");
        }
    }

    TaskHandle_t xWM{nullptr};

    const char *AP_NAME  = THE_AP_NAME.data();
    const char *AP_PASWD = THE_AP_PASWD.data();

    int64_t user_gmt_offset{};
    int32_t user_daylight_offset{};

    String user_timezone{TIME_ZONE.data()};
    String user_ntp_server{NTP_SERVER_1.data()};
    String user_ntp_server2{NTP_SERVER_2.data()};

    void vTaskWManager(void * /*pvParameters*/) {

        WiFiManagerParameter custom_field_apikey{};
        WiFiManagerParameter custom_field_location{};
        WiFiManagerParameter custom_field_ntp_s{};
        WiFiManagerParameter custom_field_ntp_s2{};
        WiFiManagerParameter custom_field_tz{};
        WiFiManagerParameter custom_field_gmt_off{};

        WiFiManagerParameter custom_field_dyl_off{};
        WiFiManagerParameter custom_field_influx_url{};
        WiFiManagerParameter custom_field_influx_token{};
        WiFiManagerParameter custom_field_influx_org{};
        WiFiManagerParameter custom_field_influx_bucket{};

        const char *pcTaskName = "vTaskWManager";

        // SPIFFS
        if (!SPIFFS.begin()) {
            N_Helpers::idAndMsg(
                {.id = pcTaskName, .word = "SPIFFS Error, rebooting"});
            N_Sys::rebootEsp(T_5_SEC);
        }

        // WIFI
        WiFiClass::mode(
            WIFI_STA); // explicitly set mode, esp defaults to STA+AP
        Serial.setDebugOutput(false);

        new (&custom_field_apikey) WiFiManagerParameter(
            "api_key", "Your API Key", "", REGULAR_CUSTOM_FIELD,
            "placeholder=\"Enter your API key\"");
        new (&custom_field_location) WiFiManagerParameter(
            "location", "Your location", "lat=xx.xxxx&lon=x.xxxx",
            REGULAR_CUSTOM_FIELD, "placeholder=\"eg. lat=xx.xxxx&lon=x.xxxx\"");
        new (&custom_field_tz) WiFiManagerParameter(
            "timezone", "Your TimeZone", TIME_ZONE.data(), REGULAR_CUSTOM_FIELD,
            "placeholder=\"eg. Europe/Paris\"");
        new (&custom_field_gmt_off) WiFiManagerParameter(
            "gmt_offset", "Your GMT offset", "3600", REGULAR_CUSTOM_FIELD,
            "placeholder=\"eg. 3600\"");
        new (&custom_field_dyl_off) WiFiManagerParameter(
            "dyl_offset", "Your daylight saving offset", "3600",
            REGULAR_CUSTOM_FIELD, "placeholder=\"eg. 3600\"");
        new (&custom_field_ntp_s) WiFiManagerParameter(
            "ntp_s", "NTP server", NTP_SERVER_1.data(), REGULAR_CUSTOM_FIELD,
            "placeholder=\"eg. pool.ntp.org\"");
        new (&custom_field_ntp_s2) WiFiManagerParameter(
            "ntp_s2", "NTP server 2", NTP_SERVER_2.data(), REGULAR_CUSTOM_FIELD,
            "placeholder=\"eg. pool.ntp.org\"");
        new (&custom_field_influx_org)
            WiFiManagerParameter("influx_org", "Influx DB org", "",
                                 REGULAR_CUSTOM_FIELD, "placeholder=\"\"");
        new (&custom_field_influx_url) WiFiManagerParameter(
            "influx_url", "Influx DB url", "http://192.168.1.17:8086",
            REGULAR_CUSTOM_FIELD, "placeholder=\"http://192.168.1.17:8086\"");
        new (&custom_field_influx_bucket)
            WiFiManagerParameter("influx_bucket", "Influx DB bucket", "esp32",
                                 REGULAR_CUSTOM_FIELD, "placeholder=\"\"");
        // long field name
        new (&custom_field_influx_token)
            WiFiManagerParameter("influx_token", "Influx token", "",
                                 LONG_CUSTOM_FIELD, "placeholder=\"\"");

        N_WM::wm->addParameter(&custom_field_apikey);
        N_WM::wm->addParameter(&custom_field_location);
        N_WM::wm->addParameter(&custom_field_tz);
        N_WM::wm->addParameter(&custom_field_gmt_off);
        N_WM::wm->addParameter(&custom_field_dyl_off);
        N_WM::wm->addParameter(&custom_field_ntp_s);
        N_WM::wm->addParameter(&custom_field_ntp_s2);
        N_WM::wm->addParameter(&custom_field_influx_org);
        N_WM::wm->addParameter(&custom_field_influx_token);
        N_WM::wm->addParameter(&custom_field_influx_url);
        N_WM::wm->addParameter(&custom_field_influx_bucket);

        N_WM::wm->setSaveParamsCallback(saveParamCallback);

        std::vector<const char *> menu = {"wifi", "info",    "param",
                                          "sep",  "restart", "exit"};
        N_WM::wm->setMenu(menu);

        N_WM::wm->setConfigPortalTimeout(
            T_PORTAL_S); // auto close configportal after n seconds

        // Create AP and wait for user to connect to it's own wifi
        bool res =
            N_WM::wm->autoConnect(N_WM::AP_NAME,
                                  N_WM::AP_PASWD); // password protected ap

        if (!res) {
            if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
                N_Graphics::clearLcd();
                N_Graphics::scrollText(
                    "         Connection to AP Failed. Restarting now",
                    {.delay = N_Graphics::LCD_SPD, .line = LCD_LINE_0});
                xSemaphoreGive(N_Common::xLCDAccess);
            }
            // reconnect
            vTaskDelay(pdMS_TO_TICKS(T_1000_MS));
            N_Sys::rebootEsp(T_5_SEC);
        }

        // make sure welcome message task finish just before this line,
        // otherwise, deadlock lol at this point, ESP is connected to user
        // desired wlan
        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
            N_Graphics::clearLcd();
            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_0);
                N_Graphics::lcd->print("WLAN CO       OK");
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
                N_Graphics::lcd->print("ConfigFile    ");
                xSemaphoreGive(N_Common::xI2C);
            }

            xSemaphoreGive(N_Common::xLCDAccess);
        }

        // if the ESP wakes up with wlan already set, it will skip the portal
        // step and jump here
        readSysConfig();

        configTime(N_WM::user_gmt_offset, N_WM::user_daylight_offset,
                   N_WM::user_ntp_server.c_str(),
                   N_WM::user_ntp_server2.c_str());

        // TODO(romaric) {thread safety}
        setenv("TZ", N_WM::user_timezone.c_str(), 1); // Set timezone
        tzset();

        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
                N_Graphics::lcd->print("                ");
                N_Graphics::lcd->setCursor(LCD_CURSOR_0, LCD_LINE_1);
                N_Graphics::lcd->print("Time cfg        ");

                xSemaphoreGive(N_Common::xI2C);
            }
            xSemaphoreGive(N_Common::xLCDAccess);
        }

        // get time from the NTP server
        while (!getLocalTime(&N_Common::STimeInfo)) {
            if ((N_Common::debug_msk & (1U << DEBUG_TIME)) != 0) {
                N_Helpers::idAndMsg(
                    {.id   = pcTaskName,
                     .word = "Failed to obtain time, re-trying.."});
            }
            vTaskDelay(pdMS_TO_TICKS(T_500_MS));
        }

        if (N_Common::rtc) {
            N_Common::rtc->setTimeStruct(N_Common::STimeInfo);
        }

        if ((N_Common::debug_msk & (1U << DEBUG_TIME)) != 0) {
            N_Helpers::idAndMsg({.id = pcTaskName, .word = "RTC timer set"});
        }

        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->setCursor(LCD_CURSOR_14, LCD_LINE_1);
                N_Graphics::lcd->print("OK");
                xSemaphoreGive(N_Common::xI2C);
            }
            xSemaphoreGive(N_Common::xLCDAccess);
        }

        vTaskDelay(pdMS_TO_TICKS(T_1000_MS));

        // everything all done
        xEventGroupSetBits(N_Common::handler_event, ((1U << WM_DONE)));
        xEventGroupSetBits(N_Common::handler_event, ((1U << WLAN_CONTD_BIT)));

        // kill himself
        vTaskSuspend(nullptr);
        vTaskDelete(nullptr);

        // setup will take control back to finish the boot process
        for (;;) {}
    }

    void saveParamCallback() {
        // save user data into a json file in flash storage SPIFFS
        std::map<String, String> config_map;
        // load the map here
        for (auto &key : PORTAL_KEYS) {
            config_map[key.data()] = getParam(key.data());
        }

        bool write_done = N_Helpers::spiffWriteFile(
            {.filename = static_cast<String>(CONFIG_FILE.data()),
             .taskname = ""},
            config_map);

        if (!write_done) {
            if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
                N_Graphics::clearLcd();
                N_Graphics::scrollText("                Error opening/writing "
                                       "config file. Rebooting...",
                                       {
                                           .delay = N_Graphics::LCD_SPD,
                                           .line  = LCD_LINE_0,
                                       });

                xSemaphoreGive(N_Common::xLCDAccess);
            }
            vTaskDelay(pdMS_TO_TICKS(2 * T_1000_MS));
            N_Sys::rebootEsp(T_5_SEC);
            return;
        }

        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
            N_Graphics::clearLcd();
            N_Graphics::scrollText("                Config file saved.",
                                   {
                                       .delay = N_Graphics::LCD_SPD,
                                       .line  = LCD_LINE_0,
                                   });

            xSemaphoreGive(N_Common::xLCDAccess);
        }

        vTaskDelay(pdMS_TO_TICKS(2 * T_1000_MS));
        N_Sys::rebootEsp(T_5_SEC);
    }

    auto getParam(String name) -> String {
        // read parameter from server, for customhmtl input
        if (N_WM::wm->server->hasArg(name)) {
            return N_WM::wm->server->arg(name);
        }
        return "";
    }

    void readSysConfig() {
        // now, try to read the confi_file (api_key and location) to use it.
        // If there's no data well, launch the wifi Portal
        String pctaskname   = "[vWM man]";
        auto config_map_opt = N_Helpers::spiffReadFile(
            {.filename = static_cast<String>(CONFIG_FILE.data()),
             .taskname = pctaskname});

        // file doen't exist or can't read it
        if (!config_map_opt) {
            if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
                if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                    N_Graphics::lcd->setCursor(LCD_CURSOR_14, LCD_LINE_1);
                    N_Graphics::lcd->print("NO");
                    xSemaphoreGive(N_Common::xI2C);
                }
                xSemaphoreGive(N_Common::xLCDAccess);
            }

            vTaskDelay(pdMS_TO_TICKS(T_1000_MS));

            // launch the ondemand portal and wait for user to fill the form
            if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
                N_Graphics::clearLcd();
                N_Graphics::scrollText(
                    "        API KEY and Location required. Opening config "
                    "portal...",
                    {.delay = N_Graphics::LCD_SPD, .line = LCD_LINE_0});

                N_Graphics::scrollText(
                    "        will timeout after " +
                        static_cast<String>(T_PORTAL_S) +
                        "seconds, starting now.",
                    {.delay = N_Graphics::LCD_SPD, .line = LCD_LINE_1});
                xSemaphoreGive(N_Common::xLCDAccess);
            }
            N_Graphics::scrollText(
                "        Hit Restart at the end.",
                {.delay = N_Graphics::LCD_SPD, .line = LCD_LINE_1});

            N_WM::wm->setConfigPortalTimeout(T_PORTAL_S);
            bool res{
                N_WM::wm->startConfigPortal(N_WM::AP_NAME, N_WM::AP_PASWD)};

            if (!res) {
                if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) ==
                    pdPASS) {
                    N_Graphics::clearLcd();
                    N_Graphics::scrollText(
                        "        failed to connect or timed out",
                        {.delay = N_Graphics::LCD_SPD, .line = LCD_LINE_0});
                    xSemaphoreGive(N_Common::xLCDAccess);
                }
                // then what next? restart? yes, after the timeout so it's
                // long, but user can hit restart right away
            }

            N_Graphics::clearLcd();
            N_Graphics::scrollText(
                "        Restarting now...",
                {.delay = N_Graphics::LCD_SPD, .line = LCD_LINE_1});
            vTaskDelay(pdMS_TO_TICKS(T_1000_MS));
            N_Sys::rebootEsp(T_5_SEC);
        }

        // load the configuration data
        auto config_map = *config_map_opt;

        N_WeatherAPI::user_api_key =
            std::make_unique<String>(config_map["api_key"]);
        N_WeatherAPI::user_location_coordinate =
            std::make_unique<String>(config_map["location"]);

        N_WM::user_gmt_offset = N_Helpers::convertStrToInt<int64_t>(
            {.pcTaskName = pctaskname, .txt_val = config_map["gmt_offset"]});
        N_WM::user_daylight_offset = N_Helpers::convertStrToInt<int32_t>(
            {.pcTaskName = pctaskname, .txt_val = config_map["dyl_offset"]});

        N_WM::user_timezone    = config_map["timezone"];
        N_WM::user_ntp_server  = config_map["ntp_s"];
        N_WM::user_ntp_server2 = config_map["ntp_s2"];

        N_InfluxDb::user_influx_org =
            std::make_unique<String>(config_map["influx_org"]);
        N_InfluxDb::user_influx_token =
            std::make_unique<String>(config_map["influx_token"]);
        N_InfluxDb::user_influx_url =
            std::make_unique<String>(config_map["influx_url"]);
        N_InfluxDb::user_influx_bucket =
            std::make_unique<String>(config_map["influx_bucket"]);

        if (xSemaphoreTake(N_Common::xLCDAccess, portMAX_DELAY) == pdPASS) {
            if (xSemaphoreTake(N_Common::xI2C, portMAX_DELAY) == pdPASS) {
                N_Graphics::lcd->setCursor(LCD_CURSOR_14, LCD_LINE_1);
                N_Graphics::lcd->print("OK");
                xSemaphoreGive(N_Common::xI2C);
            }
            xSemaphoreGive(N_Common::xLCDAccess);
        }
        vTaskDelay(pdMS_TO_TICKS(T_1000_MS));
    }
} // namespace N_WM
