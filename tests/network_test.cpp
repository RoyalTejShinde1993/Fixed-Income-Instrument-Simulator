#include "fixed_income/fix.hpp"
#include "fixed_income/network.hpp"

#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <thread>

using namespace fixed_income;

TEST(Network, FixServerAcceptsConnection) {
    boost::asio::io_context serverContext;
    FixSessionManager serverSession("SERVER", "CLIENT");
    FixServer server(serverContext, 15000, serverSession);
    server.startAccept();

    std::thread serverThread([&] { serverContext.run(); });

    boost::asio::io_context clientContext;
    FixSessionManager clientSession("CLIENT", "SERVER");
    FixClient client(clientContext, clientSession);
    client.connect("127.0.0.1", 15000);

    std::thread clientThread([&] { clientContext.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_NE(client.connection(), nullptr);

    server.stop();
    client.close();
    clientContext.stop();
    serverContext.stop();
    clientThread.join();
    serverThread.join();
}

TEST(Network, FixConnectionSendQueueAcceptsMessage) {
    boost::asio::io_context ioContext;
    FixSessionManager sessionManager("SENDER", "TARGET");
    auto connection = sessionManager.createConnection(ioContext);
    FixMessage message = sessionManager.session().createHeartbeatMessage("TESTREQ");
    connection->sendMessage(message);
    EXPECT_NE(connection, nullptr);
}
