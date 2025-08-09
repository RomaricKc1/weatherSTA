#ifndef INIT_HPP
#define INIT_HPP

#include "btn.hpp"
#include "commons.hpp"
#include "graphics.hpp"
#include "sensors.hpp"
#include "sys.hpp"
#include "weather_api.hpp"
#include "web_server.hpp"
#include "wm.hpp"

namespace N_Init {
    extern int CLOCK_PENDING;

    // ////////////////////////////////////////////////////////////////////////
    void vSetupRebootOnClock();
    void vSetupStartInitTasks();
    void vSetupStartTasks();
    void vSetupStartMdns(const char *pcTaskName);

    void vSetupResolveInputs(S_ResolveInMenunArgs &in_n_menu, S_Menu &menu_n,
                             S_BtnState &btn_state, const char *pcTaskName);

    void vSetupUpdCntDownMenu(S_ResolveInMenunArgs &in_n_menu,
                              S_CountDownArgs &cnt_down_args,
                              EventBits_t &event_result);

    void vSetupPirLogicCtrl(S_ResolveInMenunArgs &in_n_menu, S_BlMgmt &bl_mgmt,
                            EventBits_t &event_result, S_PirSateArgs &pir_args,
                            TickType_t &last_detected_pir);

    void setupBase();

} // namespace N_Init

#endif // INIT_HPP
