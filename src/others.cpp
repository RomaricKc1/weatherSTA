#include "others.h"
#include <Arduino.h>

#ifdef WAKE_ON_CLOCK_MODE
int CLOCK_PENDING = 1;
#else
int CLOCK_PENDING = 0;
#endif

// RTC RAM DATA
RTC_DATA_ATTR float SEALEVELPRESSURE_HPA_2 =
	1020; // this will be updated when esp goes online
RTC_DATA_ATTR int boot_times = 0;
RTC_DATA_ATTR long SLEEP_DURATION_M = 15;
RTC_DATA_ATTR long LCD_SPD = 250;
RTC_DATA_ATTR bool RebootOnClock = false;

// LCD
byte degre[8] = {B00110, B01001, B01001, B00110,
				 B00000, B00000, B00000, B00000};
byte up[8] = {B00100, B01110, B10101, B00100, B00100, B00100, B00100, B00100};
byte down[8] = {B00100, B00100, B00100, B00100, B00100, B10101, B01110, B00100};

// TIME
struct tm timeinfo, timeinfoLocal;

const char *AP_NAME = "STA esp";
const char *AP_PASWD = "P@55w0rd";

long IDLE_TMS = 15000;
char formated_time[80] = "";

String user_api_key(""), user_location_coordinate("");

long config_set[MENUS][6] = {
	{1, 0, 1, 0, 1, 0},							// clock 0's
	{250, 200, 300, 400, 450, 500},				// lcd_speed 1st
	{30000, 20000, 25000, 40000, 50000, 60000}, // sleep_T 2nd
	{15, 20, 30, 35, 45, 60},					// deep_sleep_T
	{0, 1, 0, 1, 0, 1},							// clock AM/PM
	{0, 1, 0, 1, 0, 1},							// wlan portal ondemand 5th
};

// OTHERS
int debug_msk = 0b00000000;

// SYNCHRO
SemaphoreHandle_t xLCDAccess = nullptr;
SemaphoreHandle_t xSerial = nullptr;
SemaphoreHandle_t xI2C = nullptr;

TimerHandle_t one_x_timer = nullptr;
TimerHandle_t scd_cal_one_x_timer = nullptr;
TimerHandle_t one_x_timer_releaser = nullptr;

QueueHandle_t xQueueOn_NOW = nullptr;
QueueHandle_t xQueueOn_FCAST = nullptr;
QueueHandle_t xQueueOffline = nullptr;
QueueHandle_t xQueueRain = nullptr;
QueueHandle_t xTempOut = nullptr;
QueueHandle_t xClockMode = nullptr;
QueueHandle_t xPortalMode = nullptr;
QueueHandle_t x24h_Mode = nullptr;
QueueHandle_t xIdle_T = nullptr;
QueueHandle_t xInfluxDB_state = nullptr;
QueueHandle_t xXtend_BL_T = nullptr;

TaskHandle_t xTaskOnline = nullptr;
TaskHandle_t xTaskOffline = nullptr;
TaskHandle_t xTaskDaemon = nullptr;
TaskHandle_t xAPI = nullptr;
TaskHandle_t xLed = nullptr;
TaskHandle_t xWM = nullptr;
TaskHandle_t xWelcome = nullptr;
TaskHandle_t xSensor = nullptr;

EventGroupHandle_t handler_event;

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION DEF
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// UTILS FUNCTIONS
// API
void parseResponeNow(StaticJsonDocument<1024> doc, dataOnline_t &data_n)
{
	JsonObject weather_0 = doc["weather"][0];
	JsonObject main = doc["main"];

	const char *weather_0_description = weather_0["description"];
	data_n.description = (String)weather_0_description;

	data_n.temp = main["temp"];
	data_n.feels_like = main["feels_like"];
	data_n.tempMin = main["temp_min"];
	data_n.tempMax = main["temp_max"];
	data_n.windSpeed = doc["wind"]["speed"];
	data_n.pressure = main["pressure"];
	data_n.humidity = main["humidity"];
	data_n.clouds = doc["clouds"]["all"];

	const char *name = doc["name"]; // e.g. "Nantes"

	data_n.city = (String)name;

	data_n.forecast = false;
	data_n.relative = "NOW";
	SEALEVELPRESSURE_HPA_2 = data_n.pressure;
}

void parseResponeFcast(StaticJsonDocument<1536> doc, dataOnline_t &data_f)
{
	JsonObject list_0 = doc["list"][0];
	JsonObject list_0_main = list_0["main"];
	JsonObject list_0_wind = list_0["wind"];
	JsonObject list_0_weather_0 = list_0["weather"][0];
	JsonObject city = doc["city"];

	data_f.temp = list_0_main["temp"];
	data_f.feels_like = list_0_main["feels_like"];
	data_f.tempMin = list_0_main["temp_min"];
	data_f.tempMax = list_0_main["temp_max"];
	data_f.pressure = list_0_main["pressure"];
	data_f.humidity = list_0_main["humidity"];

	const char *list_0_weather_0_description = list_0_weather_0["description"];
	data_f.description = (String)list_0_weather_0_description;

	data_f.clouds = list_0["clouds"]["all"];
	data_f.windSpeed = list_0_wind["speed"];

	const char *list_0_dt_txt = list_0["dt_txt"];
	data_f.date_text = (String)list_0_dt_txt;

	const char *city_name = city["name"]; // "Nantes"
	data_f.city = (String)city_name;

	data_f.forecast = true;
	data_f.relative = "FORECAST";
}

void fcastLogic(const char *pcTaskName, const String serverPath_forecast,
				dataOnline_t &data_f)
{
	HTTPClient http;
	WiFiClient client;

	http.begin(client, serverPath_forecast.c_str());
	int httpResponseCode = http.GET();

	if (!(httpResponseCode > 0))
	{
		if (debug_msk & (1 << debug_api))
		{
			id_n_talk(pcTaskName, "Forecast Error code: " + (String)httpResponseCode);
		}
		return;
	}

	StaticJsonDocument<1536> doc;
	DeserializationError error = deserializeJson(doc, http.getStream());
	http.end();

	if (error)
	{
		if (debug_msk & (1 << debug_api))
		{
			id_n_talk(pcTaskName, "Forecast Error code:deserializeJson() failed: " +
									  (String)error.c_str());
		}
		return;
	}

	parseResponeFcast(doc, data_f);

	xQueueOverwrite(xQueueOn_FCAST, &data_f);

	if (debug_msk & (1 << debug_api))
	{
		id_n_talk(pcTaskName, "Sent fcast, f like: " + (String)data_f.feels_like);
	}
}

void nowLogic(const char *pcTaskName, const String serverPath_now,
			  dataOnline_t &data_n, float &last_press)
{
	HTTPClient http;
	WiFiClient client;

	http.begin(client, serverPath_now.c_str());
	int httpResponseCode = http.GET();

	if (!(httpResponseCode > 0))
	{
		if (debug_msk & (1 << debug_api))
		{
			id_n_talk(pcTaskName, "Now Error code: " + (String)httpResponseCode);
		}
		return;
	}

	StaticJsonDocument<1024> doc;
	DeserializationError error = deserializeJson(doc, http.getStream());
	http.end();

	if (error)
	{
		if (debug_msk & (1 << debug_api))
		{
			id_n_talk(pcTaskName, "Now Error code:deserializeJson() failed: " +
									  (String)error.c_str());
		}
		return;
	}

	parseResponeNow(doc, data_n);

	(SEALEVELPRESSURE_HPA_2 == 0) ? SEALEVELPRESSURE_HPA_2 = last_press
								  : last_press = SEALEVELPRESSURE_HPA_2;

	xQueueOverwrite(xQueueOn_NOW, &data_n);

	String rain_desc_queue = data_n.description;
	float temp_queue = data_n.temp;
	xQueueOverwrite(xQueueRain, &rain_desc_queue);
	xQueueOverwrite(xTempOut, &temp_queue);

	if (debug_msk & (1 << debug_api))
	{
		id_n_talk(pcTaskName, "Sent now, f like: " + (String)data_n.feels_like);
	}
}

// SETUP
void defineSynchroStuff()
{
	// mutexes and queues (they are static var, no tied to the function
	// whatsoever)

	// event manager init
	handler_event = xEventGroupCreate();
	assert(handler_event);

	// mutexes  init
	xSerial = xSemaphoreCreateMutex();
	assert(xSerial);
	xLCDAccess = xSemaphoreCreateMutex();
	assert(xLCDAccess);
	xI2C = xSemaphoreCreateMutex();
	assert(xI2C);

	// quueues init
	xQueueOn_FCAST = xQueueCreate(MAILBOX_QUEUE, sizeof(dataOnline_t));
	assert(xQueueOn_FCAST);
	xQueueOn_NOW = xQueueCreate(MAILBOX_QUEUE, sizeof(dataOnline_t));
	assert(xQueueOn_NOW);

	xQueueOffline = xQueueCreate(MAILBOX_QUEUE, sizeof(dataOffline_t));
	assert(xQueueOffline);

	xQueueRain = xQueueCreate(MAILBOX_QUEUE, sizeof(String));
	assert(xQueueRain);

	xTempOut = xQueueCreate(MAILBOX_QUEUE, sizeof(float));
	assert(xTempOut);

	xClockMode = xQueueCreate(MAILBOX_QUEUE, sizeof(bool));
	assert(xClockMode);

	x24h_Mode = xQueueCreate(MAILBOX_QUEUE, sizeof(bool));
	assert(x24h_Mode);

	xIdle_T = xQueueCreate(MAILBOX_QUEUE, sizeof(int));
	assert(xIdle_T);

	xPortalMode = xQueueCreate(MAILBOX_QUEUE, sizeof(bool));
	assert(xPortalMode);

	xInfluxDB_state = xQueueCreate(MAILBOX_QUEUE, sizeof(bool));
	assert(xInfluxDB_state);

	xXtend_BL_T = xQueueCreate(MAILBOX_QUEUE, sizeof(int));
	assert(xXtend_BL_T);
}

void handleMenuLogic(menu_t &menu_n, const int i, int &data_nexter,
					 long &current_param, bool &clk_24_mode, bool &chg_wlan,
					 bool &clk_mode, bool &valid_clk, bool &ghost_timer,
					 bool &tmr_flag)
{
	if (debug_msk & (1 << debug_setup))
	{
		id_n_talk("vSetup", "called handle menu");
	}
	switch (i)
	{
	case 2: // dec menu (x3 click)
		if (menu_n.current_menu == 0)
		{
			menu_n.current_menu =
				MENUS - 1; // allow to cycle the menu e.g. current menu: 5, 4, 3
		}
		else
		{
			menu_n.current_menu--; // current 5? 4, 3, 2
		}
		break;

	case 1: // inc menu (2x click)
		if (menu_n.current_menu == MENUS - 1)
		{
			menu_n.current_menu =
				0; // allow to cycle the menu e.g. current menu: 5, 1, 2
		}
		else
		{
			menu_n.current_menu++;
		}
		break;

	case 0: // exec aka cycle the current menu page variable (1x click)
		// actually grab what is displayed and yeah, save it
		if (data_nexter > 5) // CHNG: should be 5
		{
			// if higher than the allowed elements 0 1 2 3 4 5
			data_nexter = 0;
		}
		// pick the value for this exact index and menu page using preset available,
		// and inc the idx
		current_param = config_set[menu_n.current_menu][data_nexter++];
		// apply the option right away
		applyMenuSave(menu_n, current_param, clk_24_mode, chg_wlan, clk_mode);
		break;

	case 3: // exit menu before timeout (long press)
		// checks the different state, validate the bit, remove the current timer,
		// and create a new one really fast (like 2ms) that will expire right away
		exitBeforeTimeout(clk_mode, valid_clk, ghost_timer, tmr_flag);
		break;

	default:
		break;
	}
}

void readBtnState(Btn &myBtn, btn_state_t &STATES_T)
{
	myBtn.tick(); // btn state cleared each time

	switch ((myBtn.getBtn_state()))
	{
	case Btn::BTN_CLICK:
		if (debug_msk & (1 << debug_setup))
		{
			id_n_talk("vSetup", "that's a click");
		}
		myBtn.clearBtn_state();
		STATES_T.flag[0] = 1;
		break;
	case Btn::BTN_X2_CLICK:
		myBtn.clearBtn_state();
		STATES_T.flag[1] = 1;
		break;
	case Btn::BTN_X3_CLICK:
		myBtn.clearBtn_state();
		STATES_T.flag[2] = 1;
		break;
	case Btn::BTN_PRESS:
		if (debug_msk & (1 << debug_setup))
		{
			id_n_talk("vSetup", "that's a long press");
		}
		myBtn.clearBtn_state();
		STATES_T.flag[3] = 1;
		break;

	default:
		break;
	}
}

void showCountDown(const int config_msk)
{
	// show countdown
	if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
	{
		if (config_msk < 10) // 9, 8, ...
		{
			lcd.setCursor(14, 0);
			lcd.print((int)0);
			lcd.setCursor(15, 0);
		}
		else
		{
			lcd.setCursor(14, 0);
		}
		lcd.print(config_msk);

		xSemaphoreGive(xI2C);
	}
}

void applyMenuSave(menu_t &menu_n, long &current_param, bool &clk_24_mode,
				   bool &chg_wlan, bool &clk_mode)
{
	switch (menu_n.current_menu)
	{
	case 1: // lcd scroll speed
		if (current_param != 0)
		{
			LCD_SPD = current_param; // global include param
		}
		break;
	case 2: // sleep param
		if (current_param != 0)
		{
			IDLE_TMS = current_param;
			xQueueOverwrite(xIdle_T,
							(void *)&IDLE_TMS); // local so gotta send it to the queue
		}
		break;
	case 3: // deep sleep duration
		if (current_param != 0)
		{
			SLEEP_DURATION_M = current_param;
		}
		break;
	case 4: // AM / PM mode
		clk_24_mode = current_param;
		xQueueOverwrite(x24h_Mode, (void *)&clk_24_mode);
		break;
	case 5: // WLAN portal ondemand
		chg_wlan = current_param;
		xQueueOverwrite(xPortalMode, (void *)&chg_wlan);
		break;
	case 0: // clock mode
		clk_mode = current_param;
		xQueueOverwrite(xClockMode, (void *)&clk_mode);

		break;
	default:
		break;
	}
}

void exitBeforeTimeout(bool &clk_mode, bool &valid_clk, bool &ghost_timer,
					   bool &tmr_flag)
{
	EventBits_t event_result;
	/*event await timer_flag and valid_clk*/
	if (clk_mode == 1)
	{
		xEventGroupSetBits(handler_event,
						   ((1 << V_CLK_STATE_BIT_SET) |
							(1 << V_CLK_STATE_BIT_LED))); // validate the mode now
		xEventGroupClearBits(handler_event, ((1 << WAIT_CLK_BIT_LED)));
	}
	event_result = xEventGroupWaitBits(handler_event, (1 << TIMER_FLAG_BIT),
									   pdFALSE, pdFALSE, 0);
	(event_result & (1 << TIMER_FLAG_BIT)) ? tmr_flag = true : tmr_flag = false;

	if (tmr_flag == true)
	{
		// countdown timer were already running: stops it and delete it
		xTimerStop(one_x_timer_releaser, portMAX_DELAY);
		xTimerDelete(one_x_timer_releaser, portMAX_DELAY);

		// recreate a timer and launch it with a small delay time (2ms)
		bool res_timer =
			createTimer(pdMS_TO_TICKS(2), releaser_daemon, "vSetup", "one-x-timer");

		if (res_timer == pdPASS)
		{
			ghost_timer = 1;
		}
	}
}

// CLOCK
void getClockData(time_var_t &time_vars)
{
	timeinfoLocal = rtc.getTimeStruct();

	strftime(time_vars.timehh, sizeof(time_vars.timehh), "%I",
			 &timeinfoLocal); // hour 12h
	strftime(time_vars.timehh_24, sizeof(time_vars.timehh_24), "%H",
			 &timeinfoLocal);
	strftime(time_vars.timemm, sizeof(time_vars.timemm), "%M",
			 &timeinfoLocal); // minutes
	strftime(time_vars.timedd, sizeof(time_vars.timedd), "%d",
			 &timeinfoLocal); // day of the month
	strftime(time_vars.timemomo, sizeof(time_vars.timemomo), "%B",
			 &timeinfoLocal); // month in text

	// convert to integer
	time_vars.timehh_int = atoi(time_vars.timehh);
	time_vars.timehh_24_int = atoi(time_vars.timehh_24);
	time_vars.timemm_int = atoi(time_vars.timemm);
	time_vars.timedd_int = atoi(time_vars.timedd);

	// Mid-night/day 12 am/pm signal
	if (time_vars.timehh_int == 12)
	{
		if (time_vars.ringhh == 1)
		{
			time_vars.ringhh = 0;
			digitalWrite(BUZZ, 1);
			vTaskDelay(pdMS_TO_TICKS(400));
			digitalWrite(BUZZ, 0);
		}
	}
	else
	{
		// re-arm for the next 12 (gonna be AM or PM)
		time_vars.ringhh = 1;
	}

	// for every hour, soft ring :: except from 2 to 6
	if (time_vars.timemm_int == 0 &&
		(time_vars.timehh_24_int < 2 || time_vars.timehh_24_int > 6))
	{
		if (time_vars.ringmm == 1)
		{
			time_vars.ringmm = 0;
			digitalWrite(BUZZ, 1);
			vTaskDelay(pdMS_TO_TICKS(100));
			digitalWrite(BUZZ, 0);
			vTaskDelay(pdMS_TO_TICKS(100));
			digitalWrite(BUZZ, 1);
			vTaskDelay(pdMS_TO_TICKS(100));
			digitalWrite(BUZZ, 0);
		}
	}
	else
	{
		time_vars.ringmm = 1;
	}

	(rtc.getAmPm(true) == "pm") ? time_vars.pm = true : time_vars.pm = false;
}

void printTimeLCD(const time_var_t time_vars, bool clear_once,
				  bool clk_24h_mode, bool &last_blank)
{
	if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdFALSE)
	{
		return;
	}

	/*clear_one event*/
	if (clear_once == true)
	{
		xEventGroupClearBits(handler_event, ((1 << CLEAR_ONC_LED)));

		lcd.setCursor(0, 0);
		lcd.print("                ");
		lcd.setCursor(0, 1);
		lcd.print("                ");
	}

	if (clk_24h_mode == false)
	{
		if (time_vars.timehh_int < 10)
		{
			lcd.setCursor(4, 0);
			lcd.print(" ");
			lcd.setCursor(5, 0);
			lcd.print(time_vars.timehh_int);
		}
		else
		{
			lcd.setCursor(4, 0);
			lcd.print(time_vars.timehh_int);
		}
	}
	else
	{
		if (time_vars.timehh_24_int < 10)
		{
			lcd.setCursor(4, 0);
			lcd.print("0");
			lcd.setCursor(5, 0);
			lcd.print(time_vars.timehh_24_int);
		}
		else
		{
			lcd.setCursor(4, 0);
			lcd.print(time_vars.timehh_24_int);
		}
	}
	// : separator
	lcd.setCursor(6, 0);
	if (last_blank == true)
	{
		lcd.print(":");
		last_blank = false;
	}
	else
	{
		lcd.print(" ");
		last_blank = true;
	}

	lcd.setCursor(7, 0);
	lcd.print(time_vars.timemm);
	lcd.setCursor(9, 0);

	if (clk_24h_mode == false)
	{
		(time_vars.pm == true) ? lcd.print("pm") : lcd.print("am");
	}
	else
	{
		lcd.print("  ");
	}

	xSemaphoreGive(xI2C);
}

void printIndoorLCD(const dataOffline_t my_data_offline,
					const time_var_t time_vars, bool &was_co2,
					int &ticktime_date, int &ticktime_co2, int &ticktime_server,
					TickType_t &ticktime_buz_alarm, bool &db_server_state,
					const char *pcTaskName)
{
	if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdFALSE)
	{
		return;
	}

	BaseType_t rc = pdFALSE;

	// clear the section first
	lcd.setCursor(0, 1);
	lcd.print("            ");

	// date or co2
	if (was_co2 == true)
	{
		ticktime_date++;
		if (ticktime_date >= 3)
		{
			ticktime_date = 0;
			// pass it to the co2 indicator
			was_co2 = false;
		}
		// print date
		if (time_vars.timedd_int < 10)
		{
			lcd.setCursor(1, 1);
			lcd.print(time_vars.timedd_int);
		}
		else
		{
			lcd.setCursor(0, 1);
			lcd.print(time_vars.timedd_int);
		}
		lcd.setCursor(3, 1);
		lcd.print(time_vars.timemomo);
	}
	else
	{
		ticktime_server++;
		if (ticktime_server >= (SENSORS_MS / LED_MS))
		{
			ticktime_server = 0;
			// every sensor duration, read the queue, peek the data,
			rc = xQueuePeek(xInfluxDB_state, (void *)&db_server_state, 0);
		}

		if ((debug_msk & (1 << debug_dbserver)) && rc == pdPASS)
		{
			id_n_talk(pcTaskName, "Recvd server state: " + (String)db_server_state);
		}

		// print dbserver status
		lcd.setCursor(0, 1);
		(db_server_state == true) ? lcd.print(":)") : lcd.print(":(");

		ticktime_co2++;
		if (ticktime_co2 >= 12)
		{
			ticktime_co2 = 0;
			// pass it to the date indicator
			was_co2 = true;
		}

		// print co2 level
		lcd.setCursor(3, 1);
		if (my_data_offline.co2 == 0xffff)
		{
			lcd.print("...");
		}
		else
		{
			lcd.print(my_data_offline.co2);
		}

		lcd.print("ppm");
	}

	// alert on Co2 high values, during daytime (except from 1 to 6) for every
	// CO2_ALARM_REP_MM minutes
	if ((time_vars.timehh_24_int < 23 || time_vars.timehh_24_int > 9) &&
		((xTaskGetTickCount() - ticktime_buz_alarm) >= CO2_ALARM_REP_MM) &&
		(my_data_offline.co2 >= CO2_THRES))
	{
		// buz here
		ticktime_buz_alarm = xTaskGetTickCount();

		// printf("co2 :: %d\n", my_data_offline.co2);
		for (int i = 0; i < 2; i++)
		{
			digitalWrite(BUZZ, 1);
			vTaskDelay(pdMS_TO_TICKS(40 + (i * 10)));
			digitalWrite(BUZZ, 0);
			vTaskDelay(pdMS_TO_TICKS(40 + (i * 10)));
		}
	}

	// temp and humi
	lcd.setCursor(0, 0);
	lcd.print((int)(round(my_data_offline.temp)));

	if ((int)(round(my_data_offline.temp)) >= 0 &&
		(int)(round(my_data_offline.temp)) < 10)
	{
		lcd.setCursor(1, 0);
		lcd.write(byte(0));
		lcd.setCursor(2, 0);
		lcd.print("  ");
	}
	else if (((int)(round(my_data_offline.temp)) < 0 &&
			  (int)(round(my_data_offline.temp)) > -10) ||
			 ((int)(round(my_data_offline.temp)) >= 10))
	{
		lcd.setCursor(2, 0);
		lcd.write(byte(0));
		lcd.setCursor(3, 0);
		lcd.print(" ");
	}
	else if ((int)(round(my_data_offline.temp)) < -9)
	{ //-10, -11, ...
		lcd.setCursor(3, 0);
		lcd.write(byte(0));
	}

	// humi
	lcd.setCursor(11, 0);
	if ((int)my_data_offline.humi == 100)
	{
		String humi_thing = " " + (String)((int)my_data_offline.humi) + "%";
		lcd.print(humi_thing);
	}
	else if ((int)my_data_offline.humi > 100)
	{
		String humi_thing = " err";
		lcd.print(humi_thing);
	}
	else if ((int)my_data_offline.humi < 100)
	{
		String humi_thing = "  " + (String)((int)my_data_offline.humi) + "%";
		lcd.print(humi_thing);
	}

	xSemaphoreGive(xI2C);
}

void printOutdoorLCD(const float outTemp)
{
	if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdFALSE)
	{
		return;
	}
	if (xQueuePeek(xTempOut, (void *)&outTemp, 0) == pdFAIL)
	{
		// make sure not to take the i2c and not return it before returning
		xSemaphoreGive(xI2C);
		return;
	}

	// out temp 12 13 14 15
	//          -  " "  x  °  < 0  - auto
	//          +   y   x  °  > 10 + auto

	lcd.setCursor(12, 1);
	lcd.print((int)(round(outTemp)));

	if (((int)(round(outTemp))) >= 0 and
		((int)(round(outTemp)) < 10))
	{ // 0, .. 9
		lcd.setCursor(13, 1);
		lcd.write(byte(0));
		lcd.setCursor(14, 1);
		lcd.print("  ");
	}
	else if (((int)(round(outTemp)) < 0 && (int)(round(outTemp)) > -10) ||
			 ((int)(round(outTemp)) >=
			  10))
	{ //-9, ..., -1   or   10, 11, 12, ...
		lcd.setCursor(14, 1);
		lcd.write(byte(0));
		lcd.setCursor(15, 1);
		lcd.print(" ");
	}
	else if ((int)(round(outTemp)) < -9)
	{ //-10, -11, ...
		lcd.setCursor(15, 1);
		lcd.write(byte(0));
	}

	xSemaphoreGive(xI2C);
}

// SENSORS IO
void clear_lcd()
{
	if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
	{
		lcd.setCursor(0, 0);
		lcd.print("                ");
		lcd.setCursor(0, 1);
		lcd.print("                ");

		xSemaphoreGive(xI2C);
	}
}

void setIOConfig()
{
	pinMode(LED_RAIN_NOW, OUTPUT);
	pinMode(LED_HOT, OUTPUT);
	pinMode(LED_COLD, OUTPUT);
	pinMode(LED_OPERATING, OUTPUT);

	pinMode(BUZZ, OUTPUT);
	pinMode(PIR_BL, INPUT);

	digitalWrite(LED_OPERATING, LOW);
	digitalWrite(LED_COLD, LOW);
	digitalWrite(LED_HOT, LOW);
}

void driveRainLed(const String &rain, String *raining, const int size)
{
	if (xQueuePeek(xQueueRain, (void *)&rain, 0) == pdFAIL)
	{
		return;
	}

	bool raining_now = false;
	for (int i = 0; i < size; i++)
	{
		if (strstr(rain.c_str(), (raining[i]).c_str()) != nullptr)
		{
			raining_now = true;
			break;
		}
	}
	digitalWrite(LED_RAIN_NOW, raining_now);
}

void onlineTaskPrint(const bool conf_active, dataOnline_t &my_data,
					 bool &validated_state_now, bool &validated_state_fcast,
					 bool &read_now, String &_id)
{
	BaseType_t rc = pdFALSE;

	if (!(conf_active == false)) // if there IS setting flag set
	{
		return;
	} // should have used simple queue but
	(read_now) ? rc = xQueuePeek(xQueueOn_NOW, (void *)&my_data, 0)
			   : rc = xQueuePeek(xQueueOn_FCAST, (void *)&my_data, 0);

	if (rc == pdPASS and (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS))
	{
		clear_lcd();

		// update state
		if (read_now == true)
		{
			// now
			bool now_int =
				scroll_text("                 weather NOW    ", LCD_SPD, 0);
			(now_int == true) ? validated_state_now = false
							  : validated_state_now = true;

			_id = "n";
			// printf("        rcvd now, f like %f\n", my_data.feels_like);
		}
		else
		{
			// fcast
			bool fcast_int =
				scroll_text("                FORECAST weather", LCD_SPD, 0);
			(fcast_int == true) ? validated_state_fcast = false
								: validated_state_fcast = true;

			_id = "f";
			// printf("        rcvd fcast, f like %f\n", my_data.feels_like);
		}

		clear_lcd();

		if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
		{
			if (((int)(round(my_data.temp))) < -9 ||
				((int)(round(my_data.tempMin))) < -9 ||
				((int)(round(my_data.tempMax))) < -9)
			{
				// critial printig stuff
				lcd.setCursor(0, 0);
				lcd.print("T:");
				lcd.setCursor(2, 0);
				lcd.print((int)(round(my_data.temp)));
				lcd.setCursor(5, 0);
				lcd.print(",");

				lcd.setCursor(6, 0);
				lcd.print((int)(round(my_data.tempMin)));
				lcd.setCursor(9, 0);
				lcd.write(2);
				lcd.setCursor(10, 0);
				lcd.print(",");

				lcd.setCursor(11, 0);
				lcd.print((int)(round(my_data.tempMax)));
				lcd.setCursor(14, 0);
				lcd.write(1);
				lcd.setCursor(15, 0);
				lcd.print(_id);
			}
			else
			{ // general stuff from -9 to 99 °, above 99 lol I don't care,
				// below -99, I don't care either lol
				lcd.setCursor(0, 0);
				lcd.print("TEMP:");
				lcd.setCursor(5, 0);
				lcd.print((int)(round(my_data.temp)));
				lcd.setCursor(7, 0);
				lcd.print(",");

				lcd.setCursor(8, 0);
				lcd.print((int)(round(my_data.tempMin)));
				lcd.setCursor(10, 0);
				lcd.write(2);
				lcd.setCursor(11, 0);
				lcd.print(",");

				lcd.setCursor(12, 0);
				lcd.print((int)(round(my_data.tempMax)));
				lcd.setCursor(14, 0);
				lcd.write(1);
				lcd.setCursor(15, 0);
				lcd.print(_id);
			}

			xSemaphoreGive(xI2C);
		}

		// others
		String text2 = "                Feels like: " + (String)my_data.feels_like +
					   (String)((char)223) +
					   ", Humidity: " + (String)((int)my_data.humidity) +
					   "%, Pressure: " + (String)((int)my_data.pressure) +
					   "hPa, Wind Speed: " + (String)my_data.windSpeed +
					   "m/s, Cloud coverage: " + (String)((int)my_data.clouds) +
					   "% " + (String)(my_data.description);

		bool interrupted = scroll_text(text2, LCD_SPD, 1);
		if (_id == "n" and interrupted == true)
		{
			validated_state_now = false;
		}
		else if (_id == "f" and interrupted == true)
		{
			validated_state_fcast = false;
		}

		xSemaphoreGive(xLCDAccess);
	}

	if ((_id == "n") and (validated_state_now == true))
	{
		rc = xTaskNotify(xTaskDaemon, (1 << SYSNOTIF_NOW), eSetBits);
		assert(rc == pdPASS);
	}
	else if ((_id == "f") and (validated_state_fcast == true))
	{
		rc = xTaskNotify(xTaskDaemon, (1 << SYSNOTIF_FCAST), eSetBits);
		assert(rc == pdPASS);
	}

	read_now = !read_now;
}

void offlineTaskPrint(const bool conf_active, dataOffline_t &my_data,
					  bool &validated_state)
{
	BaseType_t rc = pdFALSE;

	if (!(conf_active == false)) // if there IS setting flag set
	{
		return;
	}

	rc = xQueuePeek(xQueueOffline, (void *)&my_data, 0);
	if (rc == pdPASS and (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS))
	{
		clear_lcd();

		bool interrupted_ =
			scroll_text("                Indoors weather", LCD_SPD, 0);
		(interrupted_ == true) ? validated_state = false : validated_state = true;

		clear_lcd();

		// print static thing
		if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
		{
			lcd.setCursor(0, 0);
			lcd.print("TEMP:");
			lcd.setCursor(6, 0);
			lcd.print("     ");
			lcd.setCursor(6, 0);
			lcd.print(my_data.temp);
			lcd.setCursor(11, 0);
			lcd.write(byte(0));
			lcd.setCursor(12, 0);
			lcd.print("C");

			xSemaphoreGive(xI2C);
		}

		String co2_text = (my_data.co2 == 0xffff) ? "..." : (String)my_data.co2;
		String text2 = "                Humidity: " + (String)my_data.humi +
					   "%, Co2: " + co2_text +
					   "ppm, Altitude: " + (String)my_data.altitude +
					   "m, Pressure: " + (String)my_data.pressure + "hPa";

		bool interrupted = scroll_text(text2, LCD_SPD, 1);
		(interrupted == true) ? validated_state = false : validated_state = true;

		xSemaphoreGive(xLCDAccess);
	}

	if (validated_state == true)
	{
		rc = xTaskNotify(xTaskDaemon, (1 << SYSNOTIF_IN), eSetBits);
		assert(rc == pdPASS);
	}
}

bool scdSetup()
{
	uint16_t error;
	char errorMessage[256];
	scd40.begin(Wire);
	const char *pcTaskName = "scdSetup";

	// stop potentially previously started measurement
	error = scd40.stopPeriodicMeasurement();
	if (error)
	{
		if (debug_msk & (1 << debug_sensors))
		{
			errorToString(error, errorMessage, 256);
			id_n_talk(pcTaskName,
					  "Can't stopPeriodicMeasurement(): " + (String)errorMessage);
		}

		return true;
	}

	// Start Measurement
	error = scd40.startPeriodicMeasurement();
	if (error)
	{
		if (debug_msk & (1 << debug_sensors))
		{
			errorToString(error, errorMessage, 256);
			id_n_talk(pcTaskName, "Unable to startPeriodicMeasurement(): " +
									  (String)errorMessage);
		}

		return true;
	}

	// will call scd_cal_daemon() when done
	scd_cal_one_x_timer =
		xTimerCreate("scd_calibration_timer", pdMS_TO_TICKS(SCD_CAL_TMS), pdFALSE,
					 (void *)0, scd_cal_daemon);
	xEventGroupClearBits(handler_event,
						 (1 << SCD_CAL_DONE)); // set the calibration bit to 0

	// start the timer now
	if (scd_cal_one_x_timer == NULL)
	{
		if (debug_msk & (1 << debug_sensors))
		{
			id_n_talk(pcTaskName, "Error creating timer, rebooting");
		}
		return true;
	}
	else
	{
		xTimerStart(scd_cal_one_x_timer, portMAX_DELAY);
		if (debug_msk & (1 << debug_sensors))
		{
			id_n_talk(pcTaskName, "Timer spawned");
		}
	}

	return false;
}

bool bmeSetup(dataOffline_t &data)
{
	const char *pcTaskName = "bmeSetup";

	// BME CONFIG weather mode
	if (!bme.begin(bme_addr, &Wire))
	{
		if (debug_msk & (1 << debug_sensors))
		{
			id_n_talk(pcTaskName, "Error reading from bme sensor, rebooting");
		}
		return true;
	}

	// humi of bme
	data.temp = bme.readTemperature() - temp_offset;
	data.humi = bme.readHumidity();
	data.pressure = bme.readPressure() / 100.0F;
	data.altitude = bme.readAltitude(SEALEVELPRESSURE_HPA_2);

	// co2
	data.co2 = 0xffff;

	return false;
}

void lcdSetup()
{
	// LCD
	lcd.init();
	lcd.backlight();
	lcd.createChar(0, degre);
	lcd.createChar(1, up);
	lcd.createChar(2, down);
}

// OTHERS
void id_n_talk(const String &id, const String &word_)
{
	if (xSemaphoreTake(xSerial, portMAX_DELAY) == pdPASS)
	{
		String message = "[" + id + "]: " + word_ + ".\n";
		printf("%s", message.c_str());
		xSemaphoreGive(xSerial);
	}
}

bool scroll_text(const String text, const int delay, const int line)
{
	// scroll the text
	bool conf_active = false;
	EventBits_t event_result = 0;
	TickType_t xLastWakeTime = 0;

	event_result = xEventGroupWaitBits(
		handler_event, ((1 << CONF_ACTIVE_BIT_SCR)), pdFALSE, pdFALSE, 0);
	(event_result & (1 << CONF_ACTIVE_BIT_SCR)) ? conf_active = true
												: conf_active = false;

	if (conf_active == true)
	{
		return true;
	}

	for (int i = 0; i <= (text.length() - 16); i++)
	{
		xLastWakeTime = xTaskGetTickCount();
		if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
		{
			lcd.setCursor(0, line);
			for (int j = i; j <= i + 15; j++)
			{
				lcd.print(text[j]);
			}
			lcd.print(" ");

			xSemaphoreGive(xI2C);
		}

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(delay));
		event_result = xEventGroupWaitBits(
			handler_event, ((1 << CONF_ACTIVE_BIT_SCR)), pdFALSE, pdFALSE, 0);
		(event_result & (1 << CONF_ACTIVE_BIT_SCR)) ? conf_active = true
													: conf_active = false;

		if (conf_active == true)
		{
			return true;
		}
	}

	vTaskDelay(pdMS_TO_TICKS(500)); // give it 500ms before clearing the text

	return false;
}

// SERVER and influx
void update_onl_values(on_Data_t &struct_, const dataOnline_t &data)
{
	struct_.temp = data.temp;
	struct_.feels_like = data.feels_like;
	struct_.windSpeed = data.windSpeed;
	struct_.pressure = data.pressure;
	struct_.humidity = data.humidity;
	struct_.clouds = data.clouds;
	struct_.description = data.description;
	struct_.city = data.city;
}

void update_off_values(dataOffline_t &struct_2, const dataOffline_t &off_data)
{
	struct_2.temp = off_data.temp;
	struct_2.humi = off_data.humi;
	struct_2.humi_bme = off_data.humi_bme;
	struct_2.altitude = off_data.altitude;
	struct_2.pressure = off_data.pressure;
	struct_2.co2 = off_data.co2;
	struct_2.temp2 = off_data.temp2;
}

void server_init_stuff(on_Data_t &on_now, on_Data_t &on_fcast,
					   dataOffline_t &data_offline, dataOnline_t &data_online,
					   int &STATE)
{
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
			  { request->send(SPIFFS, "/index.htm", "text/html"); });
	server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
			  { request->send(SPIFFS, "/script.js", "text/javascript"); });
	server.on("/jquery-3.6.0.min.js", HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  request->send(SPIFFS, "/jquery-3.6.0.min.js", "text/javascript");
			  });
	server.on("/w3.css", HTTP_GET, [](AsyncWebServerRequest *request)
			  { request->send(SPIFFS, "/w3.css", "text/css"); });
	server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
			  { request->send(SPIFFS, "/favicon.png", "image/png"); });

	// dyn settings values
	server.on("/lcd_scrl_spd", HTTP_POST, [&](AsyncWebServerRequest *request)
			  {
    if (request->hasParam("value_lcd_scrl_spd", true)) {
      String msg = request->getParam("value_lcd_scrl_spd", true)->value();
      LCD_SPD = msg.toInt();
    }
    request->send(204); });
	server.on("/running_mode", HTTP_POST, [&](AsyncWebServerRequest *request)
			  {
    if (request->hasParam("value_running_mode", true)) {
      String msg = request->getParam("value_running_mode", true)->value();
      STATE = msg.toInt();
    }
    request->send(204); });
	server.on("/d_slp_duration", HTTP_POST, [&](AsyncWebServerRequest *request)
			  {
    if (request->hasParam("value_d_slp_duration", true)) {
      String msg = request->getParam("value_d_slp_duration", true)->value();
      SLEEP_DURATION_M = msg.toInt();
    }
    request->send(204); });

	// update server here? yee
	// web server stuff
	server.on("/wk_times", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)boot_times); });

	server.on("/intemperature", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)data_offline.temp); });
	server.on("/inhumidity", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)data_offline.humi_bme); });
	server.on("/inco2", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)data_offline.co2); });
	server.on("/inpressure", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)data_offline.pressure); });
	server.on("/inaltitude", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)data_offline.altitude); });

	// TODO: remove after
	server.on("/intemperature2", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)data_offline.temp2); });

	// 				O U	T
	server.on("/outtemperature_now", HTTP_GET,
			  [&](AsyncWebServerRequest *request)
			  {
				  request->send(200, "text/plain", (String)on_now.temp);
			  });
	server.on("/outhumidity_now", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)on_now.humidity); });
	server.on("/outpressure_now", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)on_now.pressure); });
	server.on("/outclouds_now", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)on_now.clouds); });
	server.on("/outfeelslike_now", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)on_now.feels_like); });
	server.on("/outdescription_now", HTTP_GET,
			  [&](AsyncWebServerRequest *request)
			  {
				  request->send(200, "text/plain", (String)on_now.description);
			  });
	server.on("/outwindspeed_now", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)on_now.windSpeed); });

	server.on("/outtemperature_fcast", HTTP_GET,
			  [&](AsyncWebServerRequest *request)
			  {
				  request->send(200, "text/plain", (String)on_fcast.temp);
			  });
	server.on("/outhumidity_fcast", HTTP_GET,
			  [&](AsyncWebServerRequest *request)
			  {
				  request->send(200, "text/plain", (String)on_fcast.humidity);
			  });
	server.on("/outpressure_fcast", HTTP_GET,
			  [&](AsyncWebServerRequest *request)
			  {
				  request->send(200, "text/plain", (String)on_fcast.pressure);
			  });
	server.on("/outclouds_fcast", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)on_fcast.clouds); });
	server.on("/outfeelslike_fcast", HTTP_GET,
			  [&](AsyncWebServerRequest *request)
			  {
				  request->send(200, "text/plain", (String)on_fcast.feels_like);
			  });
	server.on("/outdescription_fcast", HTTP_GET,
			  [&](AsyncWebServerRequest *request)
			  {
				  request->send(200, "text/plain", (String)on_fcast.description);
			  });
	server.on("/outwindspeed_fcast", HTTP_GET,
			  [&](AsyncWebServerRequest *request)
			  {
				  request->send(200, "text/plain", (String)on_fcast.windSpeed);
			  });

	server.on("/outcity", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)on_now.city); });
	server.on("/last_ds", HTTP_GET, [&](AsyncWebServerRequest *request)
			  { request->send(200, "text/plain", (String)formated_time); });
}

void getServer_State(bool &state, const char *pcTaskName)
{
	// check server state
	if (db_client.validateConnection() == true)
	{
		state = true; // server is up
	}
	else
	{
		state = false; // server is down
	}
	xQueueOverwrite(xInfluxDB_state, &state);

	if (debug_msk & (1 << debug_dbserver))
	{
		if (state)
		{
			id_n_talk(pcTaskName,
					  "Connected to InfluxDB: " + db_client.getServerUrl());
		}
		else
		{
			id_n_talk(pcTaskName, "InfluxDB connection failed: " +
									  db_client.getLastErrorMessage());
		}
	}
}

bool writePointToServer(Point &sensor, dataOffline_t data,
						const char *pcTaskName, int &failed_try)
{
	// returns true if ok
	// Clear fields for reusing the point. Tags will remain the same as set above.
	sensor.clearFields(); // time cleared too

	// Publish them here
	sensor.addField("co2", data.co2);
	sensor.addField("temp", data.temp);
	sensor.addField("humi", data.humi);
	sensor.addField("Pres", data.humi);
	sensor.addField("Alt", data.humi);
	sensor.addField("humi", data.humi);

	if (db_client.writePoint(sensor) == false)
	{
		if (debug_msk & (1 << debug_dbserver))
		{
			id_n_talk(pcTaskName,
					  "InfluxDB write failed: " + db_client.getLastErrorMessage());
		}
		failed_try += 1;
		return false;
	}

	if (debug_msk & (1 << debug_dbserver))
	{
		id_n_talk(pcTaskName, "Wrote: " + sensor.toLineProtocol());
	}

	failed_try = 0;
	return true;
}

void updateServerValues(TickType_t xTick_offline, dataOffline_t &my_data_off,
						dataOffline_t &data_offline, TickType_t xTick_online,
						dataOnline_t &my_data, on_Data_t &on_now,
						on_Data_t &on_fcast)
{
	if ((xTaskGetTickCount() - xTick_offline) > 10 * 1000)
	{
		// 10s for local temp
		xQueuePeek(xQueueOffline, (void *)&my_data_off, 0);
		update_off_values(data_offline, my_data_off);
		xTick_offline = xTaskGetTickCount();
	}

	if ((xTaskGetTickCount() - xTick_online) > 30 * 1000)
	{
		// 30 s for online data
		xQueuePeek(xQueueOn_NOW, (void *)&my_data, 0);
		update_onl_values(on_now, my_data);

		xQueuePeek(xQueueOn_FCAST, (void *)&my_data, 0);
		update_onl_values(on_fcast, my_data);
		xTick_online = xTaskGetTickCount();
	}
}

// SYS & CO
String getParam(String name)
{
	// read parameter from server, for customhmtl input
	String value;
	if (wm.server->hasArg(name))
	{
		value = wm.server->arg(name);
	}
	return value;
}

void reboot_it(int secon_)
{
	TickType_t ticktoco = xTaskGetTickCount();
	bool state_led_wl = true;

	while (xTaskGetTickCount() - ticktoco < 1000)
	{
		// flashes for x (above) seconds
		state_led_wl ^= true;
		digitalWrite(LED_COLD, state_led_wl);
		digitalWrite(LED_HOT, state_led_wl);
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	esp_sleep_enable_timer_wakeup(secon_ * US_2_S); // xs, to reco
	esp_deep_sleep_start();
}

void readSysConfig()
{
	// now, try to read the confi_file (api_key and location) to use it. If
	// there's no data well, launch the wifi Portal
	if (!(SPIFFS.exists(CONFIG_FILE)))
	{
		// file doesn't exist
		if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
		{
			if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
			{
				lcd.setCursor(14, 1);
				lcd.print("NO");
				xSemaphoreGive(xI2C);
			}
			xSemaphoreGive(xLCDAccess);
		}
		vTaskDelay(pdMS_TO_TICKS(1000));

		// launch the ondemand portal and wait for user to fill the form
		if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
		{
			clear_lcd();
			scroll_text("        API KEY and Location required. Opening config portal...", LCD_SPD, 0);
			scroll_text("        will timeout after 6mins, starting now.", LCD_SPD,
						1);
			xSemaphoreGive(xLCDAccess);
		}
		scroll_text("        Hit Restart at the end.", LCD_SPD, 1);

		wm.setConfigPortalTimeout(PORTAL_TMT_MS);
		bool res = wm.startConfigPortal(AP_NAME, AP_PASWD);

		if (!res)
		{
			if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
			{
				clear_lcd();
				scroll_text("        failed to connect or timed out", LCD_SPD, 0);
				xSemaphoreGive(xLCDAccess);
			}
			// then what next? restart? yes, after the timeout so it's long, but user
			// can hit restart right away
		}
		else
		{
			clear_lcd();
			scroll_text("        Restarting now...", LCD_SPD, 1);
		}

		return;
	}

	// file exist, read it
	auto config = SPIFFS.open(CONFIG_FILE, "r");
	if (!config and (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS))
	{
		// exist, but unable to read, reboot
		if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
		{
			lcd.setCursor(14, 1);
			lcd.print("NO");
			xSemaphoreGive(xI2C);
		}
		xSemaphoreGive(xLCDAccess);

		vTaskDelay(pdMS_TO_TICKS(1000));
		reboot_it(5);
	}

	StaticJsonDocument<1024> u_data;
	DeserializationError error = deserializeJson(u_data, config);

	if (error and (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS))
	{
		id_n_talk("setup", error.c_str());
		// unable to deserialize
		if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
		{
			lcd.setCursor(14, 1);
			lcd.print("NO");
			xSemaphoreGive(xI2C);
		}
		xSemaphoreGive(xLCDAccess);

		vTaskDelay(pdMS_TO_TICKS(1000));
		reboot_it(5);
	}

	const char *tmp_key = u_data["api_key"];
	const char *tmp_loc = u_data["location"];
	const char *tmp_gmt_off = u_data["gmt_offset"];
	const char *tmp_dyl_off = u_data["dyl_offset"];
	const char *tmp_tz = u_data["timezone"];
	const char *tmp_ntp_s = u_data["ntp_s"];
	const char *tmp_ntp_s2 = u_data["ntp_s2"];
	const char *tmp_influx_org = u_data["influx_org"];
	const char *tmp_influx_token = u_data["influx_token"];
	const char *tmp_influx_bucket = u_data["influx_bucket"];
	const char *tmp_influx_url = u_data["influx_url"];

	config.close();

	// put this to variable
	user_api_key = (String)tmp_key;
	user_location_coordinate = (String)tmp_loc;
	user_gmt_offset = atol(tmp_gmt_off);
	user_daylight_offset = atol(tmp_dyl_off);
	user_timezone = (String)tmp_tz;
	user_ntp_server = (String)tmp_ntp_s;
	user_ntp_server2 = (String)tmp_ntp_s2;
	user_influx_org = (String)tmp_influx_org;
	user_influx_token = (String)tmp_influx_token;
	user_influx_url = (String)tmp_influx_url;
	user_influx_bucket = (String)tmp_influx_bucket;

	if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
	{
		if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
		{
			lcd.setCursor(14, 1);
			lcd.print("OK");
			xSemaphoreGive(xI2C);
		}
		xSemaphoreGive(xLCDAccess);
	}
	vTaskDelay(pdMS_TO_TICKS(1000));
}

bool createTimer(const long duration, TimerCallbackFunction_t(call_func_ptr),
				 const char *pcTaskName, const char *timerName)
{
	int ret = pdFAIL;
	// create the timer for the timeout
	one_x_timer_releaser = xTimerCreate(timerName, pdMS_TO_TICKS(duration),
										pdFALSE, (void *)0, call_func_ptr);
	if (one_x_timer_releaser == NULL)
	{
		ret = pdFAIL;
	}
	else
	{
		ret = pdPASS;
		xTimerStart(one_x_timer_releaser, portMAX_DELAY);
	}

	if ((debug_msk & (1 << debug_setup)) and ret == pdFAIL)
	{
		id_n_talk(pcTaskName, "Error creating timer");
	}

	return ret;
}

void resolveInputs(menu_t &menu_n, const int i, int &data_nexter,
				   long &current_param, bool &clk_24_mode, bool &chg_wlan,
				   bool &clk_mode, bool &valid_clk, bool &ghost_timer,
				   bool &tmr_flag, btn_state_t &STATES_T, const int config_T,
				   const char *pcTaskName, int &config_msk,
				   TickType_t &xLastWakeTime_c)
{
	if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdFAIL)
	{
		return;
	}

	// mutex acquired
	vTaskSuspend(xTaskOffline);
	vTaskSuspend(xTaskOnline); // suspend those who have access to the lcd mainly
							   // (the led with the clock is already cleared, we
							   // don't want to stop this task)
	vTaskSuspend(xTaskDaemon); // prevent from going to sleep mode here

	// make sure the backlit is on
	lcd.backlight();

	ghost_timer = 0; // did not started a new timer

	handleMenuLogic(menu_n, i, data_nexter, current_param, clk_24_mode, chg_wlan,
					clk_mode, valid_clk, ghost_timer, tmr_flag);
	STATES_T.flag[i] = 0;

	// printf("i2c menu req: %d\n", xTaskGetTickCount());
	// display the menu here
	dispMenu(menu_n, clk_24_mode, chg_wlan, clk_mode);

	// release the mutex for next task which will wake up
	broadcastConfMode(true);
	xSemaphoreGive(xLCDAccess);

	// create the timer for the timeout
	EventBits_t event_result =
		xEventGroupWaitBits(handler_event, ((1 << TIMER_FLAG_BIT)), pdFALSE,
							pdFALSE, 0); /*event await timer_flag*/
	(event_result & (1 << TIMER_FLAG_BIT)) ? tmr_flag = true : tmr_flag = false;

	launchMenuTimout(tmr_flag, ghost_timer, config_T, pcTaskName, config_msk,
					 xLastWakeTime_c);
}

void dispMenu(menu_t &menu_n, bool &clk_24_mode, bool &chg_wlan,
			  bool &clk_mode)
{
	if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdFAIL)
	{
		return;
	}

	// printf("i2c menu granted: %d\n", xTaskGetTickCount());
	lcd.setCursor(0, 0);
	lcd.print("                ");
	lcd.setCursor(0, 1);
	lcd.print("                ");

	lcd.setCursor(0, 0);
	lcd.print(menu_n.menu[menu_n.current_menu][0]);

	lcd.setCursor(4, 1); // center a little bit

	// these values has been updated by the exec buttton
	switch (menu_n.current_menu)
	{
	case 1: // lcd speed
		lcd.print(LCD_SPD);
		break;
	case 2: // sleep duration
		lcd.print(IDLE_TMS);
		break;
	case 3: // deep sleep duration
		lcd.print(SLEEP_DURATION_M);
		break;
	case 4: // menu-config 24h mode
		if (clk_24_mode == 1)
		{
			lcd.print("24H mode");
		}
		else if (clk_24_mode == 0)
		{
			lcd.print("12H mode");
		}

		break;
	case 5: // menu-config change wlan
		if (chg_wlan == 1)
		{
			lcd.print("Yes");
		}
		else if (chg_wlan == 0)
		{
			lcd.print("No");
		}

		break;

	case 0: // menu-config clock mode
		if (clk_mode == 1)
		{
			lcd.print("ON");
		}
		else if (clk_mode == 0)
		{
			lcd.print("OFF");
		}

		break;
	default:
		break;
	}
	lcd.setCursor(10, 1); // center a little bit
	lcd.print(menu_n.menu[menu_n.current_menu][1]);

	xSemaphoreGive(xI2C);
}

void launchMenuTimout(const bool tmr_flag, const bool ghost_timer,
					  const int config_T, const char *pcTaskName,
					  int &config_msk, TickType_t &xLastWakeTime_c)
{
	if (!(tmr_flag == false && ghost_timer == 0))
	{
		return;
	}

	// enter this to create the timer only once, not multiple times # countdown
	// timer
	if (createTimer(config_T * 1000, releaser_daemon, pcTaskName,
					"one-x-timer") == pdPASS)
	{
		// prevent from launching multiple timers by settin it to 1, so next time
		// the condition check will fail
		xEventGroupSetBits(handler_event, ((1 << TIMER_FLAG_BIT)));
		config_msk = config_T;
		xLastWakeTime_c = xTaskGetTickCount();

		if (debug_msk & (1 << debug_setup))
		{
			id_n_talk("vSetup", "timeout for " + (String)config_T + " started");
		}
	}
}

void broadcastConfMode(bool clear /*  = false */)
{
	if (clear == false)
	{
		// request all tasks to stop in order to enter the config mode
		xEventGroupSetBits(handler_event,
						   ((1 << CONF_ACTIVE_BIT_ON) | (1 << CONF_ACTIVE_BIT_LED) |
							(1 << CONF_ACTIVE_BIT_SCR) |
							(1 << CONF_ACTIVE_BIT_DAE) |
							(1 << CONF_ACTIVE_BIT_OFF)));
	}
	else
	{
		// remove the config mode
		xEventGroupClearBits(
			handler_event,
			((1 << CONF_ACTIVE_BIT_ON) | (1 << CONF_ACTIVE_BIT_LED) |
			 (1 << CONF_ACTIVE_BIT_SCR) | (1 << CONF_ACTIVE_BIT_DAE) |
			 (1 << CONF_ACTIVE_BIT_OFF)));
	}
}

void clockOrStaMode()
{
	bool bl_on = false;
	// dynamically change the exec mode : station vs clock (from web server) 0
	// sta, 1 : clk
	switch (CLOCK_PENDING)
	{
	case 1:
	{
		if (debug_msk & (1 << debug_setup))
		{
			id_n_talk("vSetup", "got clock mode");
		}
		// clock mode
		// cannot just suspend them, lol let them release the mutex before proceding
		broadcastConfMode();

		if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
		{
			clear_lcd();
			vTaskSuspend(xTaskOffline);
			vTaskSuspend(xTaskOnline);
			vTaskSuspend(xTaskDaemon);

			broadcastConfMode(true); // clear the config mode
			xEventGroupSetBits(
				handler_event,
				((1 << V_CLK_STATE_BIT_SET) |
				 (1 << V_CLK_STATE_BIT_LED))); // validate clock mode now

			xSemaphoreGive(xLCDAccess);
		}

		// to prevent this from running all the way everytime, putting -1 won't
		// allow the if execution again
		//  user will need to revalidate the state
		CLOCK_PENDING = -1;
	}

	break;

	case 0:
	{
		if (debug_msk & (1 << debug_setup))
		{
			id_n_talk("vSetup", "got weather mode");
		}
		// weather mode, resume the others
		xEventGroupClearBits(handler_event, ((1 << V_CLK_STATE_BIT_SET) |
											 (1 << V_CLK_STATE_BIT_LED)));
		/* TaskStatus_t xTaskDetails;

		vTaskGetInfo(xTaskDaemon, &xTaskDetails, pdFALSE, eInvalid);
		if (xTaskDetails.eCurrentState == eSuspended)
		{
			vTaskResume(xTaskDaemon);
		} */

		// make sure lcd is on
		bl_on = true;

		vTaskResume(xTaskDaemon);
		vTaskResume(xTaskOnline);
		vTaskResume(xTaskOffline);

		CLOCK_PENDING = -2; // -2, neither sta nor clock so don't care + didn't ask
	}
	break;

	case 2:
	{
		bl_on = true;
		if (debug_msk & (1 << debug_setup))
		{
			id_n_talk("vSetup", "WM portal mode");
		}
		xEventGroupSetBits(handler_event, (1 << V_START_PORTAL_BIT_SET));
		CLOCK_PENDING = -2;
	}
	break;

	default:
		break;
	}

	if (bl_on == false)
	{
		return;
	}
	if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
	{
		if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
		{
			lcd.backlight();

			xSemaphoreGive(xI2C);
		}
		xSemaphoreGive(xLCDAccess);
	}
}

void checkCExecPortal(const bool valid_portal)
{
	if (valid_portal == false)
	{
		return;
	}

	vTaskSuspend(xTaskOffline);
	vTaskSuspend(xTaskOnline);
	vTaskSuspend(xTaskDaemon);

	server.end();
	// we need LCD permission
	// todo: duplication here
	if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
	{
		clear_lcd();
		scroll_text("        Launching config portal...", LCD_SPD, 0);
		scroll_text("        Hit Restart at the end.", LCD_SPD, 1);

		// first notify wifi disconnected and stop the server
		xEventGroupClearBits(handler_event,
							 ((1 << WLAN_CONTD_BIT))); // stop sending http requests

		wm.setConfigPortalTimeout(PORTAL_TMT_MS);
		bool res = wm.startConfigPortal(AP_NAME, AP_PASWD);

		if (!res)
		{
			clear_lcd();
			scroll_text("        failed to connect or timed out", LCD_SPD, 0);
			vTaskResume(xTaskOffline);
			vTaskResume(xTaskOnline);
			vTaskResume(xTaskDaemon);
		}
		xEventGroupClearBits(handler_event, (1 << V_START_PORTAL_BIT_SET));
		xSemaphoreGive(xLCDAccess);
	}
}

void wifiCoIssues(TickType_t &xTick_wlan_state, const bool valid_clk,
				  bool &reboot_reconfd, TickType_t &xtick_unknown, int &cnt,
				  TickType_t &xLastLedOn, bool &flicker_led,
				  bool &last_flick_state)
{
	bool go_reboot = false;

	// 60s, check wifi state
	if (((xTaskGetTickCount() - xTick_wlan_state) >= 1 * 60 * 1000) and
		(WiFi.status() != WL_CONNECTED))
	{
		xTick_wlan_state = xTaskGetTickCount();

		// notify this event
		xEventGroupClearBits(handler_event, ((1 << WLAN_CONTD_BIT)));
		// id_n_talk(pcTaskName, "Troubles wifi: " + (String)WiFi.status());

		if (WiFi.status() == WL_DISCONNECTED)
		{
			WiFi.reconnect();
		}

		if (valid_clk == true)
		{
			cnt += 1; // start counting if in clock mode

			xEventGroupSetBits(handler_event, ((1 << LED_BUSY)));
			flicker_led = true;

			digitalWrite(LED_COLD, HIGH);
			digitalWrite(LED_HOT, HIGH);
			last_flick_state = HIGH;

			if (debug_msk & (1 << debug_setup))
			{
				id_n_talk("vTaskSetup", "took ctrl of led, starting flickering");
			}

			// signal that led is now being used to indicat connection issues
			xLastLedOn = xTaskGetTickCount();
			xtick_unknown = xTaskGetTickCount();

			if ((cnt > (10 / (int)(LED_MS * 2 / 1000))) && reboot_reconfd == false)
			{
				// 10 mins (every minute, 10x) count greater than 10 mins?
				go_reboot = true;
			}
		}
	}
	else
	{
		if (WiFi.status() == WL_CONNECTED)
		{
			cnt = 0; // if wifi gets co again, clear the counting value
			xEventGroupSetBits(handler_event, ((1 << WLAN_CONTD_BIT)));
			xEventGroupClearBits(handler_event,
								 ((1 << LED_BUSY))); // also clear this when coed
		}
	}

	if (flicker_led == true)
	{
		// apply flickering here :: turn on and off at a speed=100ms
		if ((xTaskGetTickCount() - xLastLedOn) >= 100)
		{
			xLastLedOn = xTaskGetTickCount();

			last_flick_state = (last_flick_state == HIGH) ? LOW : HIGH;

			digitalWrite(LED_COLD, last_flick_state);
			digitalWrite(LED_HOT, last_flick_state);
		}
	}

	if (go_reboot == true)
	{
		go_reboot = false; // doesn't matter since esp will restart

		RebootOnClock = true;
		esp_sleep_enable_timer_wakeup(10 * US_2_S);
		// these will assure deep sleep and wake up to clock mode again after 10 s
		reboot_reconfd = true;

		if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
		{
			if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
			{
				lcd.noDisplay();
				lcd.noBacklight();
				xSemaphoreGive(xI2C);
			}

			xSemaphoreGive(xLCDAccess);
		}
		Serial.flush();
		esp_deep_sleep_start();
	}

	// release led lock. Locked for 5000 each time
	if ((xTaskGetTickCount() - xtick_unknown) >= 5000 && flicker_led == true)
	{
		// after 5000 ms, clear the busy state of the leds
		digitalWrite(LED_COLD, LOW);
		digitalWrite(LED_HOT, LOW);

		xtick_unknown = xTaskGetTickCount();
		xEventGroupClearBits(handler_event, ((1 << LED_BUSY)));

		flicker_led = false; // end flick

		if (debug_msk & (1 << debug_setup))
		{
			id_n_talk("vTaskSetup", "released led, stoping flickering");
		}
	}
}

void pirLogic(bool &pir_state, bool &last_pir, bl_mgmt_t &that_mgmt, TickType_t &last_detected_pir,
			  const bool valid_clk)
{
	if (valid_clk == false)
	{
		return;
	}

	// pir needs about 5s to re-arm the system, how to counter this?
	(digitalRead(PIR_BL)) ? pir_state = HIGH : pir_state = LOW;
	if ((pir_state != last_pir) && (pir_state == true) &&
		(xSemaphoreTake(xLCDAccess, 200) == pdPASS))
	{
		// this helps counter the fact of writing true true or false false
		// repeatedly also, only write true, the false will be taken care
		if (xSemaphoreTake(xI2C, 200) == pdPASS)
		{
			lcd.backlight();
			xSemaphoreGive(xI2C);

			that_mgmt.turned_on = true;
			that_mgmt.extended_T = 0;
		}
		xSemaphoreGive(xLCDAccess);

		// keep it lit until the end of (detection + 5s) time
	}
	last_pir = pir_state;

	if (last_pir == true)
	{
		last_detected_pir = xTaskGetTickCount(); // record last time it stopped
		return;
	}

	// aka stopped detecting, it need 5s to restart reading new state
	// turn on the lit and keep it for 5s
	if (((xTaskGetTickCount() - last_detected_pir) > PIR_COUNTER_DELAY) and
		xSemaphoreTake(xLCDAccess, 200) == pdPASS) // added 1 s to it
	{
		if (that_mgmt.turned_on == true and xSemaphoreTake(xI2C, 200) == pdPASS)
		{
			if (that_mgmt.extended_bl == false or that_mgmt.extended_T > pdMS_TO_TICKS(that_mgmt.up_to_T))
			{
				lcd.noBacklight();

				that_mgmt.extended_T = 0;
				that_mgmt.turned_on = false;
			}
			that_mgmt.extended_T++;
			// printf("xtending %ux\n", that_mgmt.extended_T);
			xSemaphoreGive(xI2C);
		}
		xSemaphoreGive(xLCDAccess);
	}
}

void idleSTAMode(bool conf_active, bool &hard_resume, int &rep,
				 bool &check_go_idle_now)
{
	if (conf_active == true)
	{
		// config is active
		hard_resume = true; // give it a chance to comeback again and go to idle
							// mode and then unclock the whole sys again
		// since the SYS_STATE_IDLE is active, offline and online shutdown
		// themselves lol waiting for the savor to occurs lol.
		// xSemaphoreGive(xLCDAccess); // give it to config thing, and it will block
		// you lol
		return;
	}

	if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdFAIL)
	{
		return;
	}

	// if the thing is not active, then go ahead, otherwise, leave what you are
	// doing and give the mutex. also turn off the lcd disp
	if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));

		lcd.noDisplay();
		lcd.noBacklight();

		xSemaphoreGive(xI2C);
	}

	if (hard_resume == true)
	{
		// come here thanks to hard_resume, so don't inc the counter, do nothing
		hard_resume = false;
	}
	else
	{
		// only idle if state not active, if hard resume, no idle
		// only inc if you did not enter through hard_resume
		rep += 1; // and yea, only if you did yo thing nicely you can count it, not
				  // while config was waiting
		check_go_idle_now = true;
	}

	xSemaphoreGive(xLCDAccess);
}

void taskNotifMan(uint32_t rv, uint32_t &rv_msk, bool &idle)
{
	if (rv & (1 << SYSNOTIF_NOW))
	{
		rv_msk |= (1 << SYSNOTIF_NOW);
	}
	else if (rv & (1 << SYSNOTIF_FCAST))
	{
		rv_msk |= (1 << SYSNOTIF_FCAST);
	}
	else if (rv & (1 << SYSNOTIF_IN))
	{
		rv_msk |= (1 << SYSNOTIF_IN);
	}

	if (rv_msk ==
		((1 << SYSNOTIF_IN) | (1 << SYSNOTIF_FCAST) | (1 << SYSNOTIF_NOW)))
	{
		idle = true; // all cases met
		rv_msk &= ~((1 << SYSNOTIF_IN) | (1 << SYSNOTIF_FCAST) |
					(1 << SYSNOTIF_NOW)); // clear the state flag
	}
	else
	{
		idle = false;
	}
}

// CALLBACK
void saveParamCallback()
{
	// save user data into a json file in flash storage
	// id_n_talk("SaveW param", "saveParamCallback fired");
	String tmp_key = getParam("apikey");
	String tmp_loc = getParam("location");
	String tmp_tz = getParam("timezone");
	String tmp_gmt_off = getParam("gmt_offset");
	String tmp_dyl_off = getParam("dyl_offset");
	String tmp_ntp_s = getParam("ntp_s");
	String tmp_ntp_s2 = getParam("ntp_s2");
	String tmp_influx_org = getParam("influx_org");
	String tmp_influx_token = getParam("influx_token");
	String tmp_influx_url = getParam("influx_url");
	String tmp_influx_bucket = getParam("influx_bucket");

	// save them into the SPIFFS
	StaticJsonDocument<512> u_data;
	u_data["api_key"] = tmp_key;
	u_data["location"] = tmp_loc;
	u_data["gmt_offset"] = tmp_gmt_off;
	u_data["dyl_offset"] = tmp_dyl_off;
	u_data["timezone"] = tmp_tz;
	u_data["ntp_s"] = tmp_ntp_s;
	u_data["ntp_s2"] = tmp_ntp_s2;
	u_data["influx_org"] = tmp_influx_org;
	u_data["influx_token"] = tmp_influx_token;
	u_data["influx_bucket"] = tmp_influx_bucket;
	u_data["influx_url"] = tmp_influx_url;

	auto config = SPIFFS.open(CONFIG_FILE, "w");
	if (!config)
	{
		if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
		{
			clear_lcd();
			scroll_text("                Error opening config file. Rebooting ...",
						LCD_SPD, 0);

			xSemaphoreGive(xLCDAccess);
		}
		vTaskDelay(pdMS_TO_TICKS(2000));
		reboot_it(5);
	}
	// data saved as json file in the Flash storage
	serializeJson(u_data, config);
	config.close();

	if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
	{
		clear_lcd();
		scroll_text("                Config file saved.", LCD_SPD, 0);

		xSemaphoreGive(xLCDAccess);
	}
	vTaskDelay(pdMS_TO_TICKS(2000));
}

void callback_touch_wakeup() {}

void releaser_daemon(TimerHandle_t xTimer)
{
	bool clk_mode = false, chg_config = false;
	xEventGroupClearBits(handler_event,
						 ((1 << WAIT_CLK_BIT_LED) | (1 << TIMER_FLAG_BIT) |
						  (1 << BTN_WAIT_MODE_BIT)));

	xEventGroupSetBits(handler_event, ((1 << CLEAR_ONC_LED)));

	xQueueReceive(xClockMode, (void *)&clk_mode, 0);
	xQueueReceive(xPortalMode, (void *)&chg_config, 0);

	if (clk_mode == 1)
	{
		// aka clock mode
		// dont release anybody lol, tmr_flag cleared, nice okay
		xEventGroupSetBits(handler_event,
						   ((1 << V_CLK_STATE_BIT_SET) |
							(1 << V_CLK_STATE_BIT_LED))); // validate the mode now
	}
	else
	{
		xEventGroupClearBits(
			handler_event,
			((1 << V_CLK_STATE_BIT_SET) | (1 << V_CLK_STATE_BIT_LED) |
			 (1 << CONF_ACTIVE_BIT_ON) | (1 << CONF_ACTIVE_BIT_OFF)));
		xEventGroupClearBits(
			handler_event,
			((1 << CONF_ACTIVE_BIT_LED) | (1 << CONF_ACTIVE_BIT_SCR) |
			 (1 << CONF_ACTIVE_BIT_DAE) | (1 << CONF_ACTIVE_BIT_SET)));

		vTaskResume(xTaskDaemon);
		vTaskResume(xTaskOnline);
		vTaskResume(xTaskOffline);
	}

	if (chg_config == true)
	{
		xEventGroupSetBits(
			handler_event,
			((1 << V_START_PORTAL_BIT_SET))); // validate the portal mode now
											  // stop others ta , cause, we won't
											  // need them until next reboot
	}
	else
	{
		xEventGroupClearBits(handler_event, ((1 << V_START_PORTAL_BIT_SET)));
		if (clk_mode == 0)
		{
			// prevent this unchanged parameter (false by default) to start tasks that
			// should be stopped in clock mode
			vTaskResume(xTaskDaemon);
			vTaskResume(xTaskOnline);
			vTaskResume(xTaskOffline);
			// else, do not resume any
		}
	}
}

void timer_daemon(TimerHandle_t xTimer)
{
	// called when timer expires
	if (xSemaphoreTake(xLCDAccess, portMAX_DELAY) == pdPASS)
	{
		xEventGroupClearBits(handler_event, ((1 << SYSSTATE_IDLE_BIT_OFF) |
											 (1 << SYSSTATE_IDLE_BIT_ON) |
											 (1 << BTN_WAIT_MODE_BIT)));

		vTaskResume(xTaskOnline);
		vTaskResume(xTaskOffline);

		if (xSemaphoreTake(xI2C, portMAX_DELAY) == pdPASS)
		{
			lcd.clear();

			lcd.display();
			lcd.backlight();

			xSemaphoreGive(xI2C);
		}

		xSemaphoreGive(xLCDAccess);
	}
}

void scd_cal_daemon(TimerHandle_t xTimer)
{
	// called when timer expires
	xEventGroupSetBits(handler_event,
					   (1 << SCD_CAL_DONE)); // set the calibration bit to 1
	if (debug_msk & (1 << debug_sensors))
	{
		id_n_talk("timer cal scd", "timer expired, bit set");
	}
}
