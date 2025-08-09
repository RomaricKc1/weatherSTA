#include "web_server.hpp"

namespace N_WebServer {
    std::unique_ptr<AsyncWebServer> server;

    void initServer() {
        try {
            server = std::make_unique<AsyncWebServer>(SERVER_PORT);
        } catch (const std::exception &e) {
            assert("abort");
        }
    }

    template <typename T> auto getValue(const ValueType &value) -> T & {
        return std::get<std::reference_wrapper<T>>(value).get();
    }

    void autoPutValuesMap(const std::map<String, ValueType> &put_values) {
        for (auto &pair : put_values) {
            if (std::holds_alternative<std::reference_wrapper<String>>(
                    pair.second)) {
                String &strRef = getValue<String>(pair.second);
                N_WebServer::server->on(pair.first.c_str(), HTTP_GET,
                                        [&](AsyncWebServerRequest *request) {
                                            request->send(SUCCESSFULL,
                                                          "text/plain", strRef);
                                        });

            } else if (std::holds_alternative<std::reference_wrapper<float>>(
                           pair.second)) {
                float &floatRef = getValue<float>(pair.second);
                N_WebServer::server->on(pair.first.c_str(), HTTP_GET,
                                        [&](AsyncWebServerRequest *request) {
                                            request->send(
                                                SUCCESSFULL, "text/plain",
                                                static_cast<String>(floatRef));
                                        });
            } else if (std::holds_alternative<std::reference_wrapper<int>>(
                           pair.second)) {
                int &intRef = getValue<int>(pair.second);
                N_WebServer::server->on(pair.first.c_str(), HTTP_GET,
                                        [&](AsyncWebServerRequest *request) {
                                            request->send(
                                                SUCCESSFULL, "text/plain",
                                                static_cast<String>(intRef));
                                        });
            } else if (std::holds_alternative<std::reference_wrapper<
                           const std::basic_string_view<char>>>(pair.second)) {
                const std::basic_string_view<char> &strVRef =
                    getValue<const std::basic_string_view<char>>(pair.second);
                N_WebServer::server->on(
                    pair.first.c_str(), HTTP_GET,
                    [&](AsyncWebServerRequest *request) {
                        request->send(SUCCESSFULL, "text/plain",
                                      static_cast<String>(strVRef.data()));
                    });
            } else if (std::holds_alternative<std::reference_wrapper<
                           std::array<char, MAX_CHAR_BUF_TIME>>>(pair.second)) {
                std::array<char, MAX_CHAR_BUF_TIME> &arrRef =
                    getValue<std::array<char, MAX_CHAR_BUF_TIME>>(pair.second);
                N_WebServer::server->on(
                    pair.first.c_str(), HTTP_GET,
                    [&](AsyncWebServerRequest *request) {
                        request->send(SUCCESSFULL, "text/plain",
                                      static_cast<String>(arrRef.data()));
                    });
            }
        }
    }

    void updateServerValues(TickType_t xTick_offline,
                            S_DataOffline &my_data_off,
                            S_DataOffline &data_offline,
                            TickType_t xTick_online, S_DataOnline &my_data,
                            S_OnData &on_now, S_OnData &on_fcast) {

        if ((xTaskGetTickCount() - xTick_offline) > T_10_S_MS) {
            // 10s for local temp
            xQueuePeek(N_Common::xQueueOffline,
                       static_cast<void *>(&my_data_off), 0);
            updateOfflineValues(data_offline, my_data_off);
            xTick_offline = xTaskGetTickCount();
        }

        if ((xTaskGetTickCount() - xTick_online) > T_30_S_MS) {
            // 30 s for online data
            xQueuePeek(N_Common::xQueueOn_NOW, static_cast<void *>(&my_data),
                       0);
            updateOnlineValues(on_now, my_data);

            xQueuePeek(N_Common::xQueueOn_FCAST, static_cast<void *>(&my_data),
                       0);
            updateOnlineValues(on_fcast, my_data);
            xTick_online = xTaskGetTickCount();
        }
    }

    void updateOnlineValues(S_OnData &struct_, const S_DataOnline &data) {
        struct_.temp        = data.temp;
        struct_.feels_like  = data.feels_like;
        struct_.windSpeed   = data.windSpeed;
        struct_.pressure    = data.pressure;
        struct_.humidity    = data.humidity;
        struct_.clouds      = data.clouds;
        struct_.description = data.description;
        struct_.city        = data.city;
    }

    void updateOfflineValues(S_DataOffline &struct_2,
                             const S_DataOffline &off_data) {
        struct_2.temp     = off_data.temp;
        struct_2.humi     = off_data.humi;
        struct_2.humi_bme = off_data.humi_bme;
        struct_2.altitude = off_data.altitude;
        struct_2.pressure = off_data.pressure;
        struct_2.co2      = off_data.co2;
        struct_2.temp2    = off_data.temp2;
    }
    template <typename T>
    auto serverGetValues(const S_ServerValuesArgs &args, T &ret,
                         const String &pctaskname) {
        N_WebServer::server->on(
            args.path.c_str(), HTTP_POST,
            [args, &ret, pctaskname](AsyncWebServerRequest *request) {
                if (request->hasParam(args.param.c_str(), true)) {
                    String msg =
                        request->getParam(args.param.c_str(), true)->value();
                    ret = static_cast<T>(msg.toInt());
                    // N_Helpers::idAndMsg({.id   = pctaskname,
                    //                      .word = "got param " + args.param +
                    //                              " " +
                    //                              static_cast<String>(ret)});
                } else {
                    // N_Helpers::idAndMsg(
                    //     {.id   = pctaskname,
                    //      .word = "doesn't have such param lol " +
                    //      args.param});
                }
                request->send(NO_CONTENT);
            });
    }

    void serverInit(S_OnData &on_now, S_OnData &on_fcast,
                    S_DataOffline &data_offline, int &system_state) {
        String pctaskname{"[vServer]"};

        serverLoadFiles({.path     = "/",
                         .param    = "",
                         .filetype = "text/html",
                         .filename = "/index.htm"});
        serverLoadFiles({.path     = "/script.js",
                         .param    = "",
                         .filetype = "text/javascript",
                         .filename = "/script.js"});
        serverLoadFiles({.path     = "/favicon.ico",
                         .param    = "",
                         .filetype = "image/png",
                         .filename = "/favicon.png"});

        // dyn settings values
        serverGetValues<int32_t>({.path     = "/lcd_scrl_spd",
                                  .param    = "value_lcd_scrl_spd",
                                  .filetype = "",
                                  .filename = ""},
                                 N_Graphics::lcd_speed_refw, pctaskname);

        serverGetValues<int32_t>({.path     = "/running_mode",
                                  .param    = "value_running_mode",
                                  .filetype = "",
                                  .filename = ""},
                                 system_state, pctaskname);

        serverGetValues<int64_t>({.path     = "/d_slp_duration",
                                  .param    = "value_d_slp_duration",
                                  .filetype = "",
                                  .filename = ""},
                                 N_Sys::sleep_duration_m_refw, pctaskname);

        // update server here? yee
        //         INDOORS
        std::map<String, ValueType> put_values{};

        put_values["/intemperature"]  = std::ref(data_offline.temp);
        put_values["/wk_times"]       = std::ref(N_Sys::boot_times);
        put_values["/intemperature"]  = std::ref(data_offline.temp);
        put_values["/inhumidity"]     = std::ref(data_offline.humi_bme);
        put_values["/inco2"]          = std::ref(data_offline.co2);
        put_values["/inpressure"]     = std::ref(data_offline.pressure);
        put_values["/inaltitude"]     = std::ref(data_offline.altitude);
        put_values["/intemperature2"] = std::ref(data_offline.temp2);

        // OUTDOORS
        //  NOW
        put_values["/outtemperature_now"] = std::ref(on_now.temp);
        put_values["/outhumidity_now"]    = std::ref(on_now.humidity);
        put_values["/outpressure_now"]    = std::ref(on_now.pressure);
        put_values["/outclouds_now"]      = std::ref(on_now.clouds);
        put_values["/outfeelslike_now"]   = std::ref(on_now.feels_like);
        put_values["/outdescription_now"] = std::ref(on_now.description);
        put_values["/outwindspeed_now"]   = std::ref(on_now.windSpeed);

        //  FCAST
        put_values["/outtemperature_fcast"] = std::ref(on_fcast.temp);
        put_values["/outhumidity_fcast"]    = std::ref(on_fcast.humidity);
        put_values["/outpressure_fcast"]    = std::ref(on_fcast.pressure);
        put_values["/outclouds_fcast"]      = std::ref(on_fcast.clouds);
        put_values["/outfeelslike_fcast"]   = std::ref(on_fcast.feels_like);
        put_values["/outdescription_fcast"] = std::ref(on_fcast.description);
        put_values["/outwindspeed_fcast"]   = std::ref(on_fcast.windSpeed);

        put_values["/outcity"] = std::ref(on_now.city);

        put_values["/last_ds"]          = std::ref(N_Common::formated_time);
        put_values["/firmware_version"] = std::ref(FIRMWARE_VERSION);

        // commit
        autoPutValuesMap(put_values);
    }

    auto serverLoadFiles(const S_ServerValuesArgs &args) -> void {
        N_WebServer::server->on(args.path.c_str(), HTTP_GET,
                                [args](AsyncWebServerRequest *request) {
                                    request->send(SPIFFS, args.filename,
                                                  args.filetype);
                                });
    }

} // namespace N_WebServer
