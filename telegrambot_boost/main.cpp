/*
 * Погодный телеграмм бот
 * - телеграмм бот выполняется асинхронно
 * - запросы погоды в городах выполняются синхронно
 *
 * TODO:
 * 1) Сделать нормальное логирование
 * 2) Сделать асинхронными запросы погоды
 */
#include "meteobot.h"
#include "telegrambot.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <iostream>
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
        std::cerr << "MAIN. Error: " << err.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
