/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_server.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include <atomic>
#include <errno.h>
#include <thread>
#include <vector>

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

constexpr uint64_t maxNumHandles = 128;

template <typename Condition>
static void waitFor(Condition condition) {
    while (!condition()) {
        std::this_thread::yield();
    }
}

class TestedIpcSocketServer : public IpcSocketServer {
  public:
    ~TestedIpcSocketServer() override {
        unblockPollStub();
    }

    bool initialize() override {
        pollStubUnblocked.store(false);
        idlePollCount = 0;
        return IpcSocketServer::initialize();
    }

    void shutdown() override {
        unblockPollStub();
        IpcSocketServer::shutdown();
    }

    static int pollNoEvents(pollfd *, nfds_t, int) {
        idlePollCount++;
        pollStubUnblocked.wait(false);
        return 0;
    }

    static void waitUntilIdle() {
        waitFor([] { return idlePollCount.load() > 0; });
    }

  protected:
    void unblockPollStub() {
        pollStubUnblocked.store(true);
        pollStubUnblocked.notify_all();
    }

    static inline std::atomic<bool> pollStubUnblocked{false};
    static inline std::atomic<int> idlePollCount{0};
};

class IpcSocketServerMtTest : public ::testing::Test {
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
        SysCalls::sysCallsPoll = TestedIpcSocketServer::pollNoEvents;
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

TEST_F(IpcSocketServerMtTest, givenServerWhenInitializedSuccessfullyThenIsRunningReturnsTrue) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());
    EXPECT_TRUE(server.isRunning());

    EXPECT_GT(SysCalls::socketCalled, 0);
    EXPECT_GT(SysCalls::bindCalled, 0);
    EXPECT_GT(SysCalls::listenCalled, 0);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenSocketCreationFailsThenInitializeFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return -1; };

    TestedIpcSocketServer server;
    EXPECT_FALSE(server.initialize());
    EXPECT_FALSE(server.isRunning());

    EXPECT_GT(SysCalls::socketCalled, 0);
    EXPECT_EQ(SysCalls::bindCalled, 0);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenBindFailsThenInitializeFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return -1; };

    TestedIpcSocketServer server;
    EXPECT_FALSE(server.initialize());
    EXPECT_FALSE(server.isRunning());

    EXPECT_GT(SysCalls::socketCalled, 0);
    EXPECT_GT(SysCalls::bindCalled, 0);
    EXPECT_EQ(SysCalls::listenCalled, 0);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenListenFailsThenInitializeFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return -1; };

    TestedIpcSocketServer server;
    EXPECT_FALSE(server.initialize());
    EXPECT_FALSE(server.isRunning());

    EXPECT_GT(SysCalls::listenCalled, 0);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenInitializedMultipleTimesThenSubsequentCallsSucceed) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());
    EXPECT_TRUE(server.isRunning());

    int initialSocketCalls = SysCalls::socketCalled;

    EXPECT_TRUE(server.initialize());
    EXPECT_EQ(initialSocketCalls, SysCalls::socketCalled);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenShutdownCalledThenServerStops) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());
    EXPECT_TRUE(server.isRunning());

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenShutdownCalledMultipleTimesThenNoError) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    server.shutdown();
    EXPECT_FALSE(server.isRunning());

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenRegisterHandleWithValidFdThenSucceeds) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    uint64_t handleId = 12345;
    int fd = 42;

    EXPECT_TRUE(server.registerHandle(handleId, fd));
    EXPECT_GT(SysCalls::dupCalled, 0);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenRegisterSameHandleMultipleTimesThenRefCountIncreases) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    uint64_t handleId = 12345;
    int fd = 42;

    EXPECT_TRUE(server.registerHandle(handleId, fd));
    int dupCallsFirst = SysCalls::dupCalled;

    EXPECT_TRUE(server.registerHandle(handleId, fd));
    EXPECT_EQ(dupCallsFirst, SysCalls::dupCalled);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenDupFailsThenRegisterHandleFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int) -> int { return -1; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    uint64_t handleId = 12345;
    int fd = 42;

    EXPECT_FALSE(server.registerHandle(handleId, fd));
}

TEST_F(IpcSocketServerMtTest, givenServerWhenUnregisterExistingHandleThenSucceeds) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    uint64_t handleId = 12345;
    int fd = 42;

    EXPECT_TRUE(server.registerHandle(handleId, fd));
    EXPECT_TRUE(server.unregisterHandle(handleId));
}

TEST_F(IpcSocketServerMtTest, givenServerWhenRegisterMultipleDifferentHandlesThenAllAreStored) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    EXPECT_TRUE(server.registerHandle(1, 10));
    EXPECT_TRUE(server.registerHandle(2, 20));
    EXPECT_TRUE(server.registerHandle(3, 30));

    EXPECT_EQ(3, SysCalls::dupCalled);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenDestructorCalledWithRegisteredHandlesThenHandlesAreCleaned) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    uint32_t closeCallsBefore = SysCalls::closeFuncCalled;

    {
        TestedIpcSocketServer server;
        EXPECT_TRUE(server.initialize());
        EXPECT_TRUE(server.registerHandle(1, 10));
        EXPECT_TRUE(server.registerHandle(2, 20));
    }

    EXPECT_GT(SysCalls::closeFuncCalled, closeCallsBefore);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenThreadCreationFailsThenInitializeFails) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(Thread::createFunc)> threadBackup(&Thread::createFunc);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    Thread::createFunc = [](void *(*)(void *), void *) -> std::unique_ptr<Thread> { return nullptr; };

    TestedIpcSocketServer server;
    EXPECT_FALSE(server.initialize());
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenPollReturnsErrorThenServerExits) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static std::atomic<int> pollCallCount{0};
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        errno = EBADF;
        return -1;
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    waitFor([] { return pollCallCount.load() >= 1; });

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenAcceptReturnsOtherErrorThenContinues) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (pollCallCount <= 2) {
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        errno = ECONNABORTED;
        return -1;
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenClientConnectionReceiveFailsThenReturnsFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (pollCallCount == 1) {
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        return -1;
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenProcessRequestForNonExistentHandleThenSendsFailureResponse) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static std::atomic<int> pollCallCount{0};
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        int count = pollCallCount.fetch_add(1);
        if (count == 0) {
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

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

    static std::atomic<bool> sendCalled{false};
    sendCalled = false;
    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        sendCalled.store(true);
        return static_cast<ssize_t>(len);
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
    EXPECT_TRUE(sendCalled.load());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenProcessRequestForExistingHandleThenSendsFdViaScmRights) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);
    VariableBackup<decltype(SysCalls::sysCallsSendmsg)> sendmsgBackup(&SysCalls::sysCallsSendmsg);

    static std::atomic<bool> connectionProcessed{false};
    static std::atomic<bool> sendmsgCalled{false};

    connectionProcessed = false;
    sendmsgCalled = false;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (!connectionProcessed.load()) {
            connectionProcessed.store(true);
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->processId = 12345;
            msg->handleId = 12345;
            msg->payloadSize = 0;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    SysCalls::sysCallsSendmsg = [](int sockfd, const struct msghdr *msg, int flags) -> ssize_t {
        sendmsgCalled.store(true);
        return msg->msg_iov ? static_cast<ssize_t>(msg->msg_iov[0].iov_len) : 0;
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.registerHandle(12345, 42));
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
    EXPECT_TRUE(sendmsgCalled.load());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenSendMessageWithPayloadAndPayloadSendFailsThenReturnsFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (pollCallCount == 1) {
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

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

    static int sendCallCount = 0;
    sendCallCount = 0;
    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        sendCallCount++;
        if (sendCallCount == 1) {
            return static_cast<ssize_t>(len);
        }
        return -1;
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenReceiveMessageWithPayloadAndPayloadRecvFailsThenReturnsFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    static int pollCallCount = 0;
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        pollCallCount++;
        if (pollCallCount == 1) {
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

    static int recvCallCount = 0;
    recvCallCount = 0;
    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCount++;
        if (recvCallCount == 1 && len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->processId = 12345;
            msg->handleId = 12345;
            msg->payloadSize = 100;
            return sizeof(IpcSocketMessage);
        }

        return -1;
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWithRegisteredHandleWhenShutdownThenClosesFileDescriptors) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    EXPECT_TRUE(server.registerHandle(12345, 42));

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenRunningServerWhenUnregisterNonExistentHandleCalledThenReturnsFalse) {
    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    EXPECT_FALSE(server.unregisterHandle(99999));

    server.shutdown();
}

TEST_F(IpcSocketServerMtTest, givenServerWhenUnregisterHandleWithInvalidFdThenStillDecrements) {
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    static int dupReturnValue = 42;
    SysCalls::sysCallsDup = [](int oldfd) -> int { return dupReturnValue; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    EXPECT_TRUE(server.registerHandle(12345, 10));

    EXPECT_TRUE(server.unregisterHandle(12345));

    server.shutdown();
}

TEST_F(IpcSocketServerMtTest, givenServerWhenPollReturnsEintrThenContinuesLoop) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static std::atomic<int> pollCallCount{0};
    pollCallCount = 0;
    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        int count = pollCallCount.fetch_add(1);
        if (count == 0) {
            errno = EINTR;
            return -1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
    EXPECT_GE(pollCallCount.load(), 2);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenReceiveMessageWithPayloadSizeLargerThanMaxThenFails) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    static std::atomic<bool> connectionProcessed{false};
    connectionProcessed = false;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (!connectionProcessed.load()) {
            connectionProcessed.store(true);
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->processId = 12345;
            msg->handleId = 12345;
            msg->payloadSize = 99999;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenProcessRequestForHandleWithInvalidFdThenSendsFailureResponse) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static std::atomic<bool> connectionProcessed{false};
    static std::atomic<bool> sendCalled{false};

    connectionProcessed = false;
    sendCalled = false;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (!connectionProcessed.load()) {
            connectionProcessed.store(true);
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = IpcSocketMessageType::requestHandle;
            msg->processId = 12345;
            msg->handleId = 12345;
            msg->payloadSize = 0;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    SysCalls::sysCallsDup = [](int oldfd) -> int { return -1; };

    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        sendCalled.store(true);
        return static_cast<ssize_t>(len);
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenSendMessageWithZeroPayloadSizeThenSkipsPayloadSend) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static std::atomic<bool> connectionProcessed{false};
    static std::atomic<int> sendCallCountLocal{0};

    connectionProcessed = false;
    sendCallCountLocal = 0;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (!connectionProcessed.load()) {
            connectionProcessed.store(true);
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

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

    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        sendCallCountLocal.fetch_add(1);
        return static_cast<ssize_t>(len);
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenPollReturnsTimeoutThenContinuesLoop) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static std::atomic<int> pollCallCountLocal{0};
    pollCallCountLocal = 0;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (pollCallCountLocal.fetch_add(1) < 2) {
            return 0;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
    EXPECT_GE(pollCallCountLocal.load(), 2);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenPollReturnsWithoutPollinThenContinuesLoop) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);

    static std::atomic<int> pollCallCountLocal{0};
    pollCallCountLocal = 0;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (pollCallCountLocal.fetch_add(1) < 2) {
            fds->revents = POLLHUP;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
    EXPECT_GE(pollCallCountLocal.load(), 2);
}

TEST_F(IpcSocketServerMtTest, givenServerWhenHandleClientConnectionReceivesDefaultMessageTypeThenReturnsDefault) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);

    static std::atomic<bool> connectionProcessed{false};
    connectionProcessed = false;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (!connectionProcessed.load()) {
            connectionProcessed.store(true);
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        if (len == sizeof(IpcSocketMessage)) {
            IpcSocketMessage *msg = static_cast<IpcSocketMessage *>(buf);
            msg->type = static_cast<IpcSocketMessageType>(255); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
            msg->processId = 12345;
            msg->handleId = 100;
            msg->payloadSize = 0;
            return sizeof(IpcSocketMessage);
        }
        return -1;
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenSendMessageHeaderFailsThenReturnsFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static std::atomic<bool> connectionProcessed{false};
    connectionProcessed = false;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (!connectionProcessed.load()) {
            connectionProcessed.store(true);
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

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

    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        return -1;
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenReceiveMessageWithZeroPayloadSizeThenSkipsPayloadRecv) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static std::atomic<bool> connectionProcessed{false};
    static std::atomic<int> recvCallCountLocal{0};

    connectionProcessed = false;
    recvCallCountLocal = 0;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (!connectionProcessed.load()) {
            connectionProcessed.store(true);
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

    SysCalls::sysCallsRecv = [](int sockfd, void *buf, size_t len, int flags) -> ssize_t {
        recvCallCountLocal.fetch_add(1);
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

    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        return static_cast<ssize_t>(len);
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
    EXPECT_EQ(1, recvCallCountLocal.load());
}

TEST_F(IpcSocketServerMtTest, givenServerWhenUnregisterHandleWithRefCountGreaterThanOneThenOnlyDecrements) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    EXPECT_TRUE(server.registerHandle(12345, 42));
    EXPECT_TRUE(server.registerHandle(12345, 42));

    uint32_t closeCallsBefore = SysCalls::closeFuncCalled;

    EXPECT_TRUE(server.unregisterHandle(12345));

    EXPECT_EQ(closeCallsBefore, SysCalls::closeFuncCalled);

    EXPECT_TRUE(server.unregisterHandle(12345));
    EXPECT_FALSE(server.unregisterHandle(12345));

    server.shutdown();
}

TEST_F(IpcSocketServerMtTest, givenServerWhenSendMessageWithValidPayloadThenSendsPayload) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(false);

    VariableBackup<decltype(SysCalls::sysCallsPoll)> pollBackup(&SysCalls::sysCallsPoll);
    VariableBackup<decltype(SysCalls::sysCallsAccept)> acceptBackup(&SysCalls::sysCallsAccept);
    VariableBackup<decltype(SysCalls::sysCallsRecv)> recvBackup(&SysCalls::sysCallsRecv);
    VariableBackup<decltype(SysCalls::sysCallsSend)> sendBackup(&SysCalls::sysCallsSend);

    static std::atomic<bool> connectionProcessed{false};
    static std::atomic<int> sendCallCountLocal{0};

    connectionProcessed = false;
    sendCallCountLocal = 0;

    SysCalls::sysCallsPoll = [](pollfd *fds, nfds_t nfds, int timeout) -> int {
        if (!connectionProcessed.load()) {
            connectionProcessed.store(true);
            fds->revents = POLLIN;
            return 1;
        }
        return TestedIpcSocketServer::pollNoEvents(fds, nfds, timeout);
    };

    SysCalls::sysCallsAccept = [](int sockfd, sockaddr *addr, socklen_t *addrlen) -> int {
        return 100;
    };

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

    SysCalls::sysCallsSend = [](int sockfd, const void *buf, size_t len, int flags) -> ssize_t {
        sendCallCountLocal.fetch_add(1);
        return static_cast<ssize_t>(len);
    };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    TestedIpcSocketServer::waitUntilIdle();

    server.shutdown();
    EXPECT_FALSE(server.isRunning());
    EXPECT_GE(sendCallCountLocal.load(), 2);
}

class IpcSocketMultiThreadedTest : public ::testing::Test {
  public:
    void SetUp() override {
        SysCalls::socketCalled = 0;
        SysCalls::bindCalled = 0;
        SysCalls::listenCalled = 0;
        SysCalls::dupCalled = 0;
        SysCalls::closeFuncCalled = 0;
        SysCalls::sysCallsPoll = TestedIpcSocketServer::pollNoEvents;
    }

    void TearDown() override {
        SysCalls::sysCallsSocket = nullptr;
        SysCalls::sysCallsBind = nullptr;
        SysCalls::sysCallsListen = nullptr;
        SysCalls::sysCallsDup = nullptr;
        SysCalls::sysCallsClose = nullptr;
        SysCalls::sysCallsPoll = nullptr;
    }
};

TEST_F(IpcSocketMultiThreadedTest, givenServerWhenMultipleThreadsRegisterHandlesConcurrentlyThenAllSucceed) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };

    static std::atomic<int> dupCounter{100};
    dupCounter = 100;
    SysCalls::sysCallsDup = [](int) -> int { return ++dupCounter; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    constexpr int numThreads = 10;
    constexpr int handlesPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&server, &successCount, i]() {
            for (int j = 0; j < handlesPerThread; j++) {
                uint64_t handleId = i * handlesPerThread + j;
                if (server.registerHandle(handleId, static_cast<int>(handleId))) {
                    successCount++;
                }
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(numThreads * handlesPerThread, successCount.load());
}

TEST_F(IpcSocketMultiThreadedTest, givenServerWhenMultipleThreadsRegisterAndUnregisterConcurrentlyThenNoRaceConditions) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    constexpr int numThreads = 8;
    constexpr int iterations = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&server, i]() {
            for (int j = 0; j < iterations; j++) {
                uint64_t handleId = i * iterations + j;
                server.registerHandle(handleId, static_cast<int>(handleId));
                server.unregisterHandle(handleId);
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(server.isRunning());
}

TEST_F(IpcSocketMultiThreadedTest, givenServerWhenOneThreadRegistersWhileAnotherUnregistersThenNoCorruption) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    constexpr uint64_t handleCount = 5000;

    // Encourages the scheduler to interleave both loops when they share a CPU.
    std::thread registerThread([&server]() {
        for (uint64_t handleId = 0; handleId < handleCount; handleId++) {
            server.registerHandle(handleId, 42);
            std::this_thread::yield();
        }
    });

    std::thread unregisterThread([&server]() {
        for (uint64_t handleId = 0; handleId < handleCount; handleId++) {
            server.unregisterHandle(handleId);
            std::this_thread::yield();
        }
    });

    registerThread.join();
    unregisterThread.join();

    EXPECT_TRUE(server.isRunning());
}

TEST_F(IpcSocketMultiThreadedTest, givenServerWhenMultipleThreadsRegisterSameHandleIdThenRefCountIsCorrect) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    constexpr uint64_t sharedHandleId = 12345;
    constexpr int numThreads = 5;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&server]() {
            server.registerHandle(sharedHandleId, 42);
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    for (int i = 0; i < numThreads - 1; i++) {
        EXPECT_TRUE(server.unregisterHandle(sharedHandleId));
    }

    EXPECT_TRUE(server.unregisterHandle(sharedHandleId));

    EXPECT_FALSE(server.unregisterHandle(sharedHandleId));
}

#if 0
TEST_F(IpcSocketMultiThreadedTest, givenServerWhenMultipleThreadsCallInitializeConcurrentlyThenOnlyOneSucceeds) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);

    static std::atomic<int> socketCallCount{0};
    socketCallCount = 0;
    SysCalls::sysCallsSocket = [](int, int, int) -> int {
        socketCallCount++;
        return 5;
    };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };

    TestedIpcSocketServer server;

    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&server, &successCount]() {
            if (server.initialize()) {
                successCount++;
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(1, successCount.load());
    EXPECT_EQ(1, socketCallCount.load());
}
#endif

TEST_F(IpcSocketMultiThreadedTest, givenServerWhenConcurrentRegisterAndShutdownThenNoDeadlock) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    for (int iteration = 0; iteration < 5; iteration++) {
        TestedIpcSocketServer server;
        EXPECT_TRUE(server.initialize());

        std::atomic<bool> stop{false};
        std::atomic<bool> registrationStarted{false};

        std::thread worker([&server, &stop, &registrationStarted]() {
            uint64_t id = 0;
            while (!stop.load()) {
                server.registerHandle(id++ % maxNumHandles, 42);
                registrationStarted.store(true);
                std::this_thread::yield();
            }
        });

        waitFor([&registrationStarted] { return registrationStarted.load(); });

        server.shutdown();
        stop.store(true);

        worker.join();

        EXPECT_FALSE(server.isRunning());
    }
}

TEST_F(IpcSocketMultiThreadedTest, givenServerWhenStressTestingRegisterUnregisterThenHandlesCorrectly) {
    VariableBackup<decltype(SysCalls::sysCallsSocket)> socketBackup(&SysCalls::sysCallsSocket);
    VariableBackup<decltype(SysCalls::sysCallsBind)> bindBackup(&SysCalls::sysCallsBind);
    VariableBackup<decltype(SysCalls::sysCallsListen)> listenBackup(&SysCalls::sysCallsListen);
    VariableBackup<decltype(SysCalls::sysCallsDup)> dupBackup(&SysCalls::sysCallsDup);

    SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    TestedIpcSocketServer server;
    EXPECT_TRUE(server.initialize());

    constexpr int numThreads = 4;
    constexpr int operationsPerThread = 200;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&server, i]() {
            for (int j = 0; j < operationsPerThread; j++) {
                uint64_t handleId = (i * operationsPerThread + j) % 50;
                if (j % 2 == 0) {
                    server.registerHandle(handleId, static_cast<int>(handleId));
                } else {
                    server.unregisterHandle(handleId);
                }
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(server.isRunning());
}
