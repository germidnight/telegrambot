/*
 * Погодный телеграмм бот
 * - телеграмм бот выполняется асинхронно
 * - запросы погоды в городах выполняются синхронно
 * - логируются: ошибки и подключение/отключение от сетевых ресурсов
 *
 * TODO:
 * 1) Сделать асинхронными запросы погоды
 */
#include "logger.h"
#include "meteobot.h"
#include "telegrambot.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <csignal>
#include <ios>
#include <memory>
#include <string>
#include <utility>

using namespace std::string_literals;

struct WeatherGet {
    std::string operator()(std::string&& town) {
        return bot->GetWeather(std::move(town));
    }

    std::shared_ptr<meteo::MeteoBot> bot;
};

int main() {
    const std::string telegramm_token{"here_must_be_your_telegram_bot_token"};

    boost::log::add_console_log(
        std::clog,
        boost::log::keywords::format = &(logger::LogFormatter),
        boost::log::keywords::auto_flush = true
    );

    boost::log::add_file_log(
        boost::log::keywords::file_name = "telegrambot_%N.log",
        boost::log::keywords::format = &(logger::LogFormatter),
        boost::log::keywords::open_mode = std::ios_base::app | std::ios_base::out,
        boost::log::keywords::rotation_size = 10 * 1024 * 1024,
        boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(12, 0, 0),
        boost::log::keywords::auto_flush = true
    );

    boost::asio::io_context ioc;

    boost::asio::signal_set signal_set(ioc, SIGINT, SIGTERM);
    signal_set.async_wait([&ioc](const boost::system::error_code& ec, [[maybe_unused]] int signal_number) {
        if (!ec) {
            ioc.stop();
        }
    });

    try {
        boost::asio::co_spawn(ioc,
                              telega::RunTelegramBot(telegramm_token,
                                                     WeatherGet{std::make_shared<meteo::MeteoBot>(ioc)}),
                              [](std::exception_ptr err) {
                                  if(err) {
                                      std::rethrow_exception(err);
                                  }
                              });
        ioc.run();

    } catch(const std::exception& err) {
        logger::LogError(err.what(), "main");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
