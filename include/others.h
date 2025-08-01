#ifndef _OTHERS_H_
#define _OTHERS_H_

// ESP
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <Arduino.h>
#include <ESP32Time.h>
#include <SPIFFS.h>
#include <stdio.h>

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

// OTHERS
#include "btn.h"
#include "time.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// WIFI and CO
#define WEBSERVER_H
#define DEVICE "ESP32"
#define server_port 80
#define NOW 1
#define FCAST 0
// influx db
#define INFLUXDB_URL "http://192.168.1.17:8086"

#define INFLUXDB_BUCKET "esp32"
#define CONFIG_FILE "/user_data.json"

// SENSORS & BTN
#define lcd_addr 0x27
#define bme_addr 0x76
#define PIR_COUNTER_DELAY 6000
#define EXEC_BTN 27
#define TOUCH_THRES 40
#define temp_offset 1
#define LED_RAIN_NOW 5
#define LED_HOT 18
#define LED_COLD 19
#define LED_OPERATING 23
#define BUZZ 2
#define PIR_BL 17
#define SCD_CAL_TMS 1 * 60 * 1000 // 1 min for the co2 to reach \tau = 63%
#define CO2_THRES 1800
#define CO2_ALARM_REP_MM 10 * 60 * 1000 // 10 mins
#define TM_CHECK_RAIN 1 * 60 * 1000     // 1 min

// TASKS piority & period
#define P_LED 1
#define P_SENSORS 1
#define P_WM 2
#define P_API 1
#define P_ONLINE 1
#define P_OFFLINE 1
#define P_DAEMON 1

#define INPUTS_STATES 4
#define DEB_T 50
#define MENUS 6
#define API_TMS 15000
#define LED_MS 500
#define SENSORS_MS 5000
#define FAILED_ATT_THRES 6
#define DEM_MS 1000
#define PORTAL_TMT_MS 360
#define ONLINE_MS 500
#define OFFLINE_MS 500
#define CHECK_SERVER_STATUS_T                                                  \
    5 * 60 * 1000 // checks if the influx db server is up or not every 5 mins if
                  // it was down

// OTHERS
#define US_2_S 1000000
#define WAKE_ON_CLOCK_MODE
#define MAILBOX_QUEUE 1

extern int CLOCK_PENDING;

// EVENTS
// config mode is when editing settings
// clock valid is when everything is set for the clock to be run

// GROUPE EVENT STUFF SET
#define SYSSTATE_ACTIVE_BIT 0
#define WM_DONE 14
#define SCD_CAL_DONE 19

#define WAIT_CLK_BIT_LED 1    // WAITING clock for LED TASK
#define V_CLK_STATE_BIT_LED 2 // clock valid bit LED TASK
#define V_CLK_STATE_BIT_SET 3 // clock valid bit SETUP TASK
#define CLEAR_ONC_LED 4
#define V_START_PORTAL_BIT_SET                                                 \
    18 // validate the start portal thing to the setup function

// system on idle, so wait everyone
#define BTN_WAIT_MODE_BIT 5
#define LED_BUSY 6

#define WLAN_CONTD_BIT 7         // WLAN connected flag
#define CONF_ACTIVE_BIT_OFF 8    // configuration mode for OFFLINE MODE TASK
#define CONF_ACTIVE_BIT_ON 9     // configuration mode for ONLINE MODE TASK
#define SYSSTATE_IDLE_BIT_OFF 15 // SYSTEM idle for OFFLINE MODE TASK
#define SYSSTATE_IDLE_BIT_ON 16  // SYSTEM idle for ONLINE MODE TASK

#define CONF_ACTIVE_BIT_LED 10 // configuration mode for LED TASK
#define CONF_ACTIVE_BIT_SCR 11 // configuration mode for text scrolling
#define CONF_ACTIVE_BIT_DAE 12 // configuration mode for DAEMON TASK
#define CONF_ACTIVE_BIT_SET 13 // configuration mode for SETUP TASK
#define TIMER_FLAG_BIT 17

#define SYSNOTIF_NOW 0
#define SYSNOTIF_FCAST 1
#define SYSNOTIF_IN 2

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// DECLARATIONS
//////////////////////////////////////////////////////////////////////////////////////////////////////////

extern long user_gmt_offset, user_daylight_offset;
extern String user_timezone, user_ntp_server, user_ntp_server2;
extern String user_influx_token, user_influx_org, user_influx_url,
    user_influx_bucket;

// RTC RAM DATA
extern RTC_DATA_ATTR float
    SEALEVELPRESSURE_HPA_2; // this will be updated when esp goes online
extern RTC_DATA_ATTR int boot_times;
extern RTC_DATA_ATTR long SLEEP_DURATION_M;
extern RTC_DATA_ATTR long LCD_SPD;
extern RTC_DATA_ATTR bool RebootOnClock;

// OBJECTS DECLARATION
extern AsyncWebServer server;
extern LiquidCrystal_I2C lcd;   // LCD
extern Adafruit_BME280 bme;     // I2C sensor
extern SensirionI2cScd4x scd40; // co2+temp+humi sensor

extern WiFiManager wm; // wifi manager
extern ESP32Time rtc;
extern InfluxDBClient db_client;

// VAR
extern struct tm timeinfo, timeinfoLocal;
extern char formated_time[80];
extern long IDLE_TMS;

// SYNCHRO
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

extern TaskHandle_t xTaskOnline;
extern TaskHandle_t xTaskOffline;
extern TaskHandle_t xTaskDaemon;
extern TaskHandle_t xAPI;
extern TaskHandle_t xLed;
extern TaskHandle_t xWM;
extern TaskHandle_t xWelcome;
extern TaskHandle_t xSensor;
extern TaskHandle_t xTaskInfluxDb;

extern EventGroupHandle_t handler_event;

extern int debug_msk;
extern const char *AP_NAME;
extern const char *AP_PASWD;

extern String user_api_key;
extern String user_location_coordinate;

extern byte degre[8];
extern byte up[8];
extern byte down[8];

extern long config_set[MENUS][6];

// STRUCTURES
typedef struct data_t_online {
    String apiKey;
    String nameOfCity;

    String city;
    String description;
    String date_text;

    float tempMin;
    float tempMax;
    float temp;
    float feels_like;
    float windSpeed;

    float pressure;
    float humidity;
    float clouds;

    boolean forecast;
    String relative;
} dataOnline_t;

typedef struct on_data_t {
    String description;
    float temp;
    float feels_like;
    float windSpeed;
    float pressure;
    float humidity;
    float clouds;
    String city;
} on_Data_t;

typedef struct data_t2_offline {
    float temp;
    float temp2;
    float humi;
    float pressure;
    float altitude;
    float humi_bme;
    uint16_t co2;
} dataOffline_t;

typedef struct state {
    char in;
    char out_n;
    char out_f;
} state_t;

typedef struct input_bl_mgmt_t {
    unsigned int extended_T = 0;
    unsigned int up_to_T;
    bool extended_bl;
    bool turned_on = true;
} bl_mgmt_t;

typedef enum debug {
    debug_sensors,
    debug_wm,
    debug_setup,
    debug_api,
    debug_dbserver,
    debug_time,
    debug_daemon
} debug_t;

typedef struct main_menu {
    int current_menu;
    String menu[MENUS][2] = {{"CLOCK mode", ""}, {"LCD Speed", "ms"},
                             {"Sleep T", "ms"},  {"Deep-sleep T", "mins"},
                             {"Am/PM", ""},      {"Chg WLAN", ""}};
} menu_t;

typedef struct btn_state {
    bool flag[INPUTS_STATES];
} btn_state_t;

typedef struct last_reading {
    float temp;
    float humi;
    float pressure;
    float alti;
} last_reading_t;

typedef struct time_stuff {
    int timemm_int, ringhh, ringmm, timehh_int, timedd_int, timehh_24_int;
    char timehh[3], timehh_24[3], timemm[3], timedd[3], timemomo[15];
    bool pm;
} time_var_t;

// UTILS FUNCTIONS
// API
void parseResponeNow(JsonDocument doc, dataOnline_t &data_n);
void parseResponeFcast(JsonDocument doc, dataOnline_t &data_f);
void nowLogic(const char *pcTaskName, const String serverPath_now,
              dataOnline_t &data_n, float &last_press);
void fcastLogic(const char *pcTaskName, const String serverPath_forecast,
                dataOnline_t &data_f);

// SETUP
void defineSynchroStuff();
void handleMenuLogic(menu_t &menu_n, const int i, int &data_nexter,
                     long &current_param, bool &clk_24_mode, bool &chg_wlan,
                     bool &clk_mode, bool &valid_clk, bool &ghost_timer,
                     bool &tmr_flag);
void readBtnState(Btn &myBtn, btn_state_t &STATES_T);
void showCountDown(const int config_msk);
void applyMenuSave(menu_t &menu_n, long &current_param, bool &clk_24_mode,
                   bool &chg_wlan, bool &clk_mode);
void exitBeforeTimeout(const bool &clk_mode, bool &valid_clk, bool &ghost_timer,
                       bool &tmr_flag);

// CLOCK
void printIndoorLCD(const dataOffline_t &my_data_offline,
                    const time_var_t &time_vars, bool &was_co2,
                    int &ticktime_date, int &ticktime_co2, int &ticktime_server,
                    TickType_t &ticktime_buz_alarm, bool &db_server_state,
                    const char *pcTaskName);
void printOutdoorLCD(const float outTemp);
void getClockData(time_var_t &time_vars);
void printTimeLCD(const time_var_t &time_vars, bool clear_once,
                  bool clk_24h_mode, bool &last_blank);

// SENSORS IO
void clear_lcd();
void setIOConfig();
void driveRainLed(const String &rain, String *raining, const int size);
void offlineTaskPrint(const bool conf_active, dataOffline_t &my_data,
                      bool &validated_state);
void onlineTaskPrint(const bool conf_active, dataOnline_t &my_data,
                     bool &validated_state_now, bool &validated_state_fcast,
                     bool &read_now, String &_id);
bool scdSetup();
bool bmeSetup(dataOffline_t &data);
void lcdSetup();

// OTHERS
bool scroll_text(const String text, const int delay, const int line);
void id_n_talk(const String &id, const String &word_);
// void checkButton();

// SERVER and influx
void update_off_values(dataOffline_t &struct_2, const dataOffline_t &off_data);
void update_onl_values(on_Data_t &struct_, const dataOnline_t &data);
void server_init_stuff(on_Data_t &on_now, on_Data_t &on_fcast,
                       dataOffline_t &data_offline, dataOnline_t &data_online,
                       int &STATE);
void getServer_State(bool &state, const char *pcTaskName);
bool writePointToServer(Point &sensor, dataOffline_t data,
                        const char *pcTaskName, int &failed_try);
void updateServerValues(TickType_t xTick_offline, dataOffline_t &my_data_off,
                        dataOffline_t &data_offline, TickType_t xTick_online,
                        dataOnline_t &my_data, on_Data_t &on_now,
                        on_Data_t &on_fcast);

// SYS & CO
String getParam(String name);
void reboot_it(int secon_);
void readSysConfig();
bool createTimer(const long duration, TimerCallbackFunction_t(call_func_ptr),
                 const char *pcTaskName, const char *timerName);
void clockOrStaMode();
void resolveInputs(menu_t &menu_n, const int i, int &data_nexter,
                   long &current_param, bool &clk_24_mode, bool &chg_wlan,
                   bool &clk_mode, bool &valid_clk, bool &ghost_timer,
                   bool &tmr_flag, btn_state_t &STATES_T, const int config_T,
                   const char *pcTaskName, int &config_msk,
                   TickType_t &xLastWakeTime_c);
void checkCExecPortal(const bool valid_portal);
void pirLogic(bool &pir_state, bool &last_pir, bl_mgmt_t &that_mgmt,
              TickType_t &last_detected_pir, const bool valid_clk);
void idleSTAMode(bool conf_active, bool &hard_resume, int &rep,
                 bool &check_go_idle_now);
void wifiCoIssues(TickType_t &xTick_wlan_state, const bool valid_clk,
                  bool &reboot_reconfd, TickType_t &xtick_unknown, int &cnt,
                  TickType_t &xLastLedOn, bool &flicker_led,
                  bool &last_flick_state);
void taskNotifMan(uint32_t rv, uint32_t &rv_msk, bool &idle);
void dispMenu(menu_t &menu_n, const bool &clk_24_mode, const bool &chg_wlan,
              const bool &clk_mode);
void launchMenuTimout(const bool tmr_flag, const bool ghost_timer,
                      const int config_T, const char *pcTaskName,
                      int &config_msk, TickType_t &xLastWakeTime_c);
void broadcastConfMode(bool clear = false);

// CALLBACK def
void timer_daemon(TimerHandle_t xTimer);
void scd_cal_daemon(TimerHandle_t xTimer);
void releaser_daemon(TimerHandle_t xTimer);
void callback_touch_wakeup();
void saveParamCallback();

/////////////////////////////////////////////////////////

#endif //_OTHERS_H_
