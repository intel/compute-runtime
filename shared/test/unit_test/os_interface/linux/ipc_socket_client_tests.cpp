/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_client.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include <errno.h>

using namespace NEO;

namespace NEO {
namespace SysCalls {
extern int socketCalled;
extern int connectCalled;
extern uint32_t closeFuncCalled;
extern int sendCalled;
extern int recvCalled;
extern int recvmsgCalled;
extern int setsockoptCalled;
} // namespace SysCalls
} // namespace NEO

class TestableIpcSocketClient : public IpcSocketClient {
  public:
    using IpcSocketClient::receiveFileDescriptor;
    using IpcSocketClient::receiveMessage;
    using IpcSocketClient::sendMessage;
};

class IpcSocketClientTest : public ::testing::Test {
  public:
    void SetUp() override {
        SysCalls::socketCalled = 0;
        SysCalls::connectCalled = 0;
        SysCalls::closeFuncCalled = 0;
        SysCalls::sendCalled = 0;
        SysCalls::recvCalled = 0;
        SysCalls::recvmsgCalled = 0;
        SysCalls::setsockoptCalled = 0;
    }

    void TearDown() override {
        SysCalls::sysCallsSocket = nullptr;
        SysCalls::sysCallsConnect = nullptr;
        SysCalls::sysCallsSend = nullptr;
        SysCalls::sysCallsRecv = nullptr;
        SysCalls::sysCallsRecvmsg = nullptr;
        SysCalls::sysCallsSetsockopt = nullptr;
    }
};

TEST_F(IpcSocketClientTest, givenClientWhenConstructedThenNotConnected) {
    IpcSocketClient client;
    EXPECT_FALSE(client.isConnected());
}

TEST_F(IpcSocketClientTest, givenClientWhenConnectToServerSucceedsThenIsConnectedReturnsTrue) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));
    EXPECT_TRUE(client.isConnected());

    EXPECT_GT(SysCalls::socketCalled, 0);
    EXPECT_GT(SysCalls::connectCalled, 0);
    EXPECT_GT(SysCalls::setsockoptCalled, 0);
}

TEST_F(IpcSocketClientTest, givenClientWhenSocketCreationFailsThenConnectFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return -1; };

    IpcSocketClient client;
    EXPECT_FALSE(client.connectToServer("test_socket"));
    EXPECT_FALSE(client.isConnected());

    EXPECT_GT(SysCalls::socketCalled, 0);
    EXPECT_EQ(SysCalls::connectCalled, 0);
}

TEST_F(IpcSocketClientTest, givenClientWhenConnectFailsThenIsConnectedReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return -1; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    IpcSocketClient client;
    EXPECT_FALSE(client.connectToServer("test_socket"));
    EXPECT_FALSE(client.isConnected());

    EXPECT_GT(SysCalls::connectCalled, 0);
}

TEST_F(IpcSocketClientTest, givenClientWhenAlreadyConnectedThenSubsequentConnectSucceeds) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int connectCallsFirst = SysCalls::connectCalled;

    EXPECT_TRUE(client.connectToServer("test_socket"));
    EXPECT_EQ(connectCallsFirst, SysCalls::connectCalled);
}

TEST_F(IpcSocketClientTest, givenClientWhenDisconnectCalledThenNotConnected) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));
    EXPECT_TRUE(client.isConnected());

    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(IpcSocketClientTest, givenClientWhenDisconnectCalledMultipleTimesThenNoError) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    client.disconnect();
    EXPECT_FALSE(client.isConnected());

    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(IpcSocketClientTest, givenClientWhenNotConnectedThenRequestHandleReturnsFail) {
    IpcSocketClient client;
    EXPECT_FALSE(client.isConnected());

    int receivedFd = client.requestHandle(12345);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleAndSendFailsThenReturnsMinusOne) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *, size_t, int) -> ssize_t { return -1; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(12345);
    EXPECT_EQ(-1, receivedFd);
    EXPECT_GT(SysCalls::sendCalled, 0);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleAndRecvmsgFailsThenReturnsMinusOne) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(12345);
    EXPECT_EQ(-1, receivedFd);
    EXPECT_GT(SysCalls::recvmsgCalled, 0);
}

TEST_F(IpcSocketClientTest, givenClientWhenDestructorCalledWhileConnectedThenDisconnects) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    uint32_t closeCallsBefore = SysCalls::closeFuncCalled;

    {
        IpcSocketClient client;
        EXPECT_TRUE(client.connectToServer("test_socket"));
        EXPECT_TRUE(client.isConnected());
    }

    EXPECT_GT(SysCalls::closeFuncCalled, closeCallsBefore);
}

TEST_F(IpcSocketClientTest, givenClientWhenDestructorCalledWithoutConnectionThenNoError) {
    IpcSocketClient client;
}

TEST_F(IpcSocketClientTest, givenClientWhenConnectWithEmptySocketPathThenStillAttempts) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer(""));
    EXPECT_TRUE(client.isConnected());
}

TEST_F(IpcSocketClientTest, givenClientWhenConnectWithLongSocketPathThenHandlesCorrectly) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    IpcSocketClient client;
    std::string longPath(200, 'a');
    EXPECT_TRUE(client.connectToServer(longPath));
}

TEST_F(IpcSocketClientTest, givenClientWhenSendPartiallySucceedsThenFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len / 2); };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(12345);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleMultipleTimesThenEachMakesSeparateRequest) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    client.requestHandle(1);
    int sendCallsFirst = SysCalls::sendCalled;

    client.requestHandle(2);
    EXPECT_GT(SysCalls::sendCalled, sendCallsFirst);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleReceivesFdButSuccessIsFalseThenCloseFdAndReturnMinusOne) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup(&SysCalls::sysCallsClose);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };

    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        if (msg->msg_iovlen > 0 && msg->msg_iov[0].iov_base) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(msg->msg_iov[0].iov_base);
            payload->success = false;
        }

        if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int))) {
            struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = 42;
        }

        return msg->msg_iov[0].iov_len;
    };

    static int closedFd = -1;
    SysCalls::sysCallsClose = [](int fd) -> int {
        closedFd = fd;
        return 0;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);

    EXPECT_EQ(-1, receivedFd);
    EXPECT_EQ(42, closedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenRecvmsgFailsAndFallbackReceiveMessageSucceedsThenReturnsMinusOne) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };

    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t {
        return -1;
    };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = sizeof(IpcSocketResponsePayload);
            msg->reserved = 0;
            return sizeof(IpcSocketMessage);
        } else if (len == sizeof(IpcSocketResponsePayload)) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(buf);
            payload->success = false;
            memset(payload->reserved, 0, sizeof(payload->reserved));
            return sizeof(IpcSocketResponsePayload);
        }
        return -1;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);

    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveFileDescriptorWithNullDataThenHandlesCorrectly) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        return -1;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int result = client.requestHandle(100);
    EXPECT_EQ(-1, result);
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveMessageWithPayloadSizeLargerThanMaxSizeThenFails) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = 9999;
            msg->reserved = 0;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));
}

TEST_F(IpcSocketClientTest, givenClientWhenRecvmsgSucceedsButNoCmsgThenReturnsMinusOne) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };

    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        if (msg->msg_iovlen > 0 && msg->msg_iov[0].iov_base) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(msg->msg_iov[0].iov_base);
            payload->success = true;
        }
        msg->msg_controllen = 0;
        return msg->msg_iov ? static_cast<ssize_t>(msg->msg_iov[0].iov_len) : 0;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenSendMessageWithPayloadAndPayloadSendFailsThenReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static int sendCallCount = 0;
    sendCallCount = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCount++;
        if (sendCallCount == 1) {
            return static_cast<ssize_t>(len);
        }
        return -1;
    };

    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    static int recvCallCount = 0;
    recvCallCount = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCount++;
        if (recvCallCount == 1 && len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = sizeof(IpcSocketResponsePayload);
            return sizeof(IpcSocketMessage);
        } else if (recvCallCount == 2 && len == sizeof(IpcSocketResponsePayload)) {
            return -1;
        }
        return -1;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveMessagePayloadRecvFailsThenReturnsFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    static int recvCallCount = 0;
    recvCallCount = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCount++;
        if (recvCallCount == 1 && len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = sizeof(IpcSocketResponsePayload);
            return sizeof(IpcSocketMessage);
        }
        return static_cast<ssize_t>(len / 2);
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenRecvmsgSucceedsWithWrongCmsgLevelThenReturnsMinusOne) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };

    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        if (msg->msg_iovlen > 0 && msg->msg_iov[0].iov_base) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(msg->msg_iov[0].iov_base);
            payload->success = true;
        }
        if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int))) {
            struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
            cmsg->cmsg_level = 0;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = 42;
        }
        return msg->msg_iov ? static_cast<ssize_t>(msg->msg_iov[0].iov_len) : 0;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenRecvmsgSucceedsWithWrongCmsgTypeThenReturnsMinusOne) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };

    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        if (msg->msg_iovlen > 0 && msg->msg_iov[0].iov_base) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(msg->msg_iov[0].iov_base);
            payload->success = true;
        }
        if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int))) {
            struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_CREDENTIALS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = 42;
        }
        return msg->msg_iov ? static_cast<ssize_t>(msg->msg_iov[0].iov_len) : 0;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleFallbackReceivesWrongMessageTypeThenReturnsMinusOne) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->processId = 12345;
            msg->handleId = 100;
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
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleFallbackReceivesWrongHandleIdThenReturnsMinusOne) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 999;
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
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleFallbackReceivesSuccessTrueThenReturnsMinusOne) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = sizeof(IpcSocketResponsePayload);
            return sizeof(IpcSocketMessage);
        } else if (len == sizeof(IpcSocketResponsePayload)) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(buf);
            payload->success = true;
            return sizeof(IpcSocketResponsePayload);
        }
        return -1;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleFallbackReceiveMessageFailsThenReturnsMinusOne) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t { return -1; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveFileDescriptorWithZeroDataSizeThenNoIovSet) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };

    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int))) {
            struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = 42;
        }
        return 0;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    (void)receivedFd;
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveMessageWithNullPayloadThenSkipsPayloadRecv) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    static int recvCallCount = 0;
    recvCallCount = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCount++;
        if (recvCallCount == 1 && len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = 0;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
    EXPECT_EQ(1, recvCallCount);
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveMessageWithPayloadSizeLargerThanMaxPayloadSizeThenSkipsPayloadRecv) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    static int recvCallCount = 0;
    recvCallCount = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCount++;
        if (recvCallCount == 1 && len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = 99999;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
    EXPECT_EQ(1, recvCallCount);
}

TEST_F(IpcSocketClientTest, givenClientWhenSendMessageWithNullPayloadThenSkipsPayloadSend) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static int sendCallCountLocal = 0;
    sendCallCountLocal = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCountLocal++;
        return static_cast<ssize_t>(len);
    };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    (void)receivedFd;

    EXPECT_EQ(1, sendCallCountLocal);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleAndRecvmsgFailsWithValidFallbackResponseThenHandlesCorrectly) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
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
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenSendMessageWithPayloadThenSendsPayload) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static int sendCallCountLocal = 0;
    static size_t lastPayloadSize = 0;
    sendCallCountLocal = 0;
    lastPayloadSize = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCountLocal++;
        if (sendCallCountLocal == 2) {
            lastPayloadSize = len;
        }
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    IpcSocketMessage msg{};
    msg.type = IpcSocketMessageType::requestHandle;
    msg.payloadSize = sizeof(IpcSocketResponsePayload);
    IpcSocketResponsePayload payload{};
    payload.success = true;

    bool result = client.sendMessage(msg, &payload);
    EXPECT_TRUE(result);
    EXPECT_EQ(2, sendCallCountLocal);
    EXPECT_EQ(sizeof(IpcSocketResponsePayload), lastPayloadSize);
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveMessageWithNullPayloadBufferThenSkipsPayloadRecv) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static int recvCallCountLocal = 0;
    recvCallCountLocal = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCountLocal++;
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = 100;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    TestableIpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    IpcSocketMessage response{};

    bool result = client.receiveMessage(response, nullptr, 100);
    EXPECT_TRUE(result);
    EXPECT_EQ(1, recvCallCountLocal);
}

TEST_F(IpcSocketClientTest, givenClientWhenFallbackReceivesWrongTypeThenNoErrorPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->processId = 12345;
            msg->handleId = 100;
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
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenFallbackReceivesWrongHandleIdThenNoErrorPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 999;
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
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenFallbackReceivesSuccessTrueThenNoErrorPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return -1; };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::responseHandle;
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = sizeof(IpcSocketResponsePayload);
            return sizeof(IpcSocketMessage);
        } else if (len == sizeof(IpcSocketResponsePayload)) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(buf);
            payload->success = true;
            return sizeof(IpcSocketResponsePayload);
        }
        return -1;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(-1, receivedFd);
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveFileDescriptorWithNullDataDirectlyThenSkipsIov) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static bool iovWasNull = false;
    iovWasNull = false;
    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        iovWasNull = (msg->msg_iovlen == 0);
        if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int))) {
            struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = 42;
        }
        return 0;
    };

    TestableIpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int result = client.receiveFileDescriptor(nullptr, 100);
    EXPECT_TRUE(iovWasNull);
    EXPECT_EQ(42, result);
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveFileDescriptorWithZeroSizeDirectlyThenSkipsIov) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static bool iovWasNull = false;
    iovWasNull = false;
    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        iovWasNull = (msg->msg_iovlen == 0);
        if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int))) {
            struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = 42;
        }
        return 0;
    };

    TestableIpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    char buffer[10];
    int result = client.receiveFileDescriptor(buffer, 0);
    EXPECT_TRUE(iovWasNull);
    EXPECT_EQ(42, result);
}

TEST_F(IpcSocketClientTest, givenClientWhenReceiveFileDescriptorWithValidDataDirectlyThenSetsIov) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static bool iovWasSet = false;
    iovWasSet = false;
    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        iovWasSet = (msg->msg_iovlen == 1 && msg->msg_iov != nullptr);
        if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int))) {
            struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = 42;
        }
        return 0;
    };

    TestableIpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    char buffer[10];
    int result = client.receiveFileDescriptor(buffer, sizeof(buffer));
    EXPECT_TRUE(iovWasSet);
    EXPECT_EQ(42, result);
}

TEST_F(IpcSocketClientTest, givenClientWhenSendMessageWithNullPayloadDirectlyThenSkipsPayloadSend) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static int sendCallCountLocal = 0;
    sendCallCountLocal = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCountLocal++;
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    IpcSocketMessage msg{};
    msg.payloadSize = 100;

    bool result = client.sendMessage(msg, nullptr);
    EXPECT_TRUE(result);
    EXPECT_EQ(1, sendCallCountLocal);
}

TEST_F(IpcSocketClientTest, givenClientWhenSendMessageWithZeroPayloadSizeDirectlyThenSkipsPayloadSend) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static int sendCallCountLocal = 0;
    sendCallCountLocal = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCountLocal++;
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    IpcSocketMessage msg{};
    msg.payloadSize = 0;
    char payload[10] = {};

    bool result = client.sendMessage(msg, payload);
    EXPECT_TRUE(result);
    EXPECT_EQ(1, sendCallCountLocal);
}

TEST_F(IpcSocketClientTest, givenClientWhenSendMessageWithValidPayloadDirectlyThenSendsPayload) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static int sendCallCountLocal = 0;
    sendCallCountLocal = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCountLocal++;
        return static_cast<ssize_t>(len);
    };

    TestableIpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    IpcSocketMessage msg{};
    msg.payloadSize = 10;
    char payload[10] = {};

    bool result = client.sendMessage(msg, payload);
    EXPECT_TRUE(result);
    EXPECT_EQ(2, sendCallCountLocal);
}

TEST_F(IpcSocketClientTest, givenClientWhenSendMessageWithValidPayloadAndPayloadSendFailsDirectlyThenReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };

    static int sendCallCountLocal = 0;
    sendCallCountLocal = 0;
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t {
        sendCallCountLocal++;
        if (sendCallCountLocal == 1) {
            return static_cast<ssize_t>(len);
        }
        return -1;
    };

    TestableIpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    IpcSocketMessage msg{};
    msg.payloadSize = 10;
    char payload[10] = {};

    bool result = client.sendMessage(msg, payload);
    EXPECT_FALSE(result);
    EXPECT_EQ(2, sendCallCountLocal);
}

TEST_F(IpcSocketClientTest, givenClientWhenRequestHandleReceivesFdWithSuccessTrueThenReturnsValidFd) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsConnect)> connectBackup(&SysCalls::sysCallsConnect);
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> setsockoptBackup(&SysCalls::sysCallsSetsockopt);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> recvmsgBackup(&SysCalls::sysCallsRecvmsg);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 10; };
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsSend = [](int, const void *buf, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };

    SysCalls::sysCallsRecvmsg = [](int sockfd, struct msghdr *msg, int flags) -> ssize_t {
        if (msg->msg_iovlen > 0 && msg->msg_iov[0].iov_base) {
            IpcSocketResponsePayload *payload = static_cast<IpcSocketResponsePayload *>(msg->msg_iov[0].iov_base);
            payload->success = true;
        }

        if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int))) {
            struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = 42;
        }

        return msg->msg_iov[0].iov_len;
    };

    IpcSocketClient client;
    EXPECT_TRUE(client.connectToServer("test_socket"));

    int receivedFd = client.requestHandle(100);
    EXPECT_EQ(42, receivedFd);
}
