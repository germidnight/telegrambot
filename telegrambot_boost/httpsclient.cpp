#include "logger.h"
#include "httpsclient.h"

#include <chrono>
#include <sstream>

namespace https_client {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;
namespace http = beast::http;

namespace {
void CheckSSLShutdownError(boost::system::error_code ec) {
    // Ignore the "stream truncated" error, as it is common during SSL shutdown
    if (ec == asio::ssl::error::stream_truncated) {
        ec.assign(0, ec.category());
    } else if (ec) {
        logger::LogError(std::string("Ошибка при выключении: ") + ec.to_string(), "HttpsClient");
    }
}

}

HttpsClient::HttpsClient(boost::asio::any_io_executor& executor)
    : executor_(executor) {}

HttpsClient::~HttpsClient() {
    boost::system::error_code ec;
    stream_->shutdown(ec);
    CheckSSLShutdownError(ec);
}

boost::asio::awaitable<void> HttpsClient::Connect(const std::string& host) {
    host_ = host;
    ssl_ctx_ = std::make_unique<ssl::context>(ssl::context::tlsv12_client);
    stream_ = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(executor_, *ssl_ctx_);

    ssl_ctx_->set_default_verify_paths();
    ssl_ctx_->set_verify_mode(ssl::verify_peer);
    stream_->set_verify_callback(ssl::host_name_verification(host_));

    tcp::resolver resolver(executor_);
    auto const resolver_result = co_await resolver.async_resolve(host_, port_, boost::asio::use_awaitable);

    boost::beast::get_lowest_layer(*stream_).expires_after(std::chrono::seconds(TIMEOUT));
    co_await boost::beast::get_lowest_layer(*stream_).async_connect(resolver_result, boost::asio::use_awaitable);

    boost::beast::get_lowest_layer(*stream_).expires_after(std::chrono::seconds(TIMEOUT));
    co_await stream_->async_handshake(asio::ssl::stream_base::client, boost::asio::use_awaitable);

    logger::LogInfo(std::string("Connected to ") + host_, "HttpsClient::Connect");
    co_return;
}

void HttpsClient::Disconnect() {
    boost::system::error_code ec;
    stream_->shutdown(ec);
    logger::LogInfo(std::string("Disconnected from ") + host_, "HttpsClient::Connect");
    CheckSSLShutdownError(ec);
}

bool HttpsClient::IsConnected() const noexcept {
    return ssl_ctx_ && stream_;
}

boost::asio::awaitable<std::string> HttpsClient::Exchange(http::request<http::string_body> req) {
#ifdef Q_OS_WINDOWS
    if (!stream_->lowest_layer().is_open()) {
        logger::LogError(std::string("Нет подключения к серверу ") + host_, "HttpsClient::Exchange");
        co_return std::string{};
    }
#endif
#ifdef Q_OS_LINUX
    if (!stream_->next_layer().is_open()) {
        logger::LogError(std::string("Нет подключения к серверу ") + host_, "HttpsClient::Exchange");
        co_return std::string{};
    }
#endif
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    boost::beast::get_lowest_layer(*stream_).expires_after(std::chrono::seconds(TIMEOUT));
    co_await http::async_write(*stream_, req, boost::asio::use_awaitable);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    co_await http::async_read(*stream_, buffer, res, boost::asio::use_awaitable);
    if (res.result() == http::status::ok) {
        co_return res.body();
    } else {
        std::stringstream ss;
        ss << "Пришёл неверный ответ от сервера:\n" << res;
        logger::LogError(ss.str(), "HttpsClient::Exchange");
    }
    co_return std::string{};
}
}
