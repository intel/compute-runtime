/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

using namespace NEO;

namespace NEO {
namespace SysCalls {
extern int socketCalled;
extern int bindCalled;
extern int listenCalled;
extern int acceptCalled;
extern int connectCalled;
extern int sendCalled;
extern int recvCalled;
extern int sendmsgCalled;
extern int recvmsgCalled;
extern int setsockoptCalled;
extern int dupCalled;
extern int getpidCalled;
} // namespace SysCalls
} // namespace NEO

class SysCallsSocketTest : public ::testing::Test {
  public:
    void SetUp() override {
        SysCalls::socketCalled = 0;
        SysCalls::bindCalled = 0;
        SysCalls::listenCalled = 0;
        SysCalls::acceptCalled = 0;
        SysCalls::connectCalled = 0;
        SysCalls::sendCalled = 0;
        SysCalls::recvCalled = 0;
        SysCalls::sendmsgCalled = 0;
        SysCalls::recvmsgCalled = 0;
        SysCalls::setsockoptCalled = 0;
        SysCalls::dupCalled = 0;
        SysCalls::getpidCalled = 0;
    }

    void TearDown() override {
        SysCalls::sysCallsSocket = nullptr;
        SysCalls::sysCallsBind = nullptr;
        SysCalls::sysCallsListen = nullptr;
        SysCalls::sysCallsAccept = nullptr;
        SysCalls::sysCallsConnect = nullptr;
        SysCalls::sysCallsSend = nullptr;
        SysCalls::sysCallsRecv = nullptr;
        SysCalls::sysCallsSendmsg = nullptr;
        SysCalls::sysCallsRecvmsg = nullptr;
        SysCalls::sysCallsSetsockopt = nullptr;
        SysCalls::sysCallsDup = nullptr;
        SysCalls::sysCallsGetpid = nullptr;
    }
};

TEST_F(SysCallsSocketTest, givenSocketWhenCalledThenCallCounterIncreases) {
    int result = NEO::SysCalls::socket(AF_UNIX, SOCK_STREAM, 0);
    EXPECT_EQ(1, SysCalls::socketCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenSocketWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> backup(&SysCalls::sysCallsSocket);
    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 42; };

    int result = NEO::SysCalls::socket(AF_UNIX, SOCK_STREAM, 0);
    EXPECT_EQ(42, result);
}

TEST_F(SysCallsSocketTest, givenSocketWhenMockReturnsErrorThenErrorIsReturned) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> backup(&SysCalls::sysCallsSocket);
    SysCalls::sysCallsSocket = [](int, int, int) -> int { return -1; };

    int result = NEO::SysCalls::socket(AF_UNIX, SOCK_STREAM, 0);
    EXPECT_EQ(-1, result);
}

TEST_F(SysCallsSocketTest, givenBindWhenCalledThenCallCounterIncreases) {
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;

    int result = NEO::SysCalls::bind(5, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    EXPECT_EQ(1, SysCalls::bindCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenBindWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsBind)> backup(&SysCalls::sysCallsBind);
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return -1; };

    struct sockaddr_un addr = {};
    int result = NEO::SysCalls::bind(5, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    EXPECT_EQ(-1, result);
}

TEST_F(SysCallsSocketTest, givenListenWhenCalledThenCallCounterIncreases) {
    int result = NEO::SysCalls::listen(5, 10);
    EXPECT_EQ(1, SysCalls::listenCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenListenWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsListen)> backup(&SysCalls::sysCallsListen);
    SysCalls::sysCallsListen = [](int, int) -> int { return -1; };

    int result = NEO::SysCalls::listen(5, 10);
    EXPECT_EQ(-1, result);
}

TEST_F(SysCallsSocketTest, givenAcceptWhenCalledThenCallCounterIncreases) {
    int result = NEO::SysCalls::accept(5, nullptr, nullptr);
    EXPECT_EQ(1, SysCalls::acceptCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenAcceptWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsAccept)> backup(&SysCalls::sysCallsAccept);
    SysCalls::sysCallsAccept = [](int, struct sockaddr *, socklen_t *) -> int { return 99; };

    int result = NEO::SysCalls::accept(5, nullptr, nullptr);
    EXPECT_EQ(99, result);
}

TEST_F(SysCallsSocketTest, givenConnectWhenCalledThenCallCounterIncreases) {
    struct sockaddr_un addr = {};
    int result = NEO::SysCalls::connect(5, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    EXPECT_EQ(1, SysCalls::connectCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenConnectWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsConnect)> backup(&SysCalls::sysCallsConnect);
    SysCalls::sysCallsConnect = [](int, const struct sockaddr *, socklen_t) -> int { return -1; };

    struct sockaddr_un addr = {};
    int result = NEO::SysCalls::connect(5, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    EXPECT_EQ(-1, result);
}

TEST_F(SysCallsSocketTest, givenSendWhenCalledThenCallCounterIncreases) {
    const char *data = "test";
    ssize_t result = NEO::SysCalls::send(5, data, 4, 0);
    EXPECT_EQ(1, SysCalls::sendCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenSendWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsSend)> backup(&SysCalls::sysCallsSend);
    SysCalls::sysCallsSend = [](int, const void *, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };

    const char *data = "test";
    ssize_t result = NEO::SysCalls::send(5, data, 4, 0);
    EXPECT_EQ(4, result);
}

TEST_F(SysCallsSocketTest, givenRecvWhenCalledThenCallCounterIncreases) {
    char buffer[10];
    ssize_t result = NEO::SysCalls::recv(5, buffer, 10, 0);
    EXPECT_EQ(1, SysCalls::recvCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenRecvWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> backup(&SysCalls::sysCallsRecv);
    SysCalls::sysCallsRecv = [](int, void *, size_t len, int) -> ssize_t { return static_cast<ssize_t>(len); };

    char buffer[10];
    ssize_t result = NEO::SysCalls::recv(5, buffer, 10, 0);
    EXPECT_EQ(10, result);
}

TEST_F(SysCallsSocketTest, givenSendmsgWhenCalledThenCallCounterIncreases) {
    struct msghdr msg = {};
    ssize_t result = NEO::SysCalls::sendmsg(5, &msg, 0);
    EXPECT_EQ(1, SysCalls::sendmsgCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenSendmsgWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsSendmsg)> backup(&SysCalls::sysCallsSendmsg);
    SysCalls::sysCallsSendmsg = [](int, const struct msghdr *, int) -> ssize_t { return 100; };

    struct msghdr msg = {};
    ssize_t result = NEO::SysCalls::sendmsg(5, &msg, 0);
    EXPECT_EQ(100, result);
}

TEST_F(SysCallsSocketTest, givenRecvmsgWhenCalledThenCallCounterIncreases) {
    struct msghdr msg = {};
    ssize_t result = NEO::SysCalls::recvmsg(5, &msg, 0);
    EXPECT_EQ(1, SysCalls::recvmsgCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenRecvmsgWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsRecvmsg)> backup(&SysCalls::sysCallsRecvmsg);
    SysCalls::sysCallsRecvmsg = [](int, struct msghdr *, int) -> ssize_t { return 50; };

    struct msghdr msg = {};
    ssize_t result = NEO::SysCalls::recvmsg(5, &msg, 0);
    EXPECT_EQ(50, result);
}

TEST_F(SysCallsSocketTest, givenSetsockoptWhenCalledThenCallCounterIncreases) {
    int optval = 1;
    int result = NEO::SysCalls::setsockopt(5, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    EXPECT_EQ(1, SysCalls::setsockoptCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenSetsockoptWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsSetsockopt)> backup(&SysCalls::sysCallsSetsockopt);
    SysCalls::sysCallsSetsockopt = [](int, int, int, const void *, socklen_t) -> int { return -1; };

    int optval = 1;
    int result = NEO::SysCalls::setsockopt(5, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    EXPECT_EQ(-1, result);
}

TEST_F(SysCallsSocketTest, givenDupWhenCalledThenCallCounterIncreases) {
    int result = NEO::SysCalls::dup(5);
    EXPECT_EQ(1, SysCalls::dupCalled);
    EXPECT_EQ(0, result);
}

TEST_F(SysCallsSocketTest, givenDupWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> backup(&SysCalls::sysCallsDup);
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    int result = NEO::SysCalls::dup(5);
    EXPECT_EQ(105, result);
}

TEST_F(SysCallsSocketTest, givenDupWhenMockReturnsErrorThenErrorIsReturned) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> backup(&SysCalls::sysCallsDup);
    SysCalls::sysCallsDup = [](int) -> int { return -1; };

    int result = NEO::SysCalls::dup(5);
    EXPECT_EQ(-1, result);
}

TEST_F(SysCallsSocketTest, givenGetpidWhenCalledThenCallCounterIncreases) {
    pid_t result = NEO::SysCalls::getpid();
    EXPECT_EQ(1, SysCalls::getpidCalled);
    EXPECT_GE(result, 0);
}

TEST_F(SysCallsSocketTest, givenGetpidWhenMockSetThenReturnsMockValue) {
    VariableBackup<decltype(SysCalls::sysCallsGetpid)> backup(&SysCalls::sysCallsGetpid);
    SysCalls::sysCallsGetpid = []() -> int { return 12345; };

    pid_t result = NEO::SysCalls::getpid();
    EXPECT_EQ(12345, result);
}

TEST_F(SysCallsSocketTest, givenMultipleSocketCallsWhenCalledThenCounterAccumulates) {
    NEO::SysCalls::socket(AF_UNIX, SOCK_STREAM, 0);
    NEO::SysCalls::socket(AF_UNIX, SOCK_STREAM, 0);
    NEO::SysCalls::socket(AF_UNIX, SOCK_STREAM, 0);

    EXPECT_EQ(3, SysCalls::socketCalled);
}

TEST_F(SysCallsSocketTest, givenMultipleDifferentCallsWhenCalledThenEachCounterIncreases) {
    NEO::SysCalls::socket(AF_UNIX, SOCK_STREAM, 0);
    NEO::SysCalls::bind(5, nullptr, 0);
    NEO::SysCalls::listen(5, 10);
    NEO::SysCalls::accept(5, nullptr, nullptr);
    NEO::SysCalls::connect(5, nullptr, 0);

    EXPECT_EQ(1, SysCalls::socketCalled);
    EXPECT_EQ(1, SysCalls::bindCalled);
    EXPECT_EQ(1, SysCalls::listenCalled);
    EXPECT_EQ(1, SysCalls::acceptCalled);
    EXPECT_EQ(1, SysCalls::connectCalled);
}

TEST_F(SysCallsSocketTest, givenSendAndRecvWhenCalledThenBothCountersIncrease) {
    const char *data = "test";
    char buffer[10];

    NEO::SysCalls::send(5, data, 4, 0);
    NEO::SysCalls::recv(5, buffer, 10, 0);

    EXPECT_EQ(1, SysCalls::sendCalled);
    EXPECT_EQ(1, SysCalls::recvCalled);
}

TEST_F(SysCallsSocketTest, givenSendmsgAndRecvmsgWhenCalledThenBothCountersIncrease) {
    struct msghdr msg = {};

    NEO::SysCalls::sendmsg(5, &msg, 0);
    NEO::SysCalls::recvmsg(5, &msg, 0);

    EXPECT_EQ(1, SysCalls::sendmsgCalled);
    EXPECT_EQ(1, SysCalls::recvmsgCalled);
}

TEST_F(SysCallsSocketTest, givenMockThatChecksParametersWhenCalledThenParametersAreCorrect) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> backup(&SysCalls::sysCallsSocket);

    static int capturedDomain = 0;
    static int capturedType = 0;
    static int capturedProtocol = 0;

    SysCalls::sysCallsSocket = [](int domain, int type, int protocol) -> int {
        capturedDomain = domain;
        capturedType = type;
        capturedProtocol = protocol;
        return 42;
    };

    NEO::SysCalls::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

    EXPECT_EQ(AF_UNIX, capturedDomain);
    EXPECT_EQ(SOCK_STREAM | SOCK_CLOEXEC, capturedType);
    EXPECT_EQ(0, capturedProtocol);
}

TEST_F(SysCallsSocketTest, givenSendWithDifferentFlagsWhenCalledThenFlagsArePassedCorrectly) {
    VariableBackup<decltype(SysCalls::sysCallsSend)> backup(&SysCalls::sysCallsSend);

    static int capturedFlags = 0;
    SysCalls::sysCallsSend = [](int, const void *, size_t, int flags) -> ssize_t {
        capturedFlags = flags;
        return 0;
    };

    const char *data = "test";
    NEO::SysCalls::send(5, data, 4, MSG_DONTWAIT);

    EXPECT_EQ(MSG_DONTWAIT, capturedFlags);
}

TEST_F(SysCallsSocketTest, givenRecvWithDifferentFlagsWhenCalledThenFlagsArePassedCorrectly) {
    VariableBackup<decltype(SysCalls::sysCallsRecv)> backup(&SysCalls::sysCallsRecv);

    static int capturedFlags = 0;
    SysCalls::sysCallsRecv = [](int, void *, size_t, int flags) -> ssize_t {
        capturedFlags = flags;
        return 0;
    };

    char buffer[10];
    NEO::SysCalls::recv(5, buffer, 10, MSG_WAITALL);

    EXPECT_EQ(MSG_WAITALL, capturedFlags);
}
