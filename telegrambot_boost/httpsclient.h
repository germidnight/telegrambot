#pragma once
/*
 * HTTPS клиент
 * асинхронный на корутинах
 */
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <string>

namespace https_client {

class HttpsClient {
    static constexpr int TIMEOUT = 30;
    const std::string port_{"443"};

public:
    explicit HttpsClient(boost::asio::any_io_executor& executor);
    ~HttpsClient();

    boost::asio::awaitable<void> Connect(const std::string& host);
    void Disconnect();

    bool IsConnected() const noexcept;

    boost::asio::awaitable<std::string> Exchange(boost::beast::http::request<boost::beast::http::string_body> req);

private:
    boost::asio::any_io_executor& executor_;
    std::string host_;

    std::unique_ptr<boost::asio::ssl::context> ssl_ctx_;
    std::unique_ptr<boost::beast::ssl_stream<boost::beast::tcp_stream>> stream_;
};

}
