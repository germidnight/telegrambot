#include "meteobot.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/url/encode.hpp>
#include <boost/url/rfc/pchars.hpp>
#include <cmath>
#include <format>
#include <iostream>

namespace meteo {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using namespace std::chrono;

namespace util {
MeteoInfo ResponseProcess(const boost::json::object& msg) {
    const std::string hourly_units_obj_str{"hourly_units"};
    const std::string hourly_obj_str{"hourly"};

    const std::string time_val_str{"time"};
    const std::string temperature_val_str{"temperature_2m"};
    const std::string rain_val_str{"rain"};
    const std::string snowfall_val_str{"snowfall"};
    MeteoInfo weather{};

    if (msg.contains(hourly_obj_str) && msg.at(hourly_obj_str).is_object()) {
        auto forecast_obj = msg.at(hourly_obj_str).as_object();
        if (forecast_obj.contains(time_val_str) && forecast_obj.at(time_val_str).is_array()) {
            auto vals = forecast_obj.at(time_val_str).as_array();
            weather.forecast.time.clear();
            for (auto val : vals) {
                weather.forecast.time.emplace_back(std::string(val.as_string()));
            }
            weather.updated_time = system_clock::now();
            weather.valid = true;
        }
        if (forecast_obj.contains(temperature_val_str) && forecast_obj.at(temperature_val_str).is_array()) {
            auto vals = forecast_obj.at(temperature_val_str).as_array();
            weather.forecast.temperature.clear();
            for (auto val : vals) {
                weather.forecast.temperature.push_back(static_cast<float>(val.as_double()));
            }
        }
        if (forecast_obj.contains(rain_val_str) && forecast_obj.at(rain_val_str).is_array()) {
            auto vals = forecast_obj.at(rain_val_str).as_array();
            weather.forecast.rain.clear();
            for (auto val : vals) {
                weather.forecast.rain.push_back(static_cast<float>(val.as_double()));
            }
        }
        if (forecast_obj.contains(snowfall_val_str) && forecast_obj.at(snowfall_val_str).is_array()) {
            auto vals = forecast_obj.at(snowfall_val_str).as_array();
            weather.forecast.snowfall.clear();
            for (auto val : vals) {
                weather.forecast.snowfall.push_back(static_cast<float>(val.as_double()));
            }
        }
    }

    if (msg.contains(hourly_units_obj_str) && msg.at(hourly_units_obj_str).is_object()) {
        auto units_obj = msg.at(hourly_obj_str).as_object();
        if (units_obj.contains(time_val_str) && units_obj.at(time_val_str).is_string()) {
            weather.units.time_format = units_obj.at(time_val_str).as_string();
        }
        if (units_obj.contains(temperature_val_str) && units_obj.at(temperature_val_str).is_string()) {
            weather.units.temperature = units_obj.at(temperature_val_str).as_string();
        }
        if (units_obj.contains(rain_val_str) && units_obj.at(rain_val_str).is_string()) {
            weather.units.rain = units_obj.at(rain_val_str).as_string();
        }
        if (units_obj.contains(snowfall_val_str) && units_obj.at(snowfall_val_str).is_string()) {
            weather.units.snowfall = units_obj.at(snowfall_val_str).as_string();
        }
    }
    return weather;
}
}

/* -------- MeteoBot -------- */

MeteoBot::MeteoBot(boost::asio::io_context& ioc)
    : client_(std::make_unique<https_sync::HttpsSyncClient>(ioc))
    , geocode_(std::make_unique<geo::Geocode>(ioc)) {}

/* Получаем погоду для запрошенного города
 * 1) строку с городом очищаем от лишних пробелов и приводим все буквы к строчным
 * 2) производим url encode строки с запрошенным городом, в дальнейшем будем работать только с ней
 * 3) если для запрошенного города координат ещё не запрашивали, то запрашиваем координаты города в Geocode
 *      - если города не существует (не найден в geocode) используем город по умолчанию
 * 4) получаем погоду для запрошенного города:
 *      - если погода для города ещё не получена или если данные устарели, обновляем погодные данные для города */
std::string MeteoBot::GetWeather(std::string town) {
    boost::algorithm::trim_all(town);
    boost::algorithm::to_lower(town);
    std::string enc_town = boost::urls::encode(town, boost::urls::pchars);

    if (!weather_.contains(enc_town)) {
        auto town_info = geocode_->GetPosition(enc_town);
        if (town_info) {
            weather_[enc_town] = {.latitude = town_info->latitude,
                                  .longitude = town_info->longitude,
                                  .address = town_info->address};
        } else {
            enc_town = DEFAULT_TOWN;
            if (!weather_.contains(enc_town)) {
                weather_[enc_town] = {};
            }
        }
    }
    if (!weather_[enc_town].valid ||
        (system_clock::now() - weather_[enc_town].updated_time) > OLD_DATA_TIMEOUT) {
        try {
            UpdateWeather(enc_town);
        } catch (const std::exception& err) {
            std::cerr << "MeteoBot::GetWeather. Error: " << err.what() << std::endl;
            return "Ошибка получения прогноза погоды";
        }
    }
    return weather_[enc_town].ToString();
}

void MeteoBot::UpdateWeather(const std::string& town) {
    const int http_version{11};

    client_->Connect(HOST);
    std::string target = std::format("/v1/forecast?latitude={}&longitude={}"
                                     "&hourly=temperature_2m,rain,snowfall"
                                     "&timezone=Europe%2FMoscow"
                                     "&forecast_days=2"
                                     "&temporal_resolution=hourly_{}",
                                     weather_[town].latitude, weather_[town].longitude, HOUR_RESOLUTION);

    http::request<http::string_body> req{http::verb::get,
                                         target,
                                         http_version};
    req.set(http::field::host, HOST);

    std::string answer = client_->Exchange(std::move(req));
    if (!answer.empty()) {
        auto weather = util::ResponseProcess(json::parse(answer).as_object());
        weather_[town].updated_time = weather.updated_time;
        weather_[town].valid = weather.valid;
        weather_[town].forecast = std::move(weather.forecast);
        weather_[town].units = std::move(weather.units);
    }
    client_->Disconnect();
}

/* -------- MeteoInfo -------- */

std::string MeteoInfo::ToString() const {
    const time_t cur_time_t{system_clock::to_time_t(system_clock::now())};
    tm cur_tm{};
#ifdef Q_OS_WINDOWS
    localtime_s(&cur_tm, &cur_time_t);
#endif
#ifdef Q_OS_LINUX
    localtime_r(&cur_time_t, &cur_tm);
#endif
    if (!forecast.time.empty()) {
        const size_t idx = cur_tm.tm_hour / MeteoBot::HOUR_RESOLUTION + 1;

        std::string time = (forecast.time.size() > idx) ? forecast.time[idx] : "NONE";
        float temperature = (forecast.temperature.size() > idx) ? forecast.temperature[idx] : INFINITY;
        float rain = (forecast.rain.size() > idx) ? forecast.rain[idx] : 0;
        float snowfall = (forecast.snowfall.size() > idx) ? forecast.snowfall[idx] : 0;

        return std::format("Погода в {} на {}:\nТемпература: {} °C\nОсадки: {} мм",
                           address, time, temperature, rain + snowfall * 10);
    } else {
        return std::format("Ошибка при разборе ответа от сервера."
                           "Размер массива time: {}"
                           "Размер массива temperature: {}"
                           "Размер массива rain: {}"
                           "Размер массива snowfall: {}",
                           forecast.time.size(), forecast.temperature.size(),
                           forecast.rain.size(), forecast.snowfall.size());
    }
}

}
