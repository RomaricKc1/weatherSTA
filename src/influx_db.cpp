#include "influx_db.hpp"

namespace N_InfluxDb {
    TaskHandle_t xTaskInfluxDb = nullptr;

    std::unique_ptr<String> user_influx_token;
    std::unique_ptr<String> user_influx_org;
    std::unique_ptr<String> user_influx_url;
    std::unique_ptr<String> user_influx_bucket;

    void initInfluxStr() {
        try {
            user_influx_token = std::make_unique<String>("");
            user_influx_org   = std::make_unique<String>("");
            user_influx_url   = std::make_unique<String>(INFLUXDB_URL.data());
            user_influx_bucket =
                std::make_unique<String>(INFLUXDB_BUCKET.data());

        } catch (const std::exception &e) {
            assert("abort");
        }
    }

    void vTaskInfluxSender(void * /*pvParameters*/) {
        const char *pcTaskName = "vTaskInfluxSender";
        S_DataOffline data;

        // influx db
        Point sensor("Co2_TH"); // data point
        sensor.addTag("device", DEVICE.data());
        sensor.addTag("SSID", WiFi.SSID());
        sensor.addTag("Where", "Main");

        // request sensors data and write to the server
        int failed_try{};

        xQueuePeek(N_Common::xQueueOffline, static_cast<void *>(&data), 0);
        writePointToServer(sensor, data, pcTaskName, failed_try);
        xQueueOverwrite(N_Common::xInfluxFailedAtt, &failed_try);

        taskYIELD();

        for (;;) {}
    }

    void getServerState(bool &state, const char *pcTaskName) {

        if (!(user_influx_org and user_influx_bucket and user_influx_url and
              user_influx_org)) {
            abort();
        }
        InfluxDBClient db_client{InfluxDBClient(
            *N_InfluxDb::user_influx_url, *N_InfluxDb::user_influx_org,
            *N_InfluxDb::user_influx_bucket, *N_InfluxDb::user_influx_token)};
        // check server state
        state = db_client.validateConnection();
        xQueueOverwrite(N_Common::xInfluxDB_state, &state);

        if ((N_Common::debug_msk & (1U << DEBUG_DB_SERVER)) != 0U) {
            if (state) {
                N_Helpers::idAndMsg({.id   = pcTaskName,
                                     .word = "Connected to InfluxDB: " +
                                             db_client.getServerUrl()});
            } else {
                N_Helpers::idAndMsg({.id   = pcTaskName,
                                     .word = "InfluxDB connection failed: " +
                                             db_client.getLastErrorMessage()});
            }
        }
    }

    auto writePointToServer(Point &sensor, S_DataOffline data,
                            const char *pcTaskName, int &failed_try) -> bool {
        // returns true if ok
        // Clear fields for reusing the point. Tags will remain the same as set
        // above.
        sensor.clearFields(); // time cleared too

        // Publish them here
        sensor.addField("co2", data.co2);
        sensor.addField("temp", data.temp);
        sensor.addField("humi", data.humi);
        sensor.addField("Pres", data.pressure);
        sensor.addField("Alt", data.altitude);

        if ((N_Common::debug_msk & (1U << DEBUG_DB_SERVER)) != 0U) {
            N_Helpers::idAndMsg(
                {.id   = pcTaskName,
                 .word = "Trying to write: " + sensor.toLineProtocol()});
        }

        InfluxDBClient db_client{InfluxDBClient(
            *N_InfluxDb::user_influx_url, *N_InfluxDb::user_influx_org,
            *N_InfluxDb::user_influx_bucket, *N_InfluxDb::user_influx_token)};
        if (!db_client.writePoint(sensor)) {
            if ((N_Common::debug_msk & (1U << DEBUG_DB_SERVER)) != 0U) {
                N_Helpers::idAndMsg({.id   = pcTaskName,
                                     .word = "InfluxDB write failed: " +
                                             db_client.getLastErrorMessage()});
            }
            failed_try += 1;
            return false;
        }

        if ((N_Common::debug_msk & (1U << DEBUG_DB_SERVER)) != 0U) {
            N_Helpers::idAndMsg({.id   = pcTaskName,
                                 .word = "Wrote: " + sensor.toLineProtocol()});
        }

        failed_try = 0;
        return true;
    }

} // namespace N_InfluxDb
