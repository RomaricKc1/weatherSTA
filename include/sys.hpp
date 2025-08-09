#ifndef SYS_HPP
#define SYS_HPP

#include "commons.hpp"
#include "graphics.hpp"
#include "init.hpp"
#include "web_server.hpp"
#include "wm.hpp"

namespace N_Sys {
    extern RTC_DATA_ATTR int boot_times;
    extern RTC_DATA_ATTR int64_t SLEEP_DURATION_M;
    extern RTC_DATA_ATTR bool RebootOnClock;
    extern int64_t IDLE_TMS;

    extern TaskHandle_t xTaskOnline;
    extern TaskHandle_t xTaskOffline;
    extern TaskHandle_t xTaskDaemon;
    extern TaskHandle_t xWelcome;

    extern std::reference_wrapper<int64_t> sleep_duration_m_refw;
    extern std::array<std::array<int32_t, MENU_INNER_6>, MENUS> config_set;

    // ////////////////////////////////////////////////////////////////////////
    void vTaskOffline(void * /*pvParameters*/);
    void vTaskOnline(void * /*pvParameters*/);
    void vTaskWelcome(void * /*pvParameters*/);
    void vDaemon(void * /*pvParameters*/);

    void rebootEsp(int secon_);
    void clockOrStaMode();
    void checkCExecPortal(bool valid_portal);
    void checkWifiState(S_WifiCoArgs &co_args);

    void taskNotifMan(S_TaskNotifArgs &notif_args, bool &idle);
    void broadcastConfMode(bool clear);
    void timerDaemon(TimerHandle_t xTimer);
    void releaserDaemon(TimerHandle_t xTimer);

    void idleSTAMode(S_IdleArgs &idle_args);
    void wifiCoIssues(S_WifiCoArgs &co_args);
    void launchMenuTimout(S_ResolveInMenunArgs &menu_args,
                          const String &pcTaskName);
    void applyMenuSave(S_Menu &menu_n, S_ResolveInMenunArgs &apply_args);
    void exitBeforeTimeout(S_ResolveInMenunArgs &exit_args);
    void handleMenuLogic(S_Menu &menu_n, S_ResolveInMenunArgs &menu_logic_args);
    auto createTimer(int64_t duration, TimerCallbackFunction_t(call_func_ptr),
                     const S_IdCreateTimer &timer_id) -> bool;
    void resolveInputs(S_Menu &menu_n, S_BtnState &btn_state,
                       S_ResolveInMenunArgs &inputs_args,
                       const char *pcTaskName);

    void vDaemonInitiateIdleCheck(S_IdleArgs &idle_args,
                                  EventBits_t &event_result, const bool &idle);
    void vDaemonInitiateSleepCheck(S_IdleArgs &idle_args,
                                   const char *pcTaskName);

} // namespace N_Sys

#endif // SYS_HPP
