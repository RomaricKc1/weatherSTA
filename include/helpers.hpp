#ifndef HELPERS_HPP
#define HELPERS_HPP

#include "commons.hpp"

namespace N_Helpers {
    void idAndMsg(const S_IdMsg &args);
    void getClockData(S_TimeVar &time_vars, const String &pctaskname);

    auto spiffReadFile(const S_SpiffsOp &args)
        -> std::optional<std::map<String, String>>;

    auto spiffWriteFile(const S_SpiffsOp &args,
                        const std::map<String, String> &config_map) -> bool;

    auto doesMapContain(std::map<String, String> &this_map,
                        const std::string_view &key) -> bool;

    template <typename T>
    auto convertStrToInt(const S_ConvertIntArgs &conv_args) -> T;

    void clockDataParse(S_TimeVar &time_vars, const String &pctaskname);

} // namespace N_Helpers

#endif // HELPERS_HPP
