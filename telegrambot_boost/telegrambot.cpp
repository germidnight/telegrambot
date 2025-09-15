#include "telegrambot.h"

#include <iostream>
#include <utility>

#include <boost/beast.hpp>

namespace telega {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace http = boost::beast::http;
namespace json = boost::json;

namespace util {
/* Обрабатываем ответ от телеграмм
 * Ищем:
 * - сообщение пользователя (/result/message/text) - предполагается название города
 * - id чата (/result/message/chat/id)
 * объект result - это массив сообщений от разных чатов (пользователей)
 * На выходе массив пар {chat_id, town} и last_update_id */
std::pair<ResponseResults, int64_t> ResponseProcess(const boost::json::object& msg) {
    constexpr static char OK_FIELD[]{"ok"};
    constexpr static char RESULT_FIELD[]{"result"};
    constexpr static char UPDATE_ID_FIELD[]{"update_id"};
    constexpr static char MESSAGE_FIELD[]{"message"};
    constexpr static char CHAT_FIELD[]{"chat"};
    constexpr static char ID_FIELD[]{"id"};

    const std::string base_town{"Казань"};

    if (!msg.at(OK_FIELD).as_bool()) {
        std::cerr << "Request failure. RESPONSE: " << json::serialize(msg) << std::endl;
        return {};
    }
    if (!msg.contains(RESULT_FIELD) || !msg.at(RESULT_FIELD).is_array()) {
        return {};
    }
    ResponseResults towns_for_chats;
    int64_t last_update_id{};
    json::array res_values = msg.at(RESULT_FIELD).as_array();
    for (const auto& res : res_values) {
        if (res.as_object().contains(UPDATE_ID_FIELD) && res.as_object().at(UPDATE_ID_FIELD).is_int64()) {
            last_update_id = res.as_object().at(UPDATE_ID_FIELD).as_int64();
        }

        boost::system::error_code ec;
        std::string text_path = "/" + std::string(MESSAGE_FIELD) +
                                "/" + std::string(TEXT_FIELD);
        auto* text_val = res.find_pointer(text_path, ec);
        std::string town = (text_val && text_val->is_string()) ? (std::string(text_val->as_string())) :
                               base_town;

        std::string chat_path = "/" + std::string(MESSAGE_FIELD) +
                                "/" + std::string(CHAT_FIELD) +
                                "/" + std::string(ID_FIELD);
        auto* chat_val = res.find_pointer(chat_path, ec);
        if (chat_val && chat_val->is_int64()) {
            towns_for_chats.push_back({std::to_string(chat_val->as_int64()), town});
        }
    }
    return std::pair{towns_for_chats, last_update_id};
}
}

TelegramBot::TelegramBot(boost::asio::any_io_executor& executor,
                         const std::string& token,
                         GetAnswerFunc callback)
    : bot_token_(token)
    , api_url_(URL_BASE + token)
    , client_(std::make_unique<https_client::HttpsClient>(executor))
    , callback_(callback) {}

TelegramBot::~TelegramBot() {
    client_->Disconnect();
}

boost::asio::awaitable<void> TelegramBot::Connect() {
    co_await client_->Connect(HOST);
    co_return;
}

boost::asio::awaitable<void> TelegramBot::Reconnect() {
    using namespace std::chrono_literals;
    client_->Disconnect();
    co_await Connect();
    co_return;
}

/* Основной цикл работы бота
 * Проверяем подключение к серверу перед началом работы
 * В цикле:
 * - Запрашиваем обновления от сервера (getUpdates)
 * - Разбираем ответ сервера
 * - Отправляем сообщения по всем чатам, запросившим информацию */
boost::asio::awaitable<void> TelegramBot::Start() {
    constexpr static char OFFSET_FIELD[]{"offset"};
    if (!client_->IsConnected()) {
        std::cerr << "TelegramBot::Start. Must be connected before running start" << std::endl;
        co_return;
    }
    do_work_ = true;
    while (do_work_) {
        try {
            json::object payload{};
            if (last_update_id_ != 0) {
                payload[OFFSET_FIELD] = last_update_id_ + 1;
            }
            auto response = co_await MakeRequest(GET_METHOD, json::serialize(payload));
            if (response.empty()) {
                co_await Reconnect();
                continue;
            }
            auto [answer, last_update_id] = util::ResponseProcess(response);
            last_update_id_ = last_update_id;
            for (const auto& [chat_id, town] : answer) {
                auto [it, is_inserted] = chats_.insert(chat_id);
                if (is_inserted) {
                    std::cout << "New chat created: " << *it << std::endl;
                }
                co_await SendMessage(chat_id, callback_(std::string(town)));
            }
        } catch (const std::exception& err) {
            std::cerr << "TelegramBot::Start. Error: " << err.what() << std::endl;
        }
    }
}

void TelegramBot::Stop() {
    do_work_ = false;
}

// формирование и отправка сообщения в указанный чат
boost::asio::awaitable<void> TelegramBot::SendMessage(const std::string& chat_id,
                                                      const std::string& text) {
    constexpr static char CHAT_ID_FIELD[]{"chat_id"};

    json::object payload;
    payload[CHAT_ID_FIELD] = chat_id;
    payload[TEXT_FIELD] = text;
    co_await MakeRequest(SEND_METHOD, json::serialize(payload));
}

// формирование запроса к API telegram
boost::asio::awaitable<json::object> TelegramBot::MakeRequest(const std::string& method,
                                                              const std::string& payload) {
    const std::string content_type{"application/json"};
    const int http_version{11};
    try {
        http::request<http::string_body> req{http::verb::post,
                                             "/bot" + bot_token_ + "/" + method,
                                             http_version};
        req.set(http::field::host, HOST);
        req.set(http::field::content_type, content_type);
        req.body() = std::move(payload);
        req.prepare_payload();

        std::string res = co_await client_->Exchange(req);
        auto res_obj = json::parse(res).as_object();
        co_return res_obj;
    } catch (const std::exception&) {
        co_return json::object{};
    }
}


// Функция запуска бота
boost::asio::awaitable<void> RunTelegramBot(const std::string& token, GetAnswerFunc callback) {
    auto executor = co_await asio::this_coro::executor;

    TelegramBot t_bot(executor, token, callback);
    co_await t_bot.Connect();
    co_await t_bot.Start();
}

}
