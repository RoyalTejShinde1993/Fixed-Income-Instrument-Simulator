#include "fixed_income/fix.hpp"

#include <gtest/gtest.h>

using namespace fixed_income;

TEST(FixMessage, CreatesValidLogonMessage) {
    FixMessage message(FixMessageType::Logon);
    message.setField(49, "SENDER");
    message.setField(56, "TARGET");
    message.setField(34, "1");
    message.setField(52, "20260101-00:00:00.000");

    const std::string text = message.toString();
    EXPECT_NE(text.find("8=FIX.4.4"), std::string::npos);
    EXPECT_NE(text.find("35=A"), std::string::npos);
    EXPECT_TRUE(message.validateChecksum());
}

TEST(FixMessage, ParsesRawMessageAndRecognizesType) {
    std::string raw = "8=FIX.4.4\x01";
    raw += "9=12\x01";
    raw += "35=0\x01";
    raw += "10=000\x01";
    const FixMessage message = FixMessage::parse(raw);
    EXPECT_EQ(message.messageType(), FixMessageType::Heartbeat);
    EXPECT_TRUE(message.hasField(35));
}

TEST(FixSession, SessionCreatesHeartbeatAndLogout) {
    FixSession session("SENDER", "TARGET");
    const FixMessage heartbeat = session.createHeartbeatMessage("TESTREQ");
    EXPECT_EQ(heartbeat.messageType(), FixMessageType::Heartbeat);
    EXPECT_TRUE(heartbeat.hasField(112));

    const FixMessage logout = session.createLogoutMessage("Goodbye");
    EXPECT_EQ(logout.messageType(), FixMessageType::Logout);
    EXPECT_TRUE(logout.hasField(58));
}

TEST(FixSession, ProcessesIncomingMessageAndUpdatesSequence) {
    FixSession session("SENDER", "TARGET");
    std::string raw = "8=FIX.4.4\x01";
    raw += "9=12\x01";
    raw += "35=0\x01";
    raw += "34=1\x01";
    raw += "10=000\x01";

    const auto parsed = session.processIncomingMessage(raw);
    EXPECT_TRUE(parsed.has_value());
    EXPECT_EQ(session.expectedIncomingSeq(), 2);
}
