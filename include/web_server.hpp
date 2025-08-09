#ifndef WEB_SERVER_HPP
#define WEB_SERVER_HPP

#include "commons.hpp"
#include "defines.hpp"
#include "graphics.hpp"

namespace N_WebServer {
    extern std::unique_ptr<AsyncWebServer> server;

    using ValueType = std::variant<
        std::monostate, std::reference_wrapper<String>,
        std::reference_wrapper<float>, std::reference_wrapper<int>,
        std::reference_wrapper<const std::basic_string_view<char>>,
        std::reference_wrapper<std::array<char, MAX_CHAR_BUF_TIME>>>;
    // ////////////////////////////////////////////////////////////////////////

    void initServer();
    void updateOfflineValues(S_DataOffline &struct_2,
                             const S_DataOffline &off_data);
    void updateOnlineValues(S_OnData &struct_, const S_DataOnline &data);
    void serverInit(S_OnData &on_now, S_OnData &on_fcast,
                    S_DataOffline &data_offline, int &system_state);
    void updateServerValues(TickType_t xTick_offline,
                            S_DataOffline &my_data_off,
                            S_DataOffline &data_offline,
                            TickType_t xTick_online, S_DataOnline &my_data,
                            S_OnData &on_now, S_OnData &on_fcast);

    template <typename T> auto getValue(const ValueType &value) -> T &;
    template <typename T>
    auto serverGetValues(const S_ServerValuesArgs &args, T &ret,
                         const String &pctaskname);
    auto serverLoadFiles(const S_ServerValuesArgs &args) -> void;
    void autoPutValuesMap(const std::map<String, ValueType> &put_values);

} // namespace N_WebServer

#endif // WEB_SERVER_HPP
