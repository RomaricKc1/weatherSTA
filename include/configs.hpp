#ifndef CONFIGS_HPP
#define CONFIGS_HPP

constexpr std::string_view FIRMWARE_VERSION{"25.08.1"};

// influx db
constexpr std::string_view DEVICE{"ESP32"};
constexpr std::string_view INFLUXDB_URL{"http://192.168.1.17:8086"};
constexpr std::string_view INFLUXDB_BUCKET{"esp32"};

// SYS cfg
constexpr std::string_view CONFIG_FILE{"/user_data.json"};
constexpr std::string_view THE_AP_NAME{"STA esp"};
constexpr std::string_view THE_AP_PASWD{"P@55w0rd"};

constexpr std::string_view TIME_ZONE{"Europe/Paris"};
constexpr std::string_view NTP_SERVER_1{"pool.ntp.org"};
constexpr std::string_view NTP_SERVER_2{"europe.pool.ntp.org"};

constexpr int PORTAL_CONFIG_KEYS_CNT{11};
constexpr std::array<std::string_view, PORTAL_CONFIG_KEYS_CNT> PORTAL_KEYS{
    "api_key",      "location",      "gmt_offset", "dyl_offset",
    "timezone",     "ntp_s",         "ntp_s2",     "influx_org",
    "influx_token", "influx_bucket", "influx_url"};

constexpr std::array<std::string_view, 5> RAINING_MATCH_STRINGS = {
    "rain", "thunderstorm", "drizzle", "snow", "mist"};

// SENSORS & IO
constexpr int LCD_I2C_ADDR{0x27};
constexpr int BME_I2C_ADDR{0x76};

constexpr int BUZZ{2};
constexpr int LED_RAIN_NOW{5};
constexpr int PIR_BL{17};
constexpr int LED_HOT{18};
constexpr int LED_COLD{19};
constexpr int LED_OPERATING{23};
constexpr int EXEC_BTN{27};

// TASKS piority & period
constexpr int P_LED{1};
constexpr int P_SENSORS{1};
constexpr int P_WM{2};
constexpr int P_API{1};
constexpr int P_ONLINE{1};
constexpr int P_OFFLINE{1};
constexpr int P_DAEMON{1};

// SYS
constexpr int MAILBOX_QUEUE{1};
constexpr int TOUCH_THRES{40};
constexpr int TEMP_OFFSET{1};
constexpr int TEMP_HOT_COLD_THRES{24};
constexpr int MAGIC_VALUE_BL{11000};
constexpr int HEALTHY_CO2_THRES{1800};
constexpr int INFLUX_DB_INFLUX_DB_FAILED_ATT_THRES{6};
constexpr int TICKS_CNT_TO_MONTH_PRINT{12};
constexpr int TICK_CNT_TO_CO2_PRINT{4};
constexpr int DEAMON_REP_CNT_THRES{10};
constexpr int REGULAR_CUSTOM_FIELD{40};
constexpr int LONG_CUSTOM_FIELD{120};
constexpr int LCD_SPEED_MS{250};
constexpr int COUNT_DOWN_INIT{10};
constexpr int WIFI_CONNECTION_ISSUE_X{10};

// WEB_SERVER
constexpr int SERVER_PORT{80};

// delays
constexpr int T_BTN_DEBOUNCE_MS{50};
constexpr int T_BTN_CLICK_MS{300};
constexpr int T_BTN_PRESS_MS{400};

constexpr int T_WAIT_PIR_I2C_LOCK_MS{200};
constexpr int T_LED_MS{500};
constexpr int T_ONLINE_MS{500};
constexpr int T_OFFLINE_MS{500};
constexpr int T_BL_UP_MS{510};

constexpr int T_DAEMON_MS{1 * 1000};
constexpr int T_SENSORS_MS{5 * 1000};
constexpr int T_LED_LOCK_RELEASE_MS{5 * 1000};
constexpr int T_PIR_COUNTER_DELAY_MS{6 * 1000};
constexpr int T_API_MS{15 * 1000};

constexpr int T_CHECK_RAIN_MS{1 * 60 * 1000};
constexpr int T_SCD_CAL_MS{1 * 60 *
                           1000}; // 1 min for the co2 to reach \tau = 63%
constexpr int T_CHECK_SERVER_STATUS_MS{5 * 60 * 1000};
constexpr int T_CO2_ALARM_REP_MS{10 * 60 * 1000};
constexpr int T_PORTAL_S{360};
constexpr int T_ESP_SLEEP_MIN{15};

// btn
constexpr bool PU{true};
constexpr bool PD{false};
constexpr int MAX_CLICKS_ALLOWED{3};
constexpr int PIN_INVALID{-1};

#endif // CONFIGS_HPP
