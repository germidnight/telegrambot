#pragma once
/*
 * Телеграмм бот.
 * Умеет отправлять тектстовые сообщения в ответ на непустые сообщения в телеграмм по токену бота.
 *
 * Создание и запуск:
 *** boost::asio::io_context ioc;
 *** boost::asio::co_spawn(ioc,
 ***                       telega::RunTelegramBot(token, callback_function),
 ***                       [](std::exception_ptr err) {
 ***                            if(err) {
 ***                                std::rethrow_exception(err);
 ***                            }
 ***                       });
 *** ioc.run();
 * callback_function имеет сигнатуру std::string(std::string) - она принимает на вход строку (сообщение от пользователя)
 * и возвращает остроку (сообщение пользователю)
 */

#include <boost/asio/co_spawn.hpp>
#include <boost/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <set>
#include <vector>

#include "httpsclient.h"

namespace telega {

using GetAnswerFunc = std::function<std::string(std::string&&)>;
using ResponseResults = std::vector<std::pair<std::string, std::string>>; // вектор пар (chat_id, town)

constexpr static char TEXT_FIELD[]{"text"};

class TelegramBot {
    constexpr static char URL_BASE[]{"https://api.telegram.org/bot"};
    constexpr static char HOST[]{"api.telegram.org"};

    constexpr static char GET_METHOD[]{"getUpdates"};
    constexpr static char SEND_METHOD[]{"sendMessage"};

    static constexpr int TIMEOUT = 30;

public:
    explicit TelegramBot(boost::asio::any_io_executor& executor,
                         const std::string& token,
                         GetAnswerFunc callback);
    ~TelegramBot();

    boost::asio::awaitable<void> Connect();
    boost::asio::awaitable<void> Start();
    void Stop();

private:
    boost::asio::awaitable<void> SendMessage(const std::string& chat_id, const std::string& msg); // формирование и отправка сообщения в указанный чат
    boost::asio::awaitable<boost::json::object> MakeRequest(const std::string& method, const std::string& payload); // формирование запроса к API telegram
    boost::asio::awaitable<void> Reconnect();

    std::string bot_token_;
    std::string api_url_;
    std::unique_ptr<https_client::HttpsClient> client_;
    GetAnswerFunc callback_;      // функция формирования ответа пользователю
    int64_t last_update_id_{};    // последнее обработанное обновление от Telegram API, для исключения повторных обновлений

    std::set<std::string> chats_;
    bool do_work_{false};
};

boost::asio::awaitable<void> RunTelegramBot(const std::string& token,
                                            GetAnswerFunc callback);

namespace util {
/* Обработка сообщений от API telegram
 * Возвращает вектор пар (chat_id, town) и last_update_id */
std::pair<ResponseResults, int64_t> ResponseProcess(const boost::json::object& msg);
}
}
