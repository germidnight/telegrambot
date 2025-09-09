#pragma once
/*
 * Получение и хранение информации о погоде в городах
 * 1) Запрашиваем координаты города у класса geo::Geocode
 * 2) По полученным координатам запрашиваем погоду у open-meteo.com и возвращаем в виде строки
 * Примечания:
 * 1) Координаты запрашиваются только для новых городов (для несуществующих городов запрашиваются всегда!)
 * 2) Погода запрашивается только для ранее не запрошенных городов и по прошествии таймаута устаревания данных
 *
 * Использование:
 *** meteo::MeteoBot bot(ioc);
 *** std::string weather = bot.GetWeather(std::move(town));
 *
 * TODO:
 * 1) Исключить дублирование информации о городах введённых в транслите и кириллицей, а также с указанием региона (и/или страны) и без неё
 * 2) Оптимизировать запросы на обновление данных о погоде и получение информации о координатах города
 */
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "httpssyncclient.h"
#include "geocode.h"

namespace meteo {

constexpr static double KAZAN_LATITUDE{55.8125}; // широта
constexpr static double KAZAN_LONGITUDE{49.1221}; // долгота
constexpr static char KAZAN_ADDRESS[]{"Россия, Республика Татарстан (Татарстан), Казань"};

struct Units {
    std::string time_format{"iso8601"};
    std::string temperature{"°C"};
    std::string rain{"mm"};
    std::string snowfall{"cm"};
};

struct Forecast {
    std::vector<std::string> time;
    std::vector<float> temperature;
    std::vector<float> rain;
    std::vector<float> snowfall;
};

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

struct MeteoInfo {
    double latitude{KAZAN_LATITUDE};
    double longitude{KAZAN_LONGITUDE};
    TimePoint updated_time{std::chrono::system_clock::now()};
    bool valid{false};
    Units units{};
    Forecast forecast{};
    std::string address{KAZAN_ADDRESS};

    std::string ToString() const;
};

using MeteoMap = std::unordered_map<std::string, MeteoInfo>; // Города, данные о погоде

class MeteoBot {
    constexpr static char HOST[]{"api.open-meteo.com"};
    constexpr static std::chrono::minutes OLD_DATA_TIMEOUT{15};

    constexpr static char DEFAULT_TOWN[]{"%D0%BA%D0%B0%D0%B7%D0%B0%D0%BD%D1%8C"}; // казань

public:
    constexpr static int HOUR_RESOLUTION{3};

    MeteoBot(boost::asio::io_context& ioc);

    std::string GetWeather(std::string town);

private:
    void UpdateWeather(const std::string& town);

    std::unique_ptr<https_sync::HttpsSyncClient> client_;
    std::unique_ptr<geo::Geocode> geocode_;
    MeteoMap weather_;
};

namespace util {
MeteoInfo ResponseProcess(const boost::json::object& msg);
}

}
