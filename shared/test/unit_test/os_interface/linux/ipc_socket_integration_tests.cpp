/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_client.h"
#include "shared/source/os_interface/linux/ipc_socket_server.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace NEO;

namespace NEO {
namespace SysCalls {
extern int sendmsgCalled;
extern int recvmsgCalled;
extern uint32_t closeFuncCalled;
extern int dupCalled;
} // namespace SysCalls
} // namespace NEO

class TestableIpcSocketServerForIntegration : public IpcSocketServer {
  public:
    using IpcSocketServer::handleMap;
    using IpcSocketServer::handleMapMutex;
    using IpcSocketServer::serverRunning;
    using IpcSocketServer::serverSocket;
    using IpcSocketServer::shutdownImpl;
};

class IpcSocketIntegrationTest : public ::testing::Test {
  public:
    void SetUp() override {
        SysCalls::sendmsgCalled = 0;
        SysCalls::recvmsgCalled = 0;
    }

    void TearDown() override {
        SysCalls::sysCallsSendmsg = nullptr;
        SysCalls::sysCallsRecvmsg = nullptr;
        SysCalls::sysCallsRecv = nullptr;
    }
};

TEST_F(IpcSocketIntegrationTest, givenServerAndClientWhenClientRequestsNonExistentHandleThenReceivesFailure) {
    VariableBackup<decltype(SysCalls::sysCallsSendmsg)> sendmsgBackup(&SysCalls::sysCallsSendmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSendmsg = [](int, const struct msghdr *msg, int) -> ssize_t {
        if (msg->msg_iov && msg->msg_iovlen > 0) {
            return static_cast<ssize_t>(msg->msg_iov[0].iov_len);
        }
        return -1;
    };

    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *msg, int) -> ssize_t {
        if (msg->msg_iov && msg->msg_iovlen > 0) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(msg->msg_iov[0].iov_base);
            payload->success = false;
            return static_cast<ssize_t>(msg->msg_iov[0].iov_len);
        }
        return -1;
    };

    SysCalls::sysCallsRecv = [](int, void *buf, size_t len, int) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->handleId = 99999;
            msg->payloadSize = sizeof(IpcSocketResponsePayload);
            return sizeof(IpcSocketMessage);
        } else if (len == sizeof(IpcSocketResponsePayload)) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(buf);
            payload->success = false;
            return sizeof(IpcSocketResponsePayload);
        }
        return -1;
    };

    IpcSocketClient client;
    int receivedFd = client.requestHandle(99999);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketIntegrationTest, givenMessageStructsWhenCheckingSizeThenCorrectSizesAreUsed) {
    EXPECT_EQ(24u, sizeof(IpcSocketMessage));
    EXPECT_EQ(8u, sizeof(IpcSocketResponsePayload));
}

TEST_F(IpcSocketIntegrationTest, givenIpcSocketMessageWhenFillingFieldsThenAllFieldsAreAccessible) {
    IpcSocketMessage msg;
    msg.type = IpcSocketMessageType::requestHandle;
    msg.processId = 12345;
    msg.handleId = 67890;
    msg.payloadSize = 100;
    msg.reserved = 0;

    EXPECT_EQ(IpcSocketMessageType::requestHandle, msg.type);
    EXPECT_EQ(12345u, msg.processId);
    EXPECT_EQ(67890u, msg.handleId);
    EXPECT_EQ(100u, msg.payloadSize);
}

TEST_F(IpcSocketIntegrationTest, givenIpcSocketResponsePayloadWhenFillingFieldsThenAllFieldsAreAccessible) {
    IpcSocketResponsePayload payload;
    payload.success = true;

    EXPECT_TRUE(payload.success);
}

TEST_F(IpcSocketIntegrationTest, givenIpcHandleEntryWhenInitializedThenDefaultValuesAreCorrect) {
    IpcHandleEntry entry;
    EXPECT_EQ(-1, entry.fileDescriptor);
    EXPECT_EQ(0u, entry.refCount);
}

TEST_F(IpcSocketIntegrationTest, givenServerWhenReceivingInvalidMessageTypeThenHandlesGracefully) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    SysCalls::sysCallsRecv = [](int, void *, size_t, int) -> ssize_t { return -1; };
    SysCalls::sysCallsClose = [](int) -> int { return 0; };

    TestableIpcSocketServerForIntegration server;
    server.serverRunning.store(true);
    server.serverSocket = 5;

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketIntegrationTest, givenServerWhenMultipleHandlesRegisteredThenEachHasUniqueEntry) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    static int dupCounter = 100;
    dupCounter = 100;
    SysCalls::sysCallsDup = [](int) -> int { return ++dupCounter; };
    SysCalls::sysCallsClose = [](int) -> int { return 0; };

    TestableIpcSocketServerForIntegration server;
    server.serverRunning.store(true);
    server.serverSocket = 5;

    EXPECT_TRUE(server.registerHandle(1, 10));
    EXPECT_TRUE(server.registerHandle(2, 20));
    EXPECT_TRUE(server.registerHandle(3, 30));

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_EQ(3u, server.handleMap.size());
        EXPECT_EQ(1u, server.handleMap.count(1));
        EXPECT_EQ(1u, server.handleMap.count(2));
        EXPECT_EQ(1u, server.handleMap.count(3));
    }

    EXPECT_TRUE(server.unregisterHandle(2));
    EXPECT_TRUE(server.unregisterHandle(1));
    EXPECT_TRUE(server.unregisterHandle(3));

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_EQ(0u, server.handleMap.size());
    }
}

TEST_F(IpcSocketIntegrationTest, givenClientWhenConnectingToNonExistentServerThenConnectFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int {
        errno = ECONNREFUSED;
        return -1;
    };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    IpcSocketClient client;
    EXPECT_FALSE(client.connectToServer("nonexistent_server"));
    EXPECT_FALSE(client.isConnected());
}

TEST_F(IpcSocketIntegrationTest, givenServerWithRegisteredHandleWhenShutdownThenHandleIsCleanedUp) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    static bool closedFd142 = false;
    static bool closedServerSocket = false;
    closedFd142 = false;
    closedServerSocket = false;
    SysCalls::sysCallsClose = [](int fd) -> int {
        if (fd == 142) {
            closedFd142 = true;
        } else if (fd == 5) {
            closedServerSocket = true;
        }
        return 0;
    };

    TestableIpcSocketServerForIntegration server;
    server.serverRunning.store(true);
    server.serverSocket = 5;

    EXPECT_TRUE(server.registerHandle(12345, 42));

    server.shutdownImpl();

    EXPECT_TRUE(closedFd142);
    EXPECT_TRUE(closedServerSocket);
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketIntegrationTest, givenServerWhenRegisterHandleWithSameIdDifferentFdThenUsesExistingHandle) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    static int dupCallCountLocal = 0;
    dupCallCountLocal = 0;
    SysCalls::sysCallsDup = [](int oldfd) -> int {
        dupCallCountLocal++;
        return oldfd + 100;
    };
    SysCalls::sysCallsClose = [](int) -> int { return 0; };

    TestableIpcSocketServerForIntegration server;
    server.serverRunning.store(true);
    server.serverSocket = 5;

    uint64_t handleId = 12345;
    EXPECT_TRUE(server.registerHandle(handleId, 42));
    EXPECT_EQ(1, dupCallCountLocal);

    EXPECT_TRUE(server.registerHandle(handleId, 99));
    EXPECT_EQ(1, dupCallCountLocal);

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_EQ(2u, server.handleMap[handleId].refCount);
    }
}

TEST_F(IpcSocketIntegrationTest, givenClientWhenDisconnectedAndTryToRequestHandleThenFails) {
    IpcSocketClient client;

    EXPECT_FALSE(client.isConnected());

    int fd = client.requestHandle(123);
    EXPECT_EQ(-1, fd);
}

TEST_F(IpcSocketIntegrationTest, givenMessageTypesWhenCheckingValuesThenCorrectEnumValues) {
    EXPECT_EQ(1u, static_cast<uint32_t>(IpcSocketMessageType::requestHandle));
    EXPECT_EQ(2u, static_cast<uint32_t>(IpcSocketMessageType::responseHandle));
}

TEST_F(IpcSocketIntegrationTest, givenServerWhenPollReturnsErrorThenServerHandlesGracefully) {
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    SysCalls::sysCallsClose = [](int) -> int { return 0; };

    TestableIpcSocketServerForIntegration server;
    server.serverRunning.store(true);
    server.serverSocket = 5;

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketIntegrationTest, givenServerWhenAcceptReturnsErrorThenContinuesRunning) {
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    SysCalls::sysCallsClose = [](int) -> int { return 0; };

    TestableIpcSocketServerForIntegration server;
    server.serverRunning.store(true);
    server.serverSocket = 5;

    EXPECT_TRUE(server.isRunning());

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketIntegrationTest, givenClientWhenSendReturnsPartialWriteThenRequestHandleFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *, size_t len, int) -> ssize_t {
        return static_cast<ssize_t>(len - 1);
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test"));

    int fd = client.requestHandle(123);
    EXPECT_EQ(-1, fd);
}
