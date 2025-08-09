#include "helpers.hpp"

namespace N_Helpers {
    template <typename T>
    auto convertStrToInt(const S_ConvertIntArgs &conv_args) -> T {
        char *endptr;
        errno = 0;

        T res = strtol(conv_args.txt_val.c_str(), &endptr, BASE_10);

        if (errno == ERANGE) {
            if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
                idAndMsg({.id   = conv_args.pcTaskName,
                          .word = "out of range, strtol"});
            }
            return 0;
        }
        if (endptr == conv_args.txt_val.c_str()) {
            if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
                idAndMsg(
                    {.id = conv_args.pcTaskName, .word = "no digit, strtol"});
            }
            return 0;
        }
        return res;
    }

    template auto convertStrToInt<uint64_t>(const S_ConvertIntArgs &conv_args)
        -> uint64_t;
    template auto convertStrToInt<int64_t>(const S_ConvertIntArgs &conv_args)
        -> int64_t;

    void clockDataParse(S_TimeVar &time_vars, const String &pctaskname) {
        size_t result{};
        result = strftime(time_vars.timehh.data(),
                          sizeof(time_vars.timehh.data()), "%I",
                          &N_Common::STimeinfoLocal); // hour 12h
        if (result == 0) {
            if ((N_Common::debug_msk & (1U << DEBUG_SETUP)) != 0U) {
                idAndMsg(
                    {.id = pctaskname, .word = "strftime failed: hour 12"});
            }
        }

        result = strftime(time_vars.timehh_24.data(),
                          sizeof(time_vars.timehh_24.data()), "%H",
                          &N_Common::STimeinfoLocal);
        if (result == 0) {
            if ((N_Common::debug_msk & (1U << DEBUG_DAEMON)) != 0U) {
                idAndMsg(
                    {.id = pctaskname, .word = "strftime failed: hour 24"});
            }
        }

        result = strftime(time_vars.timemm.data(),
                          sizeof(time_vars.timemm.data()), "%M",
                          &N_Common::STimeinfoLocal); // minutes
        if (result == 0) {
            if ((N_Common::debug_msk & (1U << DEBUG_DAEMON)) != 0U) {
                idAndMsg(
                    {.id = pctaskname, .word = "strftime failed: minutes"});
            }
        }

        result = strftime(time_vars.timedd.data(),
                          sizeof(time_vars.timedd.data()), "%d",
                          &N_Common::STimeinfoLocal); // day of the month
        if (result == 0) {
            if ((N_Common::debug_msk & (1U << DEBUG_DAEMON)) != 0U) {
                idAndMsg({.id   = pctaskname,
                          .word = "strftime failed: day of month"});
            }
        }

        result = strftime(time_vars.timemomo.data(), sizeof(time_vars.timemomo),
                          "%B",
                          &N_Common::STimeinfoLocal); // month in text
        if (result == 0) {
            if ((N_Common::debug_msk & (1U << DEBUG_DAEMON)) != 0U) {
                idAndMsg({.id = pctaskname, .word = "strftime failed: month"});
            }
        }
    }

    void getClockData(S_TimeVar &time_vars, const String &pctaskname) {
        if (N_Common::rtc) {

            N_Common::STimeinfoLocal = N_Common::rtc->getTimeStruct();

            clockDataParse(time_vars, pctaskname);

            // convert to integer
            time_vars.timehh_int = convertStrToInt<int>(
                {.pcTaskName = pctaskname, .txt_val = time_vars.timehh.data()});
            time_vars.timehh_24_int =
                convertStrToInt<int>({.pcTaskName = pctaskname,
                                      .txt_val = time_vars.timehh_24.data()});
            time_vars.timemm_int = convertStrToInt<int>(
                {.pcTaskName = pctaskname, .txt_val = time_vars.timemm.data()});
            time_vars.timedd_int = convertStrToInt<int>(
                {.pcTaskName = pctaskname, .txt_val = time_vars.timedd.data()});

            // Mid-night/day 12 am/pm signal
            if (time_vars.timehh_int == HOUR_12) {
                if (time_vars.ringhh == 1) {
                    time_vars.ringhh = 0;
                    digitalWrite(BUZZ, 1);
                    vTaskDelay(pdMS_TO_TICKS(T_400_MS));
                    digitalWrite(BUZZ, 0);
                }
            } else {
                // re-arm for the next 12 (gonna be AM or PM)
                time_vars.ringhh = 1;
            }

            // for every hour, soft ring :: except from 2 to 6
            if (time_vars.timemm_int == TIME_MIN_0 &&
                (time_vars.timehh_24_int < HOUR_2 ||
                 time_vars.timehh_24_int > HOURS_6_AM)) {
                if (time_vars.ringmm == 1) {
                    time_vars.ringmm = 0;
                    digitalWrite(BUZZ, BUZZ_ON);
                    vTaskDelay(pdMS_TO_TICKS(T_100_MS));
                    digitalWrite(BUZZ, BUZZ_OFF);
                    vTaskDelay(pdMS_TO_TICKS(T_100_MS));
                    digitalWrite(BUZZ, BUZZ_ON);
                    vTaskDelay(pdMS_TO_TICKS(T_100_MS));
                    digitalWrite(BUZZ, BUZZ_OFF);
                }
            } else {
                time_vars.ringmm = 1;
            }

            (N_Common::rtc->getAmPm(true) == "pm") ? time_vars.pm = true
                                                   : time_vars.pm = false;
        }
    }

    void idAndMsg(const S_IdMsg &args) {
        if (xSemaphoreTake(N_Common::xSerial, portMAX_DELAY) == pdPASS) {
            String message{"[" + args.id + "]: " + args.word + ".\n"};
            // std::cout << "%s" << message.c_str();
            Serial.printf("%s", message.c_str());
            // printf("%s", message.c_str());
            xSemaphoreGive(N_Common::xSerial);
        }
    }

    auto spiffReadFile(const S_SpiffsOp &args)
        -> std::optional<std::map<String, String>> {

        if (!(SPIFFS.exists(args.filename.c_str()))) {
            // file doesn't exist
            return std::nullopt;
        }
        // open the config file and read it
        auto config = SPIFFS.open(args.filename.c_str(), "r");
        if (!config) {
            // exist, but unable to read, reboot
            return std::nullopt;
        }

        JsonDocument u_data{};
        DeserializationError error{deserializeJson(u_data, config)};
        if (error) {
            N_Helpers::idAndMsg({.id = args.taskname, .word = error.c_str()});
            // unable to deserialize
            return std::nullopt;
        }
        // populate the map
        std::map<String, String> config_data;
        for (auto &key : PORTAL_KEYS) {
            config_data[key.data()] = static_cast<String>(u_data[key.data()]);
        }

        config.close();

        return config_data;
    }

    auto spiffWriteFile(const S_SpiffsOp &args,
                        const std::map<String, String> &config_map) -> bool {
        JsonDocument u_data{};

        // try to read it first
        auto config_map_opt = N_Helpers::spiffReadFile(
            {.filename = static_cast<String>(CONFIG_FILE.data()),
             .taskname = ""});

        if (config_map_opt) {
            auto old_config_map = *config_map_opt;
            // try loading old u_data
            for (auto &key : PORTAL_KEYS) {
                if (config_map.at(key.data()) == "" and
                    doesMapContain(old_config_map, key)) {
                    u_data[key.data()] = old_config_map.at(key.data());
                } else {
                    u_data[key.data()] = config_map.at(key.data());
                }
            }
        } else {
            // just load the u_data
            for (auto &key : PORTAL_KEYS) {
                u_data[key.data()] = config_map.at(key.data());
            }
        }

        // save the new config u_data
        auto config = SPIFFS.open(args.filename.c_str(), "w");
        if (!config) {
            N_Helpers::idAndMsg(
                {.id = args.taskname, .word = "unable to open config file"});
            return false;
        }

        // data saved as json file in the Flash storage
        serializeJson(u_data, config);
        config.close();

        return true;
    }

    auto doesMapContain(std::map<String, String> &this_map,
                        const std::string_view &key) -> bool {
        auto it = this_map.find(key.data());
        return (it != this_map.end());
    }

} // namespace N_Helpers
