#pragma once

#include "fixed_income/fix.hpp"

#include <boost/asio.hpp>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fixed_income {

class FixSessionManager;

class FixConnection : public std::enable_shared_from_this<FixConnection> {
public:
    using Tcp = boost::asio::ip::tcp;
    using MessageHandler = std::function<void(FixMessage)>;
    using ErrorHandler = std::function<void(const boost::system::error_code&)>;

    FixConnection(boost::asio::io_context& ioContext, FixSessionManager& sessionManager);
    ~FixConnection();

    Tcp::socket& socket();
    void start();
    void sendMessage(const FixMessage& message);
    void close();

    void setMessageHandler(MessageHandler handler);
    void setErrorHandler(ErrorHandler handler);

private:
    static constexpr std::size_t kReadBufferSize = 8192;

    boost::asio::io_context& ioContext_;
    Tcp::socket socket_;
    FixSessionManager& sessionManager_;
    std::vector<char> readBuffer_;
    std::deque<std::string> writeQueue_;
    MessageHandler messageHandler_;
    ErrorHandler errorHandler_;
    std::string incomingBuffer_;

    void doRead();
    void doWrite();
    void handleRead(const boost::system::error_code& ec, std::size_t bytesTransferred);
    void handleWrite(const boost::system::error_code& ec, std::size_t bytesTransferred);
    void processIncomingData();
};

class FixSessionManager {
public:
    explicit FixSessionManager(std::string senderCompID, std::string targetCompID);

    std::shared_ptr<FixConnection> createConnection(boost::asio::io_context& ioContext);
    FixSession& session() noexcept;

private:
    FixSession session_;
};

class FixServer {
public:
    using Tcp = boost::asio::ip::tcp;

    FixServer(boost::asio::io_context& ioContext, unsigned short port, FixSessionManager& sessionManager);
    void startAccept();
    void stop();

private:
    boost::asio::io_context& ioContext_;
    Tcp::acceptor acceptor_;
    FixSessionManager& sessionManager_;

    void handleAccept(const boost::system::error_code& ec, std::shared_ptr<FixConnection> connection);
};

class FixClient {
public:
    using Tcp = boost::asio::ip::tcp;

    FixClient(boost::asio::io_context& ioContext, FixSessionManager& sessionManager);
    void connect(const std::string& host, unsigned short port);
    void close();
    std::shared_ptr<FixConnection> connection() const;

private:
    boost::asio::io_context& ioContext_;
    FixSessionManager& sessionManager_;
    std::shared_ptr<FixConnection> connection_;
};

} // namespace fixed_income
