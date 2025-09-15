#pragma once
/*
 * Получение координат и текстового описания города по названию
 * Используется API geocode-maps.yandex.ru
 */
#include "httpssyncclient.h"

#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <memory>
#include <optional>
#include <string>

namespace geo {

struct GeoInfo {
    double latitude{};
    double longitude{};
    std::string address{};
};

class Geocode {
    constexpr static char API_KEY[]{"Here_must_be_Your_api_key"};
    constexpr static char HOST[]{"geocode-maps.yandex.ru"};
public:
    Geocode(boost::asio::io_context& ioc);

    std::optional<GeoInfo> GetPosition(const std::string& town) const;

private:
    std::unique_ptr<https_sync::HttpsSyncClient> client_;
};

namespace util {
std::optional<GeoInfo> ResponseProcess(const boost::json::value& msg);
}

}
