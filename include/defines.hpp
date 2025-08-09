#ifndef DEFINES_HPP
#define DEFINES_HPP

#define WAKE_ON_CLOCK_MODE

// WIFI and CO
#define WEBSERVER_H

// STACK VAL
constexpr int TASK_STAKE_2K{2048};
constexpr int TASK_STAKE_4K{4096};
constexpr int TASK_STAKE_8K{8192};

// LCD
constexpr int LCD_LINE_0{0};
constexpr int LCD_LINE_1{1};

constexpr int LCD_MAX_CHARS{16};
constexpr int LCD_LINES_MAX{2};

constexpr int LCD_CURSOR_0{0};
constexpr int LCD_CURSOR_1{1};
constexpr int LCD_CURSOR_2{2};
constexpr int LCD_CURSOR_3{3};
constexpr int LCD_CURSOR_4{4};
constexpr int LCD_CURSOR_5{5};
constexpr int LCD_CURSOR_6{6};
constexpr int LCD_CURSOR_7{7};
constexpr int LCD_CURSOR_8{8};
constexpr int LCD_CURSOR_9{9};
constexpr int LCD_CURSOR_10{10};
constexpr int LCD_CURSOR_11{11};
constexpr int LCD_CURSOR_12{12};
constexpr int LCD_CURSOR_13{13};
constexpr int LCD_CURSOR_14{14};
constexpr int LCD_CURSOR_15{15};

// delays
constexpr int US_2_S{1000 * 1000};

constexpr int T_10_MS{10};
constexpr int T_40_MS{40};
constexpr int T_50_MS{50};
constexpr int T_100_MS{100};
constexpr int T_200_MS{200};
constexpr int T_250_MS{250};
constexpr int T_300_MS{300};
constexpr int T_400_MS{400};
constexpr int T_450_MS{450};
constexpr int T_500_MS{500};
constexpr int T_1000_MS{1000};

constexpr int T_1_S_MS{1 * 1000};
constexpr int T_3_S_MS{3 * 1000};
constexpr int T_10_S_MS{10 * 1000};
constexpr int T_15_S_MS{15 * 1000};
constexpr int T_20_S_MS{20 * 1000};
constexpr int T_25_S_MS{25 * 1000};
constexpr int T_30_S_MS{30 * 1000};
constexpr int T_40_S_MS{40 * 1000};
constexpr int T_50_S_MS{50 * 1000};
constexpr int T_60_S_MS{60 * 1000};

constexpr int T_5_SEC{5};
constexpr int T_10_SEC{10};
constexpr int T_30_SEC{30};

constexpr int T_15_MIN{15};
constexpr int T_20_MIN{20};
constexpr int T_30_MIN{30};
constexpr int T_35_MIN{35};
constexpr int T_45_MIN{45};
constexpr int T_60_MIN{60};

// TIME
constexpr unsigned int HOURS_22_PM{22U};
constexpr unsigned int HOURS_6_AM{6U};
constexpr unsigned int HOUR_12{12U};
constexpr unsigned int HOUR_2{2U};
constexpr unsigned int DAY_10{10U};
constexpr unsigned int TIME_MIN_0{0U};
constexpr unsigned int HOUR_23{23U};
constexpr unsigned int HOUR_9{9U};
constexpr unsigned int HOUR_10{10U};

// WEATHER API & parsing literals
constexpr unsigned int NOW{1};
constexpr unsigned int FCAST{0};

const std::string_view str_temp{"temp"};
const std::string_view str_main{"main"};
const std::string_view str_weather{"weather"};
const std::string_view str_desc{"description"};
const std::string_view str_f_like{"feels_like"};
const std::string_view str_temp_min{"temp_min"};
const std::string_view str_temp_max{"temp_max"};
const std::string_view str_wind{"wind"};
const std::string_view str_speed{"speed"};
const std::string_view str_pressure{"pressure"};
const std::string_view str_humi{"humidity"};
const std::string_view str_clouds{"clouds"};
const std::string_view str_all{"all"};
const std::string_view str_name{"name"};
const std::string_view str_city{"city"};
const std::string_view str_list{"list"};
const std::string_view str_d_txt{"dt_txt"};

// SYS
constexpr int MIN_TO_S{60};
constexpr int BASE_10{10};
constexpr int INPUTS_STATES = 4;
constexpr int TEN_MS_X{10};
constexpr int BUZZ_ON{1};
constexpr int BUZZ_OFF{0};
constexpr int INVALID_CO2_VAL{0xffff};
constexpr float SEA_LEVEL_PRESSURE_INIT{1020.0};
constexpr float INIT_PRESSURE_VALUE{1013.0F};
constexpr float PRESSURE_CALC_100_FLOAT{100.0F};
constexpr unsigned int SCD_ERR_MSG_LEN{256U};
constexpr int LCD_BYTES_CNT_8{8};
constexpr int CHAR_CODE_DEGRE{223};

constexpr int CLK_OR_STA_STA_MODE{0};
constexpr int CLK_OR_STA_CLOCK_MODE{1};
constexpr int CLK_OR_STA_CLOCK_STA_NONE{2};

constexpr unsigned int SYSNOTIF_NOW{0U};
constexpr unsigned int SYSNOTIF_FCAST{1U};
constexpr unsigned int SYSNOTIF_IN{2U};

constexpr int MAX_CHAR_BUF_TIME{80};

constexpr int TEMP_0{0};
constexpr int TEMP_10{10};
constexpr int TEMP_NEG_10{-10};
constexpr int TEMP_NEG_9{-9};
constexpr int HUMI_FULL{100};

constexpr int MENUS{6};
constexpr int MENU_0{0};
constexpr int MENU_1{1};
constexpr int MENU_2{2};
constexpr int MENU_3{3};
constexpr int MENU_4{4};
constexpr int MENU_5{5};
constexpr int MENU_INNER_6{6};
constexpr int MENU_INNER_2{2};

constexpr int MENU_CYCLE_INNER{0};
constexpr int MENU_FORWARD{1};
constexpr int MENU_BACKWARD{2};
constexpr int MENU_VALIDATE_AND_EXIT{3};

// WEB_SERVER
constexpr int SUCCESSFULL{200};
constexpr int NO_CONTENT{204};

// EVENTS
// config mode is when editing settings
// clock valid is when everything is set for the clock to be run

// GROUPE EVENT STUFF SET
constexpr unsigned int SYSSTATE_ACTIVE_BIT{0U};
constexpr unsigned int WAIT_CLK_BIT_LED{1U};    // WAITING clock for LED TASK
constexpr unsigned int V_CLK_STATE_BIT_LED{2U}; // clock valid bit LED TASK

// clock valid bit SETUP TASK validate the start portal thing to the setup
// function
constexpr unsigned int V_CLK_STATE_BIT_SET{3U};
constexpr unsigned int CLEAR_ONC_LED{4U};

// system on idle, so wait everyone
constexpr unsigned int BTN_WAIT_MODE_BIT{5U};
constexpr unsigned int LED_BUSY{6U};

// WLAN connected flag
constexpr unsigned int WLAN_CONTD_BIT{7U};

// configuration mode for OFFLINE MODE TASK
constexpr unsigned int CONF_ACTIVE_BIT_OFF{8U};

// configuration mode for ONLINE MODE TASK
constexpr unsigned int CONF_ACTIVE_BIT_ON{9U};

// configuration mode for LED TASK
constexpr unsigned int CONF_ACTIVE_BIT_LED{10U};

// configuration mode for text scrolling
constexpr unsigned int CONF_ACTIVE_BIT_SCR{11U};

// configuration mode for DAEMON TASK
constexpr unsigned int CONF_ACTIVE_BIT_DAE{12U};

// configuration mode for SETUP TASK
constexpr unsigned int CONF_ACTIVE_BIT_SET{13U};

constexpr unsigned int WM_DONE{14U};

// SYSTEM idle for OFFLINE MODE TASK
constexpr unsigned int SYSSTATE_IDLE_BIT_OFF{15U};

// SYSTEM idle for ONLINE MODE TASK
constexpr unsigned int SYSSTATE_IDLE_BIT_ON{16U};

constexpr unsigned int TIMER_FLAG_BIT{17U};
constexpr unsigned int V_START_PORTAL_BIT_SET{18U};
constexpr unsigned int SCD_CAL_DONE{19U};

#endif // DEFINES_HPP
