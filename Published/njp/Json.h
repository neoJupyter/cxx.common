#pragma once

#include <kls/temp/STL.h>
#include <nlohmann/json.hpp>

namespace njp {
    using json = nlohmann::basic_json<>;

    using temp_json = nlohmann::basic_json<
            std::map, std::vector, kls::temp::string,
            bool, std::int64_t, std::uint64_t, double,
            kls::temp::allocator, nlohmann::adl_serializer, kls::temp::vector<std::uint8_t>
    >;

    template<class T, class Json>
    auto from_json(const Json &json) {
        T result = json;
        return result;
    }

    template<class Json, class T>
    Json to_json(const T &o) { return Json{o}; }
}
