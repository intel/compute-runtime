/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/tbx_in_process_simulator.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_os_library.h"

#include "gtest/gtest.h"

#include <atomic>
#include <cstring>
#include <thread>

namespace NEO {

class JoinTrackingThread : public Thread {
  public:
    explicit JoinTrackingThread(std::unique_ptr<Thread> thread) : thread(std::move(thread)) {}
    void join() override {
        joinEntered = true;
        thread->join();
    }
    void detach() override {
        thread->detach();
    }
    void yield() override {
        thread->yield();
    }

    static inline std::atomic<bool> joinEntered{false};

  protected:
    std::unique_ptr<Thread> thread;
};

class TrackedOsLibrary : public MockOsLibrary {
  public:
    using MockOsLibrary::MockOsLibrary;
    ~TrackedOsLibrary() override {
        libraryDestroyed = true;
    }

    static inline std::atomic<bool> libraryDestroyed{false};
};

struct TbxInProcessSimulatorMtTests : public ::testing::Test {
    void SetUp() override {
        TrackedOsLibrary::libraryDestroyed = false;
        JoinTrackingThread::joinEntered = false;
        mainEntered = false;
        mainReleased = false;
        mainFinished = false;
        libraryAliveWhenMainFinished = false;
        argumentsValidWhenMainFinished = false;
        createRealThread = Thread::createFunc;
        Thread::createFunc = TbxInProcessSimulatorMtTests::createJoinTrackingThread;
    }

    void TearDown() override {
        Thread::createFunc = createRealThread;
    }

    static OsLibrary *loadLibrary(const OsLibraryCreateProperties &) {
        return new TrackedOsLibrary(reinterpret_cast<void *>(&TbxInProcessSimulatorMtTests::simulatorMain), true);
    }

    static std::unique_ptr<Thread> createJoinTrackingThread(void *(*function)(void *), void *argument) {
        return std::make_unique<JoinTrackingThread>(createRealThread(function, argument));
    }

    static int simulatorMain(int argc, const char **argv) {
        mainEntered = true;
        while (!mainReleased) {
            std::this_thread::yield();
        }
        libraryAliveWhenMainFinished = !TrackedOsLibrary::libraryDestroyed;
        argumentsValidWhenMainFinished = (argc == 3) && (strcmp(argv[0], "simulator.dll") == 0) &&
                                         (strcmp(argv[1], "-device") == 0) && (strcmp(argv[2], "device.xml") == 0);
        mainFinished = true;
        return 0;
    }

    static inline decltype(Thread::createFunc) createRealThread = nullptr;
    static inline std::atomic<bool> mainEntered{false};
    static inline std::atomic<bool> mainReleased{false};
    static inline std::atomic<bool> mainFinished{false};
    static inline std::atomic<bool> libraryAliveWhenMainFinished{false};
    static inline std::atomic<bool> argumentsValidWhenMainFinished{false};

    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, TbxInProcessSimulatorMtTests::loadLibrary};
};

TEST_F(TbxInProcessSimulatorMtTests, givenRunningSimulatorWhenDestroyingSimulatorThenDestructionWaitsForMainToFinishWhileLibraryAndArgumentsStayAlive) {
    auto simulator = TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments("-device;device.xml"));
    ASSERT_NE(nullptr, simulator);

    while (!mainEntered) {
        std::this_thread::yield();
    }

    std::atomic<bool> simulatorDestroyed{false};
    std::thread destroyingThread([&simulator, &simulatorDestroyed]() {
        simulator.reset();
        simulatorDestroyed = true;
    });

    while (!JoinTrackingThread::joinEntered) {
        std::this_thread::yield();
    }
    EXPECT_FALSE(simulatorDestroyed);
    EXPECT_FALSE(mainFinished);
    EXPECT_FALSE(TrackedOsLibrary::libraryDestroyed);

    mainReleased = true;
    destroyingThread.join();

    EXPECT_TRUE(simulatorDestroyed);
    EXPECT_TRUE(mainFinished);
    EXPECT_TRUE(libraryAliveWhenMainFinished);
    EXPECT_TRUE(argumentsValidWhenMainFinished);
    EXPECT_TRUE(TrackedOsLibrary::libraryDestroyed);
}

} // namespace NEO
