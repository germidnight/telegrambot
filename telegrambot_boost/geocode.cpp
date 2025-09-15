#include "logger.h"
#include "geocode.h"

#include <format>
#include <string>


namespace geo {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

namespace util {

/* Разбор геоданных.
 * Мы запрашиваем только один ответ от сервера
 * Если поле found содержит строку "0", то город не найден
 * Далее из массива featureMember (если город найден, должен содержать только один элемент)
 * извлекаемописание города и его координаты */
std::optional<GeoInfo> ResponseProcess(const json::value& msg) {
    // количество найденных городов, если не нашёл, то "0"
    const std::string path_found_str{"/response/GeoObjectCollection/metaDataProperty/GeocoderResponseMetaData/found"};
    // массив найденных городов
    const std::string path_feature_str{"/response/GeoObjectCollection/featureMember"};
    // далее ищем в path_feature_str (берём только 0-ой элемент массива)
    const std::string path_address_str{"/GeoObject/metaDataProperty/GeocoderMetaData/text"};
    const std::string path_pos_str{"/GeoObject/Point/pos"}; // получим строку "49.106414 55.796127" {longitude, latitude}

    GeoInfo town_info{};
    boost::system::error_code ec;
    auto* found_val = msg.find_pointer(path_found_str, ec);
    if (found_val && found_val->is_string() && (found_val->as_string() != "0")) {
        auto* feature_val = msg.find_pointer(path_feature_str, ec);
        if (feature_val && feature_val->is_array()) {
            for (const auto& val : feature_val->as_array()) {
                auto* address_val = val.find_pointer(path_address_str, ec);
                if (address_val && address_val->is_string()) {
                    town_info.address = address_val->as_string();
                }
                auto* pos_val = val.find_pointer(path_pos_str, ec);
                if (pos_val && pos_val->is_string()) {
                    std::string point{pos_val->as_string()};
                    size_t ch_pos{0};
                    town_info.longitude = std::stod(point, &ch_pos);
                    town_info.latitude = std::stod(point.substr(ch_pos));
                }
                break;
            }
        }
    } else {
        return std::nullopt;
    }
    return town_info;
}

}

Geocode::Geocode(boost::asio::io_context& ioc)
    : client_(std::make_unique<https_sync::HttpsSyncClient>(ioc)) {}

/* запрос GET в API geocode-maps.yandex.ru должен быть URL-encoded
 * !!! town должен быть URL encoded */
std::optional<GeoInfo> Geocode::GetPosition(const std::string& town) const {
    const int http_version{11};

    try {
        client_->Connect(HOST);
        // Ищем только один город
        std::string target = std::format("/v1/?apikey={}"
                                         "&geocode={}&lang=ru_RU&results=1&format=json",
                                         API_KEY, town);
        http::request<http::string_body> req{http::verb::get,
                                             target,
                                             http_version};
        req.set(http::field::host, HOST);

        std::string answer = client_->Exchange(std::move(req));
        if (!answer.empty()) {
            return util::ResponseProcess(json::parse(answer));
        }
        client_->Disconnect();
    } catch (const std::exception& err) {
        logger::LogError(err.what(), "Geocode::GetPosition");
        return std::nullopt;
    }
    return std::nullopt;
}

}
