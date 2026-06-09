#include "fixed_income/fix.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fixed_income {

FixMessage::FixMessage() = default;

FixMessage::FixMessage(FixMessageType msgType) : msgType_(msgType) {
    setField(8, std::string(kBeginString));
    setField(35, msgTypeToString(msgType));
}

void FixMessage::setField(int tag, std::string value) {
    for (auto& field : fields_) {
        if (field.tag == tag) {
            field.value = std::move(value);
            cachedBodyLength_.reset();
            cachedChecksum_.reset();
            return;
        }
    }
    fields_.push_back({tag, std::move(value)});
    cachedBodyLength_.reset();
    cachedChecksum_.reset();
}

std::optional<std::string> FixMessage::getField(int tag) const {
    for (const auto& field : fields_) {
        if (field.tag == tag) {
            return field.value;
        }
    }
    return std::nullopt;
}

bool FixMessage::hasField(int tag) const noexcept {
    return getField(tag).has_value();
}

std::string FixMessage::getFieldValue(int tag) const {
    const auto result = getField(tag);
    if (!result) {
        throw std::out_of_range("Field not found");
    }
    return *result;
}

static std::string buildBodyString(const std::vector<FixField>& fields) {
    std::ostringstream output;
    for (const auto& field : fields) {
        if (field.tag == 8 || field.tag == 9 || field.tag == 10) {
            continue;
        }
        output << field.tag << '=' << field.value << kFixDelimiter;
    }
    return output.str();
}

static int calculateChecksum(std::string_view message) {
    int sum = 0;
    for (unsigned char ch : message) {
        sum += ch;
    }
    return sum % 256;
}

std::string FixMessage::toString() const {
    bool hasBeginString = false;
    bool hasMsgType = false;
    for (const auto& field : fields_) {
        if (field.tag == 8) {
            hasBeginString = true;
        }
        if (field.tag == 35) {
            hasMsgType = true;
        }
    }
    if (!hasBeginString || !hasMsgType) {
        throw std::runtime_error("FIX message missing required header fields.");
    }

    const std::string body = buildBodyString(fields_);
    const int bodyLength = static_cast<int>(body.size());

    std::ostringstream message;
    message << "8=" << kBeginString << kFixDelimiter;
    message << "9=" << bodyLength << kFixDelimiter;
    message << body;
    const std::string headerAndBody = message.str();
    const int checksum = calculateChecksum(headerAndBody);
    message << "10=" << std::setw(3) << std::setfill('0') << checksum << kFixDelimiter;
    return message.str();
}

int FixMessage::computeBodyLength() const {
    if (cachedBodyLength_) {
        return *cachedBodyLength_;
    }
    const std::string body = buildBodyString(fields_);
    cachedBodyLength_ = static_cast<int>(body.size());
    return *cachedBodyLength_;
}

int FixMessage::computeChecksum() const {
    if (cachedChecksum_) {
        return *cachedChecksum_;
    }
    const std::string body = buildBodyString(fields_);
    std::ostringstream message;
    message << "8=" << kBeginString << kFixDelimiter;
    message << "9=" << static_cast<int>(body.size()) << kFixDelimiter;
    message << body;
    cachedChecksum_ = calculateChecksum(message.str());
    return *cachedChecksum_;
}

bool FixMessage::validateChecksum() const noexcept {
    try {
        const std::string raw = toString();
        const auto pos = raw.rfind("10=");
        if (pos == std::string::npos || pos + 6 > raw.size()) {
            return false;
        }
        const std::string checksumStr = raw.substr(pos + 3, 3);
        const int parsed = std::stoi(checksumStr);
        return parsed == computeChecksum();
    } catch (...) {
        return false;
    }
}

FixMessageType FixMessage::messageType() const noexcept {
    return msgType_.value_or(FixMessageType::Unknown);
}

FixMessageType FixMessage::msgTypeFromString(std::string_view type) {
    if (type == "A") return FixMessageType::Logon;
    if (type == "5") return FixMessageType::Logout;
    if (type == "0") return FixMessageType::Heartbeat;
    if (type == "1") return FixMessageType::TestRequest;
    if (type == "2") return FixMessageType::ResendRequest;
    if (type == "4") return FixMessageType::SequenceReset;
    if (type == "d") return FixMessageType::SecurityDefinition;
    if (type == "W") return FixMessageType::MarketDataSnapshot;
    return FixMessageType::Unknown;
}

std::string FixMessage::msgTypeToString(FixMessageType type) {
    switch (type) {
        case FixMessageType::Logon: return "A";
        case FixMessageType::Logout: return "5";
        case FixMessageType::Heartbeat: return "0";
        case FixMessageType::TestRequest: return "1";
        case FixMessageType::ResendRequest: return "2";
        case FixMessageType::SequenceReset: return "4";
        case FixMessageType::SecurityDefinition: return "d";
        case FixMessageType::MarketDataSnapshot: return "W";
        default: return "";
    }
}

FixMessage FixMessage::parse(std::string_view raw) {
    FixMessage message;
    std::size_t start = 0;
    while (start < raw.size()) {
        const auto pos = raw.find(kFixDelimiter, start);
        const auto field = raw.substr(start, pos == std::string_view::npos ? raw.size() - start : pos - start);
        const auto eq = field.find('=');
        if (eq == std::string_view::npos) {
            throw std::invalid_argument("Malformed FIX field.");
        }
        const int tag = std::stoi(std::string(field.substr(0, eq)));
        const std::string value(field.substr(eq + 1));
        message.setField(tag, value);
        if (tag == 35) {
            message.msgType_ = msgTypeFromString(value);
        }
        if (pos == std::string_view::npos) {
            break;
        }
        start = pos + 1;
    }
    return message;
}

FixSession::FixSession(std::string senderCompID, std::string targetCompID)
    : senderCompID_(std::move(senderCompID)),
      targetCompID_(std::move(targetCompID)),
      incomingSeqNum_(1),
      outgoingSeqNum_(1) {
}

FixMessage FixSession::buildSessionMessage(FixMessageType msgType) {
    FixMessage message(msgType);
    message.setField(49, senderCompID_);
    message.setField(56, targetCompID_);
    message.setField(34, std::to_string(outgoingSeqNum_));
    message.setField(52, "20260101-00:00:00.000");
    storeSentMessage(outgoingSeqNum_, message);
    ++outgoingSeqNum_;
    return message;
}

void FixSession::storeSentMessage(int seqNum, const FixMessage& message) {
    sentMessages_.emplace(seqNum, message);
}

void FixSession::storeReceivedMessage(int seqNum, const FixMessage& message) {
    receivedMessages_.emplace(seqNum, message);
}

FixMessage FixSession::createLogonMessage(int heartbeatSeconds) {
    FixMessage message = buildSessionMessage(FixMessageType::Logon);
    message.setField(108, std::to_string(heartbeatSeconds));
    message.setField(98, "0");
    return message;
}

FixMessage FixSession::createLogoutMessage(std::string text) {
    FixMessage message = buildSessionMessage(FixMessageType::Logout);
    if (!text.empty()) {
        message.setField(58, std::move(text));
    }
    return message;
}

FixMessage FixSession::createHeartbeatMessage(std::string testReqID) {
    FixMessage message = buildSessionMessage(FixMessageType::Heartbeat);
    if (!testReqID.empty()) {
        message.setField(112, std::move(testReqID));
    }
    return message;
}

FixMessage FixSession::createTestRequestMessage(std::string testReqID) {
    FixMessage message = buildSessionMessage(FixMessageType::TestRequest);
    message.setField(112, std::move(testReqID));
    return message;
}

FixMessage FixSession::createResendRequestMessage(int beginSeqNo, int endSeqNo) {
    FixMessage message = buildSessionMessage(FixMessageType::ResendRequest);
    message.setField(7, std::to_string(beginSeqNo));
    message.setField(16, std::to_string(endSeqNo));
    return message;
}

FixMessage FixSession::createSequenceResetMessage(int newSeqNo) {
    FixMessage message = buildSessionMessage(FixMessageType::SequenceReset);
    message.setField(36, std::to_string(newSeqNo));
    return message;
}

FixMessage FixSession::createSecurityDefinitionMessage(std::string securityReqID, std::string symbol, std::string securityType) {
    FixMessage message = buildSessionMessage(FixMessageType::SecurityDefinition);
    message.setField(320, std::move(securityReqID));
    message.setField(55, std::move(symbol));
    message.setField(167, std::move(securityType));
    return message;
}

FixMessage FixSession::createMarketDataSnapshotMessage(std::string symbol, double bid, double ask) {
    FixMessage message = buildSessionMessage(FixMessageType::MarketDataSnapshot);
    message.setField(55, std::move(symbol));
    message.setField(132, std::to_string(bid));
    message.setField(133, std::to_string(ask));
    return message;
}

std::optional<FixMessage> FixSession::processIncomingMessage(std::string_view rawMessage) {
    const FixMessage message = FixMessage::parse(rawMessage);
    const auto seqNoValue = message.getField(34);
    if (!seqNoValue) {
        return std::nullopt;
    }
    const int seqNo = std::stoi(*seqNoValue);
    storeReceivedMessage(seqNo, message);
    incomingSeqNum_ = seqNo + 1;
    return message;
}

int FixSession::expectedIncomingSeq() const noexcept {
    return incomingSeqNum_;
}

int FixSession::nextOutgoingSeq() const noexcept {
    return outgoingSeqNum_;
}

} // namespace fixed_income
