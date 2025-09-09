#pragma once
/*
 * HTTPS клиент синхронный
 */
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <memory>
#include <string>

namespace https_sync {

using RequestType = boost::beast::http::request<boost::beast::http::string_body>;

class HttpsSyncClient {
public:
    explicit HttpsSyncClient(boost::asio::io_context& ioc);
    ~HttpsSyncClient();

    void Connect(const std::string& host);
    void Disconnect();

    std::string Exchange(RequestType req);

private:
    boost::asio::io_context& ioc_;
    std::unique_ptr<boost::asio::ssl::context> ssl_ctx_;
    std::unique_ptr<boost::beast::ssl_stream<boost::beast::tcp_stream>> stream_;
    std::string host_;
};

}
