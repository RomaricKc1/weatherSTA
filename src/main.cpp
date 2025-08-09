#include "commons.hpp"
#include "graphics.hpp"
#include "influx_db.hpp"
#include "init.hpp"
#include "sensors.hpp"
#include "weather_api.hpp"
#include "web_server.hpp"
#include "wm.hpp"

void setup() {
    N_Common::initRtc();
    N_WeatherAPI::initApiStr();

    N_Graphics::initLcd();
    N_InfluxDb::initInfluxStr();
    N_Sensors::initSensors(); // bme, scd40
    N_WebServer::initServer();

    N_WM::initWM();

    if (!N_Sensors::scd40 or !N_Sensors::bme or !N_WebServer::server or
        !N_Graphics::lcd) {
        assert("abort");
    }

    N_Init::setupBase();
}

void loop() {
    // put your main code here, to run repeatedly:
    // no lol
    vTaskDelete(nullptr);
}
