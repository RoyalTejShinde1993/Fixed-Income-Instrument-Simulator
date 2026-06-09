#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace fixed_income {

static constexpr char kFixDelimiter = '\x01';
static constexpr std::string_view kBeginString = "FIX.4.4";

enum class FixMessageType {
    Unknown,
    Logon,
    Logout,
    Heartbeat,
    TestRequest,
    ResendRequest,
    SequenceReset,
    SecurityDefinition,
    MarketDataSnapshot
};

struct FixField {
    int tag;
    std::string value;
};

class FixMessage {
public:
    FixMessage();
    explicit FixMessage(FixMessageType msgType);

    void setField(int tag, std::string value);
    std::optional<std::string> getField(int tag) const;
    bool hasField(int tag) const noexcept;
    std::string toString() const;
    static FixMessage parse(std::string_view raw);
    bool validateChecksum() const noexcept;
    FixMessageType messageType() const noexcept;

private:
    std::vector<FixField> fields_;
    std::optional<FixMessageType> msgType_;
    mutable std::optional<int> cachedBodyLength_;
    mutable std::optional<int> cachedChecksum_;

    std::string getFieldValue(int tag) const;
    int computeBodyLength() const;
    int computeChecksum() const;
    static FixMessageType msgTypeFromString(std::string_view type);
    static std::string msgTypeToString(FixMessageType type);
};

class FixSession {
public:
    FixSession(std::string senderCompID, std::string targetCompID);

    FixMessage createLogonMessage(int heartbeatSeconds = 30);
    FixMessage createLogoutMessage(std::string text = {});
    FixMessage createHeartbeatMessage(std::string testReqID = {});
    FixMessage createTestRequestMessage(std::string testReqID);
    FixMessage createResendRequestMessage(int beginSeqNo, int endSeqNo);
    FixMessage createSequenceResetMessage(int newSeqNo);
    FixMessage createSecurityDefinitionMessage(std::string securityReqID, std::string symbol, std::string securityType);
    FixMessage createMarketDataSnapshotMessage(std::string symbol, double bid, double ask);

    std::optional<FixMessage> processIncomingMessage(std::string_view rawMessage);
    int expectedIncomingSeq() const noexcept;
    int nextOutgoingSeq() const noexcept;

private:
    std::string senderCompID_;
    std::string targetCompID_;
    int incomingSeqNum_;
    int outgoingSeqNum_;
    std::unordered_map<int, FixMessage> sentMessages_;
    std::unordered_map<int, FixMessage> receivedMessages_;

    void storeSentMessage(int seqNum, const FixMessage& message);
    void storeReceivedMessage(int seqNum, const FixMessage& message);
    FixMessage buildSessionMessage(FixMessageType msgType);
};

} // namespace fixed_income
