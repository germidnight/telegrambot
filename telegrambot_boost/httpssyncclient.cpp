#include "logger.h"
#include "httpssyncclient.h"

#include <sstream>

namespace https_sync {

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
        logger::LogError(std::string("Ошибка при выключении: ") + ec.to_string(), "HttpsSyncClient");
    }
}
}

HttpsSyncClient::HttpsSyncClient(boost::asio::io_context& ioc)
    : ioc_(ioc) {}

HttpsSyncClient::~HttpsSyncClient() {
    Disconnect();
}

void HttpsSyncClient::Connect(const std::string& host) {
    host_ = host;
    const std::string port{"443"};

    ssl_ctx_ = std::make_unique<ssl::context>(ssl::context::tlsv12_client);
    ssl_ctx_->set_default_verify_paths();
    ssl_ctx_->set_verify_mode(ssl::verify_none);

    tcp::resolver resolver(ioc_);
    stream_ = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(ioc_, *ssl_ctx_);

    if(!SSL_set_tlsext_host_name(stream_->native_handle(), host_.c_str())) {
        throw beast::system_error(
            static_cast<int>(::ERR_get_error()),
            asio::error::get_ssl_category());
    }
    stream_->set_verify_callback(ssl::host_name_verification(host_));

    auto const results = resolver.resolve(host_, port);
    beast::get_lowest_layer(*stream_).connect(results);
    stream_->handshake(ssl::stream_base::client);

    logger::LogInfo(std::string("Connected to ") + host_, "HttpsSyncClient::Connect");
}

void HttpsSyncClient::Disconnect() {
    boost::system::error_code ec;
    stream_->shutdown(ec);
    logger::LogInfo(std::string("Disconnected from ") + host_, "HttpsSyncClient::Disconnect");
    CheckSSLShutdownError(ec);
}

std::string HttpsSyncClient::Exchange(RequestType req) {
#ifdef Q_OS_WINDOWS
    if (!stream_->lowest_layer().is_open()) {
        logger::LogError(std::string("Нет подключения к серверу ") + host_, "HttpsSyncClient::Exchange");
        return {};
    }
#endif
#ifdef Q_OS_LINUX
    if (!stream_->next_layer().is_open()) {
        logger::LogError(std::string("Нет подключения к серверу ") + host_, "HttpsSyncClient::Exchange");
        return {};
    }
#endif
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    http::write(*stream_, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;

    http::read(*stream_, buffer, res);
    if (res.result() == http::status::ok) {
        return res.body();
    } else {
        std::stringstream ss;
        ss << "Пришёл неверный ответ от сервера:\n" << res;
        logger::LogError(ss.str(), "HttpsSyncClient::Exchange");
        return {};
    }
}

}
