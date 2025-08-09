#ifndef COMMONS_HPP
#define COMMONS_HPP

// ESP
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <Arduino.h>
#include <ESP32Time.h>
#include <SPIFFS.h>

// WIFI and CO
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiMulti.h>

// any
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>

// SENSORS
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>
#include <map>
#include <variant>

#include "configs.hpp"
#include "defines.hpp"

// STRUCTURES
// ////////////////////////////////////////////////////////////////////////////
// actual base structs

struct S_DataOnline {
    boolean forecast{};

    float tempMin{};
    float tempMax{};
    float temp{};
    float feels_like{};
    float windSpeed{};
    float pressure{};
    float humidity{};
    float clouds{};

    String apiKey{};
    String nameOfCity{};
    String city{};
    String description{};
    String date_text{};
    String relative{};
};

struct S_OnData {
    float temp{};
    float feels_like{};
    float windSpeed{};
    float pressure{};
    float humidity{};
    float clouds{};

    String description{};
    String city{};
};

constexpr int SClockData_PACK_SIZE{8};
struct S_ClockData {
    bool clk_24_mode{};
    bool chg_wlan{};
    bool ghost_timer{};
    bool tmr_flag{};
    bool valid_clk{};
    bool clk_mode{};
    bool conf_active{};
} __attribute__((packed)) __attribute__((aligned(SClockData_PACK_SIZE)));

constexpr int SMenuArgs_PACK_SIZE{16};
struct S_MenuArgs {
    int current_param{};
    int data_nexter{};
    size_t idx{};
} __attribute__((packed)) __attribute__((aligned(SMenuArgs_PACK_SIZE)));

constexpr int SConfArgs_PACK_SIZE{16};
struct S_ConfArgs {
    uint32_t config_msk{};
    uint32_t config_T{};
    uint32_t xLastWakeTime_c{};
} __attribute__((packed)) __attribute__((aligned(SConfArgs_PACK_SIZE)));

constexpr int DATA_OFFLINE_PACK_SIZE{32};
struct S_DataOffline {
    float temp{};
    float temp2{};
    float humi{};
    float pressure{};
    float altitude{};
    float humi_bme{};
    int co2{};
};
// } __attribute__((packed)) __attribute__((aligned(DATA_OFFLINE_PACK_SIZE)));

constexpr int STATE_PACK_SIZE{4};
struct S_State {
    char out_n;
    char out_f;
    char in;
} __attribute__((packed)) __attribute__((aligned(STATE_PACK_SIZE)));

constexpr int Blmgmt_PACK_SIZE{16};
struct S_BlMgmt {
    bool extended_bl{};
    bool turned_on{};
    uint32_t up_to_T{};
    uint32_t extended_T{};
} __attribute__((packed)) __attribute__((aligned(Blmgmt_PACK_SIZE)));

constexpr int SOnPrintArgs_PACK_SIZE{4};
struct S_OnPrintArgs {
    bool validated_state_now{};
    bool validated_state_fcast{};
    bool read_now{};
    String id{};
};

constexpr int SPrintInArgs_PACK_SIZE{32};
struct S_PrintInArgs {
    bool db_server_state{};
    bool was_co2{};
    uint32_t ticktime_date{};
    uint32_t ticktime_co2{};
    uint32_t ticktime_server{};
    uint32_t ticktime_buz_alarm{};
} __attribute__((packed)) __attribute__((aligned(SPrintInArgs_PACK_SIZE)));

constexpr int SScrollTxtArgs_PACK_SIZE{8};
struct S_ScrollTxtArgs {
    int delay{};
    int line{};
} __attribute__((packed)) __attribute__((aligned(SScrollTxtArgs_PACK_SIZE)));

constexpr int STaskNotifArgs_PACK_SIZE{8};
struct S_TaskNotifArgs {
    uint32_t rv{};
    uint32_t rv_msk{};
} __attribute__((packed)) __attribute__((aligned(STaskNotifArgs_PACK_SIZE)));

constexpr int SIdCreateTimer_PACK_SIZE{16};
struct S_IdCreateTimer {
    String pcTaskName{};
    String timerName{};
};
// } __attribute__((packed)) __attribute__((aligned(SIdCreateTimer_PACK_SIZE)));

constexpr int SConvertIntArgs_PACK_SIZE{16};
struct S_ConvertIntArgs {
    String pcTaskName{};
    String txt_val{};
};

struct S_SpiffsOp {
    String filename{};
    String taskname{};
};

struct S_VledHelperArgs {
    float outTemp{};
    bool last_blank{};
    bool wait_clk{};
};

struct S_InfluxTaskRunnerArgs {
    bool is_server_up{};
    bool writer_task_running{};
    int failed_try{};
    TickType_t ticktime{};
};

struct S_ServerValuesArgs {
    String path{};
    String param{};
    String filetype{};
    String filename{};
};

struct S_TickArgs {
    uint64_t waitTime{};
    uint64_t now{};
};

struct S_CountDownArgs {
    TickType_t xLastWakeTime_c{};
};

struct S_PirSateArgs {
    bool last_pir{};
    bool pir_state{};
};

// } __attribute__((packed))
// __attribute__((aligned(SConvertIntArgs_PACK_SIZE)));
enum E_Debug : uint8_t {
    DEBUG_SENSORS,
    DEBUG_WM,
    DEBUG_SETUP,
    DEBUG_API,
    DEBUG_DB_SERVER,
    DEBUG_TIME,
    DEBUG_DAEMON,
};

// other constructs

constexpr int SPrintTimeArgs_PACK_SIZE{2};
struct S_PrintTimeArgs {
    bool clear_once{};
    bool clk_24h_mode{};
} __attribute__((packed)) __attribute__((aligned(SPrintTimeArgs_PACK_SIZE)));

constexpr int SIdleArgs_PACK_SIZE{16};
struct S_IdleArgs {
    bool hard_resume{};
    bool check_go_idle_now{};
    uint32_t rep{};
    S_ClockData clock_data{};
};
// } __attribute__((packed)) __attribute__((aligned(SIdleArgs_PACK_SIZE)));

constexpr int SOffPrintArgs_PACK_SIZE{4};
struct S_OffPrintArgs {
    S_ClockData clock_data{};
};

constexpr int S_WifiCoArgs_PACK_SIZE{32};
struct S_WifiCoArgs {
    bool reboot_reconfd{};
    bool flicker_led{};
    bool last_flick_state{};
    int cnt{};
    TickType_t xTick_wlan_state{};
    TickType_t xtick_unknown{};
    TickType_t xLastLedOn{};
    S_ClockData clock_data{};
};
// } __attribute__((packed)) __attribute__((aligned(SWifiCOArgs_PACK_SIZE)));

constexpr int SResolveInArgs_PACK_SIZE{64};
struct S_ResolveInMenunArgs {
    S_ConfArgs conf_args{};
    S_MenuArgs menu_args{};
    S_ClockData clock_data{};
} __attribute__((packed)) __attribute__((aligned(SResolveInArgs_PACK_SIZE)));

struct S_Menu {
    int current_menu{};
    std::array<std::array<String, MENU_INNER_2>, MENUS> menu = {
        {{"CLOCK mode", ""},
         {"LCD Speed", "ms"},
         {"Sleep T", "ms"},
         {"Deep-sleep T", "mins"},
         {"Am/PM", ""},
         {"Chg WLAN", ""}}};
};

constexpr int SBtnState_PACK_SIZE{4};
struct S_BtnState {
    std::array<bool, INPUTS_STATES> flag{};
} __attribute__((packed)) __attribute__((aligned(SBtnState_PACK_SIZE)));

constexpr int LAST_READING_PACK_SIZE{16};
struct S_LastReading {
    float temp{};
    float humi{};
    float pressure{};
    float alti{};
} __attribute__((packed)) __attribute__((aligned(LAST_READING_PACK_SIZE)));

struct S_IdMsg {
    String id{};
    String word{};
};

constexpr int TIME_CHARS_15{15};
constexpr int TIME_CHARS_3{3};

constexpr int STimeVar_PACK_SIZE{64};
struct S_TimeVar {
    bool pm{};

    uint32_t timemm_int{};
    uint32_t timehh_int{};
    uint32_t timedd_int{};
    uint32_t timehh_24_int{};

    uint32_t ringhh{};
    uint32_t ringmm{};

    std::array<char, TIME_CHARS_3> timehh{};
    std::array<char, TIME_CHARS_3> timemm{};
    std::array<char, TIME_CHARS_3> timehh_24{};
    std::array<char, TIME_CHARS_3> timedd{};
    std::array<char, TIME_CHARS_15> timemomo{};
} __attribute__((packed)) __attribute__((aligned(STimeVar_PACK_SIZE)));

// ///////////////////////////////////////////////////////////////////////////////////////////////////
namespace N_Common {
    extern unsigned int debug_msk;

    extern struct tm STimeInfo;
    extern struct tm STimeinfoLocal;

    extern std::unique_ptr<ESP32Time> rtc;
    extern std::array<char, MAX_CHAR_BUF_TIME> formated_time;

    extern EventGroupHandle_t handler_event;

    extern SemaphoreHandle_t xLCDAccess;
    extern SemaphoreHandle_t xSerial;
    extern SemaphoreHandle_t xI2C;

    extern TimerHandle_t one_x_timer;
    extern TimerHandle_t scd_cal_one_x_timer;
    extern TimerHandle_t one_x_timer_releaser;

    extern QueueHandle_t xQueueOn_NOW;
    extern QueueHandle_t xQueueOn_FCAST;
    extern QueueHandle_t xQueueOffline;
    extern QueueHandle_t xQueueRain;
    extern QueueHandle_t xTempOut;
    extern QueueHandle_t xClockMode;
    extern QueueHandle_t xPortalMode;
    extern QueueHandle_t x24h_Mode;
    extern QueueHandle_t xIdle_T;
    extern QueueHandle_t xInfluxDB_state;
    extern QueueHandle_t xXtend_BL_T;
    extern QueueHandle_t xInfluxFailedAtt;

    void initRtc();
    void defineSynchroElements();

}; // namespace N_Common

#include "helpers.hpp"
#endif // COMMONS_HPP
