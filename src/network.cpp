#include "fixed_income/network.hpp"

#include <stdexcept>

namespace fixed_income {

FixConnection::FixConnection(boost::asio::io_context& ioContext, FixSessionManager& sessionManager)
    : ioContext_(ioContext),
      socket_(ioContext),
      sessionManager_(sessionManager),
      readBuffer_(kReadBufferSize) {
}

FixConnection::~FixConnection() {
    close();
}

FixConnection::Tcp::socket& FixConnection::socket() {
    return socket_;
}

void FixConnection::start() {
    doRead();
}

void FixConnection::sendMessage(const FixMessage& message) {
    const std::string payload = message.toString();
    bool writeInProgress = !writeQueue_.empty();
    writeQueue_.push_back(payload);
    if (!writeInProgress) {
        doWrite();
    }
}

void FixConnection::close() {
    boost::system::error_code ec;
    socket_.close(ec);
}

void FixConnection::setMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

void FixConnection::setErrorHandler(ErrorHandler handler) {
    errorHandler_ = std::move(handler);
}

void FixConnection::doRead() {
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(readBuffer_),
        [self](const boost::system::error_code& ec, std::size_t bytesTransferred) {
            self->handleRead(ec, bytesTransferred);
        });
}

void FixConnection::handleRead(const boost::system::error_code& ec, std::size_t bytesTransferred) {
    if (ec) {
        if (errorHandler_) {
            errorHandler_(ec);
        }
        return;
    }

    incomingBuffer_.append(readBuffer_.data(), bytesTransferred);
    processIncomingData();
    doRead();
}

void FixConnection::doWrite() {
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(writeQueue_.front()),
        [self](const boost::system::error_code& ec, std::size_t bytesTransferred) {
            self->handleWrite(ec, bytesTransferred);
        });
}

void FixConnection::handleWrite(const boost::system::error_code& ec, std::size_t) {
    if (ec) {
        if (errorHandler_) {
            errorHandler_(ec);
        }
        return;
    }

    writeQueue_.pop_front();
    if (!writeQueue_.empty()) {
        doWrite();
    }
}

static std::optional<std::size_t> findCompleteFixMessage(const std::string& buffer) {
    if (buffer.size() < 10) {
        return std::nullopt;
    }
    if (buffer.rfind("8=FIX.4.4", 0) != 0) {
        return std::nullopt;
    }

    const auto headerEnd = buffer.find(kFixDelimiter, 0);
    if (headerEnd == std::string::npos) {
        return std::nullopt;
    }

    const auto bodyLengthTag = std::string("9=");
    const auto bodyLengthPos = buffer.find(bodyLengthTag, headerEnd + 1);
    if (bodyLengthPos == std::string::npos) {
        return std::nullopt;
    }
    const auto bodyLengthEnd = buffer.find(kFixDelimiter, bodyLengthPos);
    if (bodyLengthEnd == std::string::npos) {
        return std::nullopt;
    }

    const auto lengthStart = bodyLengthPos + bodyLengthTag.size();
    const auto lengthText = buffer.substr(lengthStart, bodyLengthEnd - lengthStart);
    if (lengthText.empty() || lengthText.find_first_not_of("0123456789") != std::string::npos) {
        return std::nullopt;
    }

    const std::size_t bodyLength = static_cast<std::size_t>(std::stoul(lengthText));
    const std::size_t bodyStart = bodyLengthEnd + 1;
    const std::size_t checksumFieldSize = 7; // 10=###<SOH>
    const std::size_t messageEnd = bodyStart + bodyLength + checksumFieldSize;
    if (buffer.size() < messageEnd) {
        return std::nullopt;
    }
    if (buffer.substr(messageEnd - checksumFieldSize, 3) != "10=") {
        return std::nullopt;
    }

    return messageEnd;
}

void FixConnection::processIncomingData() {
    while (true) {
        const auto messageLength = findCompleteFixMessage(incomingBuffer_);
        if (!messageLength.has_value()) {
            break;
        }

        const std::string rawMessage = incomingBuffer_.substr(0, *messageLength);
        incomingBuffer_.erase(0, *messageLength);
        try {
            const FixMessage message = FixMessage::parse(rawMessage);
            if (messageHandler_) {
                messageHandler_(message);
            }
        } catch (...) {
            if (errorHandler_) {
                errorHandler_(boost::asio::error::operation_aborted);
            }
        }
    }
}

FixSessionManager::FixSessionManager(std::string senderCompID, std::string targetCompID)
    : session_(std::move(senderCompID), std::move(targetCompID)) {
}

std::shared_ptr<FixConnection> FixSessionManager::createConnection(boost::asio::io_context& ioContext) {
    return std::make_shared<FixConnection>(ioContext, *this);
}

FixSession& FixSessionManager::session() noexcept {
    return session_;
}

FixServer::FixServer(boost::asio::io_context& ioContext, unsigned short port, FixSessionManager& sessionManager)
    : ioContext_(ioContext),
      acceptor_(ioContext, Tcp::endpoint(Tcp::v4(), port)),
      sessionManager_(sessionManager) {
}

void FixServer::startAccept() {
    auto connection = sessionManager_.createConnection(ioContext_);
    acceptor_.async_accept(connection->socket(),
        [this, connection](const boost::system::error_code& ec) {
            handleAccept(ec, connection);
        });
}

void FixServer::handleAccept(const boost::system::error_code& ec, std::shared_ptr<FixConnection> connection) {
    if (!ec) {
        connection->start();
    }
    startAccept();
}

void FixServer::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
}

FixClient::FixClient(boost::asio::io_context& ioContext, FixSessionManager& sessionManager)
    : ioContext_(ioContext),
      sessionManager_(sessionManager) {
}

void FixClient::connect(const std::string& host, unsigned short port) {
    connection_ = sessionManager_.createConnection(ioContext_);
    boost::asio::ip::tcp::resolver resolver(ioContext_);
    auto endpoints = resolver.resolve(host, std::to_string(port));
    boost::asio::async_connect(connection_->socket(), endpoints,
        [self = connection_](const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint&) {
            if (!ec) {
                self->start();
            }
        });
}

void FixClient::close() {
    if (connection_) {
        connection_->close();
    }
}

std::shared_ptr<FixConnection> FixClient::connection() const {
    return connection_;
}

} // namespace fixed_income
