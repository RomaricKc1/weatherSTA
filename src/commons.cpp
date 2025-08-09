#include "commons.hpp"

namespace N_Common {
    unsigned int debug_msk = 0b00000000;

    std::unique_ptr<ESP32Time> rtc;

    std::array<char, MAX_CHAR_BUF_TIME> formated_time{};

    EventGroupHandle_t handler_event{};

    SemaphoreHandle_t xLCDAccess = nullptr;
    SemaphoreHandle_t xSerial    = nullptr;
    SemaphoreHandle_t xI2C       = nullptr;

    TimerHandle_t one_x_timer          = nullptr;
    TimerHandle_t scd_cal_one_x_timer  = nullptr;
    TimerHandle_t one_x_timer_releaser = nullptr;

    QueueHandle_t xQueueOn_NOW     = nullptr;
    QueueHandle_t xQueueOn_FCAST   = nullptr;
    QueueHandle_t xQueueOffline    = nullptr;
    QueueHandle_t xQueueRain       = nullptr;
    QueueHandle_t xTempOut         = nullptr;
    QueueHandle_t xClockMode       = nullptr;
    QueueHandle_t xPortalMode      = nullptr;
    QueueHandle_t x24h_Mode        = nullptr;
    QueueHandle_t xIdle_T          = nullptr;
    QueueHandle_t xInfluxDB_state  = nullptr;
    QueueHandle_t xXtend_BL_T      = nullptr;
    QueueHandle_t xInfluxFailedAtt = nullptr;

    struct tm STimeInfo{};
    struct tm STimeinfoLocal{};

    void initRtc() {
        try {
            rtc = std::make_unique<ESP32Time>();
        } catch (const std::exception &e) {
            assert("abort");
        }
    }

    void defineSynchroElements() {
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
        xQueueOn_FCAST = xQueueCreate(MAILBOX_QUEUE, sizeof(S_DataOnline));
        assert(xQueueOn_FCAST);
        xQueueOn_NOW = xQueueCreate(MAILBOX_QUEUE, sizeof(S_DataOnline));
        assert(xQueueOn_NOW);

        xQueueOffline = xQueueCreate(MAILBOX_QUEUE, sizeof(S_DataOffline));
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

        xInfluxFailedAtt = xQueueCreate(MAILBOX_QUEUE, sizeof(int));
        assert(xInfluxFailedAtt);
    }
}; // namespace N_Common
