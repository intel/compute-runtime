/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_server.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include <errno.h>
#include <mutex>
#include <poll.h>

using namespace NEO;

namespace NEO {
namespace SysCalls {
extern int socketCalled;
extern int bindCalled;
extern int listenCalled;
extern int acceptCalled;
extern uint32_t closeFuncCalled;
extern int sendCalled;
extern int recvCalled;
extern int sendmsgCalled;
extern int recvmsgCalled;
extern int setsockoptCalled;
extern int dupCalled;
extern uint32_t pollFuncCalled;
} // namespace SysCalls
} // namespace NEO

class TestableIpcSocketServer : public IpcSocketServer {
  public:
    using IpcSocketServer::handleClientConnection;
    using IpcSocketServer::handleMap;
    using IpcSocketServer::handleMapMutex;
    using IpcSocketServer::processRequestMessage;
    using IpcSocketServer::receiveMessage;
    using IpcSocketServer::sendFileDescriptor;
    using IpcSocketServer::sendMessage;
    using IpcSocketServer::serverRunning;
    using IpcSocketServer::serverSocket;
    using IpcSocketServer::serverThreadEntry;
    using IpcSocketServer::serverThreadRun;
    using IpcSocketServer::shutdownImpl;
    using IpcSocketServer::shutdownRequested;
};

class IpcSocketServerTest : public ::testing::Test {
  public:
    void SetUp() override {
        SysCalls::socketCalled = 0;
        SysCalls::bindCalled = 0;
        SysCalls::listenCalled = 0;
        SysCalls::acceptCalled = 0;
        SysCalls::closeFuncCalled = 0;
        SysCalls::sendCalled = 0;
        SysCalls::recvCalled = 0;
        SysCalls::sendmsgCalled = 0;
        SysCalls::recvmsgCalled = 0;
        SysCalls::setsockoptCalled = 0;
        SysCalls::dupCalled = 0;
        SysCalls::pollFuncCalled = 0;
    }

    void TearDown() override {
        SysCalls::sysCallsSocket = nullptr;
        SysCalls::sysCallsBind = nullptr;
        SysCalls::sysCallsListen = nullptr;
        SysCalls::sysCallsAccept = nullptr;
        SysCalls::sysCallsSend = nullptr;
        SysCalls::sysCallsRecv = nullptr;
        SysCalls::sysCallsSendmsg = nullptr;
        SysCalls::sysCallsRecvmsg = nullptr;
        SysCalls::sysCallsSetsockopt = nullptr;
        SysCalls::sysCallsDup = nullptr;
        SysCalls::sysCallsPoll = nullptr;
    }
};

TEST_F(IpcSocketServerTest, givenValidServerWhenConstructedThenSocketPathIsSet) {
    IpcSocketServer server;

    auto socketPath = server.getSocketPath();
    EXPECT_FALSE(socketPath.empty());
    EXPECT_TRUE(socketPath.find("neo_ipc_") != std::string::npos);
}

TEST_F(IpcSocketServerTest, givenServerWhenSocketPathContainsPidThenPathIsValid) {
    IpcSocketServer server;
    auto path = server.getSocketPath();

    EXPECT_FALSE(path.empty());
    EXPECT_NE(path.find("neo_ipc_"), std::string::npos);

    bool hasDigit = false;
    for (char c : path) {
        if (std::isdigit(c)) {
            hasDigit = true;
            break;
        }
    }
    EXPECT_TRUE(hasDigit);
}

TEST_F(IpcSocketServerTest, givenServerWhenDestructorCalledBeforeInitializeThenNoError) {
    IpcSocketServer server;
}

TEST_F(IpcSocketServerTest, givenNonRunningServerWhenShutdownCalledThenEarlyReturns) {
    IpcSocketServer server;

    EXPECT_FALSE(server.isRunning());

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerTest, givenServerWhenSendMessageWithNullPayloadDirectlyThenSkipsPayloadSend) {
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static int sendCallCountLocal = 0;
    sendCallCountLocal = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCountLocal++;
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    msg.payloadSize = 100;

    bool result = server.sendMessage(10, msg, nullptr);
    EXPECT_TRUE(result);
    EXPECT_EQ(1, sendCallCountLocal);
}

TEST_F(IpcSocketServerTest, givenServerWhenSendMessageWithZeroPayloadSizeDirectlyThenSkipsPayloadSend) {
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static int sendCallCountLocal = 0;
    sendCallCountLocal = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCountLocal++;
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    msg.payloadSize = 0;
    char payload[10] = {};

    bool result = server.sendMessage(10, msg, payload);
    EXPECT_TRUE(result);
    EXPECT_EQ(1, sendCallCountLocal);
}

TEST_F(IpcSocketServerTest, givenServerWhenSendMessageWithValidPayloadDirectlyThenSendsPayload) {
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static int sendCallCountLocal = 0;
    sendCallCountLocal = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCountLocal++;
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    msg.payloadSize = 10;
    char payload[10] = {};

    bool result = server.sendMessage(10, msg, payload);
    EXPECT_TRUE(result);
    EXPECT_EQ(2, sendCallCountLocal);
}

TEST_F(IpcSocketServerTest, givenServerWhenReceiveMessageWithNullPayloadDirectlyThenSkipsPayloadRecv) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    static int recvCallCountLocal = 0;
    recvCallCountLocal = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCountLocal++;
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->payloadSize = 100;
            return sizeof(IpcSocketMessage);
        }
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};

    bool result = server.receiveMessage(10, msg, nullptr, 100);
    EXPECT_TRUE(result);
    EXPECT_EQ(1, recvCallCountLocal);
}

TEST_F(IpcSocketServerTest, givenServerWhenReceiveMessageWithZeroPayloadSizeDirectlyThenSkipsPayloadRecv) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    static int recvCallCountLocal = 0;
    recvCallCountLocal = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCountLocal++;
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->payloadSize = 0;
            return sizeof(IpcSocketMessage);
        }
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    char payload[100] = {};

    bool result = server.receiveMessage(10, msg, payload, sizeof(payload));
    EXPECT_TRUE(result);
    EXPECT_EQ(1, recvCallCountLocal);
}

TEST_F(IpcSocketServerTest, givenServerWhenReceiveMessageWithValidPayloadDirectlyThenReceivesPayload) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    static int recvCallCountLocal = 0;
    recvCallCountLocal = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCountLocal++;
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->payloadSize = 10;
            return sizeof(IpcSocketMessage);
        }
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    char payload[100] = {};

    bool result = server.receiveMessage(10, msg, payload, sizeof(payload));
    EXPECT_TRUE(result);
    EXPECT_EQ(2, recvCallCountLocal);
}

TEST_F(IpcSocketServerTest, givenServerWhenReceiveMessageWithTooSmallBufferDirectlyThenFails) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    static int recvCallCountLocal = 0;
    recvCallCountLocal = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCountLocal++;
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->payloadSize = 100;
            return sizeof(IpcSocketMessage);
        }
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    char payload[10] = {};

    bool result = server.receiveMessage(10, msg, payload, sizeof(payload));
    EXPECT_FALSE(result);
    EXPECT_EQ(1, recvCallCountLocal);
}

TEST_F(IpcSocketServerTest, givenServerWhenReceiveMessageWithValidPayloadAndPayloadRecvFailsDirectlyThenFails) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    static int recvCallCountLocal = 0;
    recvCallCountLocal = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCountLocal++;
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->payloadSize = 10;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    char payload[100] = {};

    bool result = server.receiveMessage(10, msg, payload, sizeof(payload));
    EXPECT_FALSE(result);
    EXPECT_EQ(2, recvCallCountLocal);
}

TEST_F(IpcSocketServerTest, givenServerWhenShutdownWithServerSocketMinusOneThenSkipsClose) {
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    static int closeCallCountLocal = 0;
    closeCallCountLocal = 0;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closeCallCountLocal++;
        return 0;
    };

    TestableIpcSocketServer server;

    server.serverRunning.store(true);
    server.serverSocket = -1;

    server.shutdown();

    EXPECT_FALSE(server.serverRunning.load());
    EXPECT_EQ(0, closeCallCountLocal);
}

TEST_F(IpcSocketServerTest, givenServerWhenProcessRequestForHandleWithFdMinusOneThenSendsFailure) {
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static bool sendWasCalled = false;
    static IpcSocketResponsePayload lastPayload{};
    sendWasCalled = false;

    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        sendWasCalled = true;
        if (len == sizeof(IpcSocketMessage)) {
        } else if (len == sizeof(IpcSocketResponsePayload)) {
            lastPayload = *static_cast<const IpcSocketResponsePayload *>(buf);
        }
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        IpcHandleEntry entry;
        entry.fileDescriptor = -1;
        entry.refCount = 1;
        server.handleMap[12345] = entry;
    }

    IpcSocketMessage msg{};
    msg.type = IpcSocketMessageType::requestHandle;
    msg.handleId = 12345;
    msg.payloadSize = 0;

    bool result = server.processRequestMessage(10, msg);

    EXPECT_TRUE(result);
    EXPECT_TRUE(sendWasCalled);
    EXPECT_FALSE(lastPayload.success);
}

TEST_F(IpcSocketServerTest, givenServerWhenUnregisterHandleDirectlyWithRefCountBecomingZeroThenRemovesHandle) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    static int closeCallCountLocal = 0;
    closeCallCountLocal = 0;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closeCallCountLocal++;
        return 0;
    };

    TestableIpcSocketServer server;

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        IpcHandleEntry entry;
        entry.fileDescriptor = 42;
        entry.refCount = 1;
        server.handleMap[12345] = entry;
    }

    EXPECT_TRUE(server.unregisterHandle(12345));

    EXPECT_EQ(1, closeCallCountLocal);

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_EQ(0u, server.handleMap.count(12345));
    }
}

TEST_F(IpcSocketServerTest, givenServerWhenUnregisterHandleDirectlyWithRefCountStillPositiveThenKeepsHandle) {
    TestableIpcSocketServer server;

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        IpcHandleEntry entry;
        entry.fileDescriptor = 42;
        entry.refCount = 2;
        server.handleMap[12345] = entry;
    }

    EXPECT_TRUE(server.unregisterHandle(12345));

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_EQ(1u, server.handleMap.count(12345));
        EXPECT_EQ(1u, server.handleMap[12345].refCount);
    }
}

TEST_F(IpcSocketServerTest, givenServerWhenShutdownWithValidServerSocketThenClosesSocket) {
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    static int closeCallCountLocal = 0;
    static int closedSocketFd = -1;
    closeCallCountLocal = 0;
    closedSocketFd = -1;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closeCallCountLocal++;
        closedSocketFd = fd;
        return 0;
    };

    TestableIpcSocketServer server;

    server.serverRunning.store(true);
    server.serverSocket = 42;

    server.shutdown();

    EXPECT_FALSE(server.serverRunning.load());
    EXPECT_GE(closeCallCountLocal, 1);
    EXPECT_EQ(42, closedSocketFd);
}

TEST_F(IpcSocketServerTest, givenServerWhenHandleClientConnectionReceiveFailsThenReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        return -1;
    };

    TestableIpcSocketServer server;

    bool result = server.handleClientConnection(10);
    EXPECT_FALSE(result);
}

TEST_F(IpcSocketServerTest, givenServerWhenHandleClientConnectionReceivesRequestHandleThenProcessesIt) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->processId = 12345;
            msg->handleId = 99999;
            msg->payloadSize = 0;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    static bool sendCalled = false;
    sendCalled = false;
    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        sendCalled = true;
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    bool result = server.handleClientConnection(10);
    EXPECT_TRUE(result);
    EXPECT_TRUE(sendCalled);
}

TEST_F(IpcSocketServerTest, givenServerWhenHandleClientConnectionReceivesUnknownMessageTypeThenReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = static_cast<IpcSocketMessageType>(999);
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = 0;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    TestableIpcSocketServer server;

    bool result = server.handleClientConnection(10);
    EXPECT_FALSE(result);
}

TEST_F(IpcSocketServerTest, givenServerWhenProcessRequestForExistingHandleWithValidFdThenSendsFdViaScmRights) {
    VariableBackup<decltype(SysCalls::sysCallsSendmsg)> sendmsgBackup(&SysCalls::sysCallsSendmsg);

    static bool sendmsgCalled = false;
    sendmsgCalled = false;
    SysCalls::sysCallsSendmsg = [](int sockfd, const struct msghdr *msg, int flags) -> ssize_t {
        sendmsgCalled = true;
        return msg->msg_iov ? static_cast<ssize_t>(msg->msg_iov[0].iov_len) : 0;
    };

    TestableIpcSocketServer server;

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        IpcHandleEntry entry;
        entry.fileDescriptor = 42;
        entry.refCount = 1;
        server.handleMap[12345] = entry;
    }

    IpcSocketMessage msg{};
    msg.type = IpcSocketMessageType::requestHandle;
    msg.handleId = 12345;
    msg.payloadSize = 0;

    bool result = server.processRequestMessage(10, msg);

    EXPECT_TRUE(result);
    EXPECT_TRUE(sendmsgCalled);
}

TEST_F(IpcSocketServerTest, givenServerWhenProcessRequestForNonExistentHandleThenSendsFailureResponse) {
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static bool sendCalled = false;
    static IpcSocketResponsePayload lastPayload{};
    sendCalled = false;
    lastPayload = {};

    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        sendCalled = true;
        if (len == sizeof(IpcSocketResponsePayload)) {
            lastPayload = *static_cast<const IpcSocketResponsePayload *>(buf);
        }
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    msg.type = IpcSocketMessageType::requestHandle;
    msg.handleId = 99999;
    msg.payloadSize = 0;

    bool result = server.processRequestMessage(10, msg);

    EXPECT_TRUE(result);
    EXPECT_TRUE(sendCalled);
    EXPECT_FALSE(lastPayload.success);
}

TEST_F(IpcSocketServerTest, givenServerWhenSendFileDescriptorSucceedsThenReturnsTrue) {
    VariableBackup<decltype(SysCalls::sysCallsSendmsg)> sendmsgBackup(&SysCalls::sysCallsSendmsg);

    static bool sendmsgCalled = false;
    sendmsgCalled = false;
    SysCalls::sysCallsSendmsg = [](int sockfd, const struct msghdr *msg, int flags) -> ssize_t {
        sendmsgCalled = true;
        return msg->msg_iov ? static_cast<ssize_t>(msg->msg_iov[0].iov_len) : 0;
    };

    TestableIpcSocketServer server;

    IpcSocketResponsePayload payload{};
    payload.success = true;

    bool result = server.sendFileDescriptor(10, 42, &payload, sizeof(payload));

    EXPECT_TRUE(result);
    EXPECT_TRUE(sendmsgCalled);
}

TEST_F(IpcSocketServerTest, givenServerWhenSendFileDescriptorFailsThenReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsSendmsg)> sendmsgBackup(&SysCalls::sysCallsSendmsg);

    SysCalls::sysCallsSendmsg = [](int sockfd, const struct msghdr *msg, int flags) -> ssize_t {
        return -1;
    };

    TestableIpcSocketServer server;

    IpcSocketResponsePayload payload{};
    payload.success = true;

    bool result = server.sendFileDescriptor(10, 42, &payload, sizeof(payload));

    EXPECT_FALSE(result);
}

TEST_F(IpcSocketServerTest, givenServerWhenSendMessageHeaderFailsThenReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        return -1;
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    msg.payloadSize = 0;

    bool result = server.sendMessage(10, msg, nullptr);

    EXPECT_FALSE(result);
}

TEST_F(IpcSocketServerTest, givenServerWhenSendMessagePayloadFailsThenReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static int sendCallCount = 0;
    sendCallCount = 0;
    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        sendCallCount++;
        if (sendCallCount == 1) {
            return static_cast<ssize_t>(len);
        }
        return -1;
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};
    msg.payloadSize = 10;
    char payload[10] = {};

    bool result = server.sendMessage(10, msg, payload);

    EXPECT_FALSE(result);
    EXPECT_EQ(2, sendCallCount);
}

TEST_F(IpcSocketServerTest, givenServerWhenReceiveMessageHeaderFailsThenReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        return -1;
    };

    TestableIpcSocketServer server;

    IpcSocketMessage msg{};

    bool result = server.receiveMessage(10, msg, nullptr, 0);

    EXPECT_FALSE(result);
}

TEST_F(IpcSocketServerTest, givenServerWhenRegisterHandleWithExistingHandleThenIncrementsRefCount) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    static int dupCallCount = 0;
    dupCallCount = 0;
    SysCalls::sysCallsDup = [](int oldfd) -> int {
        dupCallCount++;
        return oldfd + 100;
    };

    TestableIpcSocketServer server;

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        IpcHandleEntry entry;
        entry.fileDescriptor = 142;
        entry.refCount = 1;
        server.handleMap[12345] = entry;
    }

    bool result = server.registerHandle(12345, 42);

    EXPECT_TRUE(result);
    EXPECT_EQ(0, dupCallCount);

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_EQ(2u, server.handleMap[12345].refCount);
    }
}

TEST_F(IpcSocketServerTest, givenServerWhenRegisterHandleWithNewHandleAndDupFailsThenReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsDup = [](int oldfd) -> int {
        return -1;
    };

    TestableIpcSocketServer server;

    bool result = server.registerHandle(12345, 42);

    EXPECT_FALSE(result);

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_EQ(0u, server.handleMap.count(12345));
    }
}

TEST_F(IpcSocketServerTest, givenServerWhenRegisterHandleWithNewHandleAndDupSucceedsThenReturnsTrue) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsDup = [](int oldfd) -> int {
        return oldfd + 100;
    };

    TestableIpcSocketServer server;

    bool result = server.registerHandle(12345, 42);

    EXPECT_TRUE(result);

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_EQ(1u, server.handleMap.count(12345));
        EXPECT_EQ(142, server.handleMap[12345].fileDescriptor);
        EXPECT_EQ(1u, server.handleMap[12345].refCount);
    }
}

TEST_F(IpcSocketServerTest, givenServerWhenUnregisterHandleNotFoundThenReturnsFalse) {
    TestableIpcSocketServer server;

    bool result = server.unregisterHandle(99999);

    EXPECT_FALSE(result);
}

TEST_F(IpcSocketServerTest, givenServerWhenUnregisterHandleWithFdMinusOneAndRefCountZeroThenRemovesWithoutClose) {
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    static int closeCallCount = 0;
    closeCallCount = 0;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closeCallCount++;
        return 0;
    };

    TestableIpcSocketServer server;

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        IpcHandleEntry entry;
        entry.fileDescriptor = -1;
        entry.refCount = 1;
        server.handleMap[12345] = entry;
    }

    EXPECT_TRUE(server.unregisterHandle(12345));

    EXPECT_EQ(0, closeCallCount);

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_EQ(0u, server.handleMap.count(12345));
    }
}

TEST_F(IpcSocketServerTest, givenServerWhenShutdownImplWithRegisteredHandlesThenClosesAllFds) {
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    static int closeCallCount = 0;
    static bool closedServerSocket = false;
    static bool closedFd42 = false;
    static bool closedFd43 = false;
    closeCallCount = 0;
    closedServerSocket = false;
    closedFd42 = false;
    closedFd43 = false;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closeCallCount++;
        if (fd == 100) {
            closedServerSocket = true;
        } else if (fd == 42) {
            closedFd42 = true;
        } else if (fd == 43) {
            closedFd43 = true;
        }
        return 0;
    };

    TestableIpcSocketServer server;

    server.serverRunning.store(true);
    server.serverSocket = 100;

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        IpcHandleEntry entry1;
        entry1.fileDescriptor = 42;
        entry1.refCount = 1;
        server.handleMap[1] = entry1;

        IpcHandleEntry entry2;
        entry2.fileDescriptor = 43;
        entry2.refCount = 1;
        server.handleMap[2] = entry2;

        IpcHandleEntry entry3;
        entry3.fileDescriptor = -1;
        entry3.refCount = 1;
        server.handleMap[3] = entry3;
    }

    server.shutdownImpl();

    EXPECT_FALSE(server.serverRunning.load());
    EXPECT_GE(closeCallCount, 3);
    EXPECT_TRUE(closedServerSocket);
    EXPECT_TRUE(closedFd42);
    EXPECT_TRUE(closedFd43);

    {
        std::lock_guard<std::mutex> lock(server.handleMapMutex);
        EXPECT_TRUE(server.handleMap.empty());
    }
}

TEST_F(IpcSocketServerTest, givenServerWhenSocketCreationFailsThenInitializeFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return -1; };

    IpcSocketServer server;
    bool result = server.initialize();

    EXPECT_FALSE(result);
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerTest, givenServerWhenBindFailsThenInitializeFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return -1; };

    static int closeCallCount = 0;
    closeCallCount = 0;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closeCallCount++;
        return 0;
    };

    IpcSocketServer server;
    bool result = server.initialize();

    EXPECT_FALSE(result);
    EXPECT_FALSE(server.isRunning());
    EXPECT_EQ(1, closeCallCount);
}

TEST_F(IpcSocketServerTest, givenServerWhenListenFailsThenInitializeFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return -1; };

    static int closeCallCount = 0;
    closeCallCount = 0;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closeCallCount++;
        return 0;
    };

    IpcSocketServer server;
    bool result = server.initialize();

    EXPECT_FALSE(result);
    EXPECT_FALSE(server.isRunning());
    EXPECT_EQ(1, closeCallCount);
}

TEST_F(IpcSocketServerTest, givenServerWhenInitializeAndThreadCreationFailsThenReturnsFalseAndCleansUp) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);
    VariableBackup<decltype(Thread::createFunc)> threadBackup(&Thread::createFunc);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };

    static int closeCallCount = 0;
    closeCallCount = 0;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closeCallCount++;
        return 0;
    };

    Thread::createFunc = [](void *(*)(void *), void *) -> std::unique_ptr<Thread> {
        return nullptr;
    };

    IpcSocketServer server;
    bool result = server.initialize();

    EXPECT_FALSE(result);
    EXPECT_FALSE(server.isRunning());
    EXPECT_EQ(1, closeCallCount);
}

TEST_F(IpcSocketServerTest, givenServerThreadEntryWhenCalledDirectlyThenCallsServerThreadRunAndReturnsNullptr) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static int pollCallCount = 0;
    static TestableIpcSocketServer *serverPtr = nullptr;
    pollCallCount = 0;

    TestableIpcSocketServer server;
    serverPtr = &server;
    server.serverSocket = 5;
    server.shutdownRequested.store(false);

    SysCalls::sysCallsPoll = [](struct pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        serverPtr->shutdownRequested.store(true);
        return 0;
    };

    void *result = TestableIpcSocketServer::serverThreadEntry(&server);

    EXPECT_EQ(nullptr, result);
    EXPECT_EQ(1, pollCallCount);
    serverPtr = nullptr;
}

TEST_F(IpcSocketServerTest, givenServerThreadRunWhenShutdownRequestedAfterPollThenExitsLoop) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static int pollCallCount = 0;
    static TestableIpcSocketServer *serverPtr = nullptr;
    pollCallCount = 0;

    TestableIpcSocketServer server;
    serverPtr = &server;
    server.serverSocket = 5;
    server.shutdownRequested.store(false);

    SysCalls::sysCallsPoll = [](struct pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        serverPtr->shutdownRequested.store(true);
        return 0;
    };

    server.serverThreadRun();

    EXPECT_EQ(1, pollCallCount);
    serverPtr = nullptr;
}

TEST_F(IpcSocketServerTest, givenServerThreadRunWhenPollReturnsErrorWithEINTRThenContinuesAndChecksShutdown) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](struct pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (pollCallCount == 1) {
            errno = EINTR;
            return -1;
        }
        errno = EBADF;
        return -1;
    };

    TestableIpcSocketServer server;
    server.serverSocket = 5;
    server.shutdownRequested.store(false);

    server.serverThreadRun();

    EXPECT_EQ(2, pollCallCount);
}

TEST_F(IpcSocketServerTest, givenServerThreadRunWhenPollReturnsErrorWithNonEINTRThenBreaksLoop) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](struct pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        errno = EBADF;
        return -1;
    };

    TestableIpcSocketServer server;
    server.serverSocket = 5;
    server.shutdownRequested.store(false);

    server.serverThreadRun();

    EXPECT_EQ(1, pollCallCount);
}

TEST_F(IpcSocketServerTest, givenServerThreadRunWhenPollReturnsZeroThenContinuesLoopUntilShutdown) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](struct pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (pollCallCount >= 3) {
            errno = EBADF;
            return -1;
        }
        return 0;
    };

    TestableIpcSocketServer server;
    server.serverSocket = 5;
    server.shutdownRequested.store(false);

    server.serverThreadRun();

    EXPECT_EQ(3, pollCallCount);
}

TEST_F(IpcSocketServerTest, givenServerThreadRunWhenPollReturnsWithoutPollinThenContinuesLoop) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](struct pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (fds) {
            fds->revents = POLLHUP;
        }
        if (pollCallCount >= 3) {
            errno = EBADF;
            return -1;
        }
        return 1;
    };

    TestableIpcSocketServer server;
    server.serverSocket = 5;
    server.shutdownRequested.store(false);

    server.serverThreadRun();

    EXPECT_EQ(3, pollCallCount);
}

TEST_F(IpcSocketServerTest, givenServerThreadRunWhenAcceptReturnsErrorWithEAGAINThenContinuesLoop) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](struct pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (fds) {
            fds->revents = POLLIN;
        }
        if (pollCallCount >= 2) {
            errno = EBADF;
            return -1;
        }
        return 1;
    };

    static int acceptCallCount = 0;
    acceptCallCount = 0;
    SysCalls::sysCallsAccept = [](int, struct sockaddr *, socklen_t *) -> int {
        acceptCallCount++;
        errno = EAGAIN;
        return -1;
    };

    TestableIpcSocketServer server;
    server.serverSocket = 5;
    server.shutdownRequested.store(false);

    server.serverThreadRun();

    EXPECT_GE(acceptCallCount, 1);
}

TEST_F(IpcSocketServerTest, givenServerThreadRunWhenAcceptReturnsErrorWithOtherErrnoThenLogsAndContinues) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](struct pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (fds) {
            fds->revents = POLLIN;
        }
        if (pollCallCount >= 2) {
            errno = EBADF;
            return -1;
        }
        return 1;
    };

    static int acceptCallCount = 0;
    acceptCallCount = 0;
    SysCalls::sysCallsAccept = [](int, struct sockaddr *, socklen_t *) -> int {
        acceptCallCount++;
        errno = ECONNABORTED;
        return -1;
    };

    TestableIpcSocketServer server;
    server.serverSocket = 5;
    server.shutdownRequested.store(false);

    server.serverThreadRun();

    EXPECT_GE(acceptCallCount, 1);
}

TEST_F(IpcSocketServerTest, givenServerWhenShutdownImplCalledWithServerSocketSetThenClosesSocket) {
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    static int closedFd = -1;
    closedFd = -1;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closedFd = fd;
        return 0;
    };

    TestableIpcSocketServer server;

    server.serverRunning.store(true);
    server.serverSocket = 42;

    server.shutdownImpl();

    EXPECT_EQ(42, closedFd);
    EXPECT_EQ(-1, server.serverSocket);
}

TEST_F(IpcSocketServerTest, givenServerWhenInitializeCalledWhileAlreadyRunningThenReturnsTrue) {
    TestableIpcSocketServer server;
    server.serverRunning.store(true);

    bool result = server.initialize();

    EXPECT_TRUE(result);
}

TEST_F(IpcSocketServerTest, givenServerWhenShutdownImplCalledWithNullServerThreadThenSkipsThreadJoin) {
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    static int closeCallCount = 0;
    closeCallCount = 0;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closeCallCount++;
        return 0;
    };

    TestableIpcSocketServer server;

    server.serverRunning.store(true);
    server.serverSocket = 42;

    server.shutdownImpl();

    EXPECT_EQ(1, closeCallCount);
    EXPECT_FALSE(server.serverRunning.load());
}

TEST_F(IpcSocketServerTest, givenServerThreadRunWhenAcceptSucceedsThenHandlesClientAndClosesSocket) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](struct pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (fds) {
            fds->revents = POLLIN;
        }
        if (pollCallCount >= 2) {
            errno = EBADF;
            return -1;
        }
        return 1;
    };

    static int acceptCallCount = 0;
    acceptCallCount = 0;
    SysCalls::sysCallsAccept = [](int, struct sockaddr *, socklen_t *) -> int {
        acceptCallCount++;
        return 100;
    };

    static int setsockoptCallCount = 0;
    setsockoptCallCount = 0;
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int {
        setsockoptCallCount++;
        return 0;
    };

    SysCalls::sysCallsRecv = [](int, void *, size_t, int) -> ssize_t {
        return -1;
    };

    static int closedClientFd = -1;
    closedClientFd = -1;
    SysCalls::sysCallsClose = [](int fd) -> int {
        if (fd == 100) {
            closedClientFd = fd;
        }
        return 0;
    };

    TestableIpcSocketServer server;
    server.serverSocket = 5;
    server.shutdownRequested.store(false);

    server.serverThreadRun();

    EXPECT_EQ(1, acceptCallCount);
    EXPECT_EQ(2, setsockoptCallCount);
    EXPECT_EQ(100, closedClientFd);
}

TEST_F(IpcSocketServerTest, givenServerWhenInitializeSucceedsWithMockedThreadThenServerSocketRemainsValid) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);
    VariableBackup<decltype(Thread::createFunc)> threadBackup(&Thread::createFunc);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsClose = [](int) -> int { return 0; };

    static bool threadJoinCalled = false;
    threadJoinCalled = false;

    class MockThread : public Thread {
      public:
        void join() override {
            threadJoinCalled = true;
        }
        void detach() override {}
        void yield() override {}
    };

    Thread::createFunc = [](void *(*)(void *), void *) -> std::unique_ptr<Thread> {
        return std::make_unique<MockThread>();
    };

    IpcSocketServer server;
    bool result = server.initialize();

    EXPECT_TRUE(result);
    EXPECT_TRUE(server.isRunning());

    server.shutdown();
    EXPECT_TRUE(threadJoinCalled);
}
