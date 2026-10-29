/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/tbx_in_process_simulator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_os_library.h"

#include "gtest/gtest.h"

#include <string>
#include <vector>

namespace NEO {

class TbxInProcessSimulatorWhitebox : public TbxInProcessSimulator {
  public:
    using TbxInProcessSimulator::getLibraryLoadFlags;
    using TbxInProcessSimulator::mainFunctionName;
};

class RecordingThread : public Thread {
  public:
    void join() override {
        joinCount++;
    }
    void detach() override {}
    void yield() override {}

    static inline uint32_t joinCount = 0u;
};

struct TbxInProcessSimulatorTests : public ::testing::Test {
    void SetUp() override {
        RecordingThread::joinCount = 0u;
        threadFunction = nullptr;
        threadArgument = nullptr;
        passedArguments.clear();
        passedLibraryName.clear();
        passedLoadFlags = nullptr;
        mainFunctionCallCount = 0u;
        mainFunctionExitCode = 0;
    }

    void TearDown() override {
        passedArguments.clear();
        passedArguments.shrink_to_fit();
        passedLibraryName.clear();
        passedLibraryName.shrink_to_fit();
    }

    static OsLibrary *loadLibrary(const OsLibraryCreateProperties &properties) {
        passedLibraryName = properties.libraryName;
        passedLoadFlags = properties.customLoadFlags;
        auto library = libraryToLoad;
        libraryToLoad = nullptr;
        return library;
    }

    static std::unique_ptr<Thread> createThread(void *(*function)(void *), void *argument) {
        threadFunction = function;
        threadArgument = argument;
        return std::make_unique<RecordingThread>();
    }

    static int simulatorMain(int argc, const char **argv) {
        mainFunctionCallCount++;
        for (int i = 0; i < argc; i++) {
            passedArguments.push_back(argv[i]);
        }
        return mainFunctionExitCode;
    }

    static inline OsLibrary *libraryToLoad = nullptr;
    static inline std::string passedLibraryName;
    static inline int *passedLoadFlags = nullptr;
    static inline void *(*threadFunction)(void *) = nullptr;
    static inline void *threadArgument = nullptr;
    static inline std::vector<std::string> passedArguments;
    static inline uint32_t mainFunctionCallCount = 0u;
    static inline int mainFunctionExitCode = 0;

    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, TbxInProcessSimulatorTests::loadLibrary};
    VariableBackup<decltype(Thread::createFunc)> createThreadBackup{&Thread::createFunc, TbxInProcessSimulatorTests::createThread};
};

TEST_F(TbxInProcessSimulatorTests, givenLibraryWhichCannotBeLoadedWhenStartingSimulatorThenNullptrIsReturnedAndNoThreadIsCreated) {
    libraryToLoad = nullptr;

    EXPECT_EQ(nullptr, TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments("-a")));
    EXPECT_EQ(nullptr, threadFunction);
}

TEST_F(TbxInProcessSimulatorTests, givenNotLoadedLibraryWhenStartingSimulatorThenNullptrIsReturnedAndNoThreadIsCreated) {
    libraryToLoad = new MockOsLibrary(reinterpret_cast<void *>(&TbxInProcessSimulatorTests::simulatorMain), false);

    EXPECT_EQ(nullptr, TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments("-a")));
    EXPECT_EQ(nullptr, threadFunction);
}

TEST_F(TbxInProcessSimulatorTests, givenLibraryWithoutMainFunctionWhenStartingSimulatorThenNullptrIsReturnedAndNoThreadIsCreated) {
    auto library = new MockOsLibrary(nullptr, true);
    libraryToLoad = library;

    EXPECT_EQ(nullptr, TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments("-a")));
    EXPECT_EQ(nullptr, threadFunction);
}

TEST_F(TbxInProcessSimulatorTests, givenLibraryWithMainFunctionWhenStartingSimulatorThenMainIsRunOnThreadWithLibraryPathAndSplitArgumentsAndThreadIsJoinedOnDestruction) {
    auto library = new MockOsLibrary(reinterpret_cast<void *>(&TbxInProcessSimulatorTests::simulatorMain), true);
    libraryToLoad = library;

    auto simulator = TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments(" -device  :config/device.xml\t-socket tcp:5000 "));
    ASSERT_NE(nullptr, simulator);
    EXPECT_STREQ(TbxInProcessSimulatorWhitebox::mainFunctionName, library->lastRequestedProcName.c_str());
    EXPECT_STREQ("simulator.dll", passedLibraryName.c_str());
    EXPECT_EQ(TbxInProcessSimulatorWhitebox::getLibraryLoadFlags(), passedLoadFlags);
    ASSERT_NE(nullptr, threadFunction);
    EXPECT_EQ(0u, mainFunctionCallCount);

    EXPECT_EQ(nullptr, threadFunction(threadArgument));

    EXPECT_EQ(1u, mainFunctionCallCount);
    std::vector<std::string> expectedArguments = {"simulator.dll", "-device", ":config/device.xml", "-socket", "tcp:5000"};
    EXPECT_EQ(expectedArguments, passedArguments);

    EXPECT_EQ(0u, RecordingThread::joinCount);
    simulator.reset();
    EXPECT_EQ(1u, RecordingThread::joinCount);
}

TEST_F(TbxInProcessSimulatorTests, givenEmptyArgumentsWhenStartedSimulatorRunsThenOnlyLibraryPathIsPassed) {
    libraryToLoad = new MockOsLibrary(reinterpret_cast<void *>(&TbxInProcessSimulatorTests::simulatorMain), true);

    auto simulator = TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments(""));
    ASSERT_NE(nullptr, simulator);
    threadFunction(threadArgument);

    std::vector<std::string> expectedArguments = {"simulator.dll"};
    EXPECT_EQ(expectedArguments, passedArguments);
}

TEST_F(TbxInProcessSimulatorTests, givenSemicolonSeparatedArgumentsWhenStartedSimulatorRunsThenArgumentsAreSplitOnSemicolonsOnlyAndKeepSpaces) {
    libraryToLoad = new MockOsLibrary(reinterpret_cast<void *>(&TbxInProcessSimulatorTests::simulatorMain), true);

    auto simulator = TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments("-device;C:/Program Files/device.xml;;-socket;tcp:5000;"));
    ASSERT_NE(nullptr, simulator);
    threadFunction(threadArgument);

    std::vector<std::string> expectedArguments = {"simulator.dll", "-device", "C:/Program Files/device.xml", "-socket", "tcp:5000"};
    EXPECT_EQ(expectedArguments, passedArguments);
}

TEST_F(TbxInProcessSimulatorTests, givenPrintDebugMessagesDisabledAndSimulatorMainReturningErrorWhenSimulatorThreadFinishesThenNothingIsPrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDebugMessages.set(false);
    libraryToLoad = new MockOsLibrary(reinterpret_cast<void *>(&TbxInProcessSimulatorTests::simulatorMain), true);
    mainFunctionExitCode = 3;

    auto simulator = TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments(""));
    ASSERT_NE(nullptr, simulator);
    testing::internal::CaptureStderr();
    threadFunction(threadArgument);
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(TbxInProcessSimulatorTests, givenPrintDebugMessagesEnabledAndSimulatorMainReturningZeroWhenSimulatorThreadFinishesThenNothingIsPrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDebugMessages.set(true);
    libraryToLoad = new MockOsLibrary(reinterpret_cast<void *>(&TbxInProcessSimulatorTests::simulatorMain), true);

    auto simulator = TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments(""));
    ASSERT_NE(nullptr, simulator);
    testing::internal::CaptureStderr();
    threadFunction(threadArgument);
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(TbxInProcessSimulatorTests, givenPrintDebugMessagesEnabledAndSimulatorMainReturningErrorWhenSimulatorThreadFinishesThenExitCodeIsPrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDebugMessages.set(true);
    libraryToLoad = new MockOsLibrary(reinterpret_cast<void *>(&TbxInProcessSimulatorTests::simulatorMain), true);
    mainFunctionExitCode = 3;

    auto simulator = TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments(""));
    ASSERT_NE(nullptr, simulator);
    testing::internal::CaptureStderr();
    threadFunction(threadArgument);
    EXPECT_EQ(std::string("TBX in-process simulator exited with code 3\n"), testing::internal::GetCapturedStderr());
}

TEST(TbxInProcessSimulatorArgumentsTests, givenSocketAndSingleSharedMemoryFileWhenReplacingRootDeviceArgumentsThenPortAndFileAreReplaced) {
    auto arguments = TbxInProcessSimulator::splitArguments("-device;dev.xml;-socket;tcp:5000;-shmname;sys.bin;-shmversion;4");

    TbxInProcessSimulator::replaceRootDeviceArguments(arguments, 5001u, {"sys_1.bin"});

    std::vector<std::string> expectedArguments = {"-device", "dev.xml", "-socket", "tcp:5001", "-shmname", "sys_1.bin", "-shmversion", "4"};
    EXPECT_EQ(expectedArguments, arguments);
}

TEST(TbxInProcessSimulatorArgumentsTests, givenTwoSharedMemoryFilesAtEndWhenReplacingRootDeviceArgumentsThenBothFilesAreReplacedInOrder) {
    auto arguments = TbxInProcessSimulator::splitArguments("-socket 5000 -shmname local.bin sys.bin");

    TbxInProcessSimulator::replaceRootDeviceArguments(arguments, 5002u, {"local_2.bin", "sys_2.bin"});

    std::vector<std::string> expectedArguments = {"-socket", "5002", "-shmname", "local_2.bin", "sys_2.bin"};
    EXPECT_EQ(expectedArguments, arguments);
}

TEST(TbxInProcessSimulatorArgumentsTests, givenArgumentsWithoutSocketAndSharedMemoryOptionsWhenReplacingRootDeviceArgumentsThenArgumentsAreUnchanged) {
    auto arguments = TbxInProcessSimulator::splitArguments("-device dev.xml -socket");
    const auto originalArguments = arguments;

    TbxInProcessSimulator::replaceRootDeviceArguments(arguments, 5001u, {"sys_1.bin"});

    EXPECT_EQ(originalArguments, arguments);
}

TEST_F(TbxInProcessSimulatorTests, givenThreadCreationFailureWhenStartingSimulatorThenNullptrIsReturnedAndNothingIsJoined) {
    libraryToLoad = new MockOsLibrary(reinterpret_cast<void *>(&TbxInProcessSimulatorTests::simulatorMain), true);
    VariableBackup<decltype(Thread::createFunc)> failingThreadBackup{&Thread::createFunc, [](void *(*)(void *), void *) -> std::unique_ptr<Thread> { return nullptr; }};

    EXPECT_EQ(nullptr, TbxInProcessSimulator::start("simulator.dll", TbxInProcessSimulator::splitArguments("-a")));
    EXPECT_EQ(0u, mainFunctionCallCount);
    EXPECT_EQ(0u, RecordingThread::joinCount);
}

} // namespace NEO
