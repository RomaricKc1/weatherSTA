#ifndef INFLUX_DB_HPP
#define INFLUX_DB_HPP

#include "commons.hpp"

namespace N_InfluxDb {
    extern TaskHandle_t xTaskInfluxDb;

    extern std::unique_ptr<String> user_influx_token;
    extern std::unique_ptr<String> user_influx_org;

    extern std::unique_ptr<String> user_influx_url;
    extern std::unique_ptr<String> user_influx_bucket;

    // ////////////////////////////////////////////////////////////////////////
    void vTaskInfluxSender(void * /*pvParameters*/);

    void initInfluxStr();
    void getServerState(bool &state, const char *pcTaskName);
    auto writePointToServer(Point &sensor, S_DataOffline data,
                            const char *pcTaskName, int &failed_try) -> bool;

} // namespace N_InfluxDb

#endif // INFLUX_DB_HPP
