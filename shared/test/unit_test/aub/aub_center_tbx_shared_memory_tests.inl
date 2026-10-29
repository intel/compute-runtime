/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/mocks/mock_os_thread.h"

#include "aubstream/aubstream.h"
#include "gtest/gtest.h"

#include <string>
#include <vector>

namespace NEO {

struct AubCenterTbxSharedMemoryTests : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.UseAubStream.set(true);
        debugManager.flags.PrintDebugMessages.set(true);
        debugManager.flags.HBMSizePerTileInGigabytes.set(1);
        debugManager.flags.TbxSharedSystemMemoryFile.set("system_memory.bin");
        physicalMemorySize = GfxCoreHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()) * MemoryConstants::gigaByte;
        translationTableSize = physicalMemorySize / MemoryConstants::pageSize * sizeof(uint64_t);
    }

    MockAubManager *getAubManager(MockAubCenter &aubCenter) {
        return static_cast<MockAubManager *>(aubCenter.aubManager.get());
    }

    static std::string tooSmallFileMessage(const char *memoryName, uint64_t fileSize, uint64_t requiredSize) {
        return std::string("TBX shared ") + memoryName + " memory file is too small: " + std::to_string(fileSize) + " bytes, " +
               std::to_string(requiredSize) + " required, falling back to TBX socket mode\n";
    }

    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment{};
    RootDeviceEnvironment &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    MappedFileSysCallsMock sysCallsMock;
    alignas(MemoryConstants::pageSize) uint8_t systemMemory[MemoryConstants::pageSize]{};
    alignas(MemoryConstants::pageSize) uint8_t localMemory[2 * MemoryConstants::pageSize]{};
    uint64_t physicalMemorySize = 0;
    uint64_t translationTableSize = 0;
};

TEST_F(AubCenterTbxSharedMemoryTests, givenMappableSharedSystemMemoryFileWhenCreatingTbxAubCenterThenTbxShmModeWithSharedSystemMemoryIsUsed) {
    sysCallsMock.makeFileMappable(0u, systemMemory, physicalMemorySize);

    {
        MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
        auto aubManager = getAubManager(aubCenter);
        ASSERT_NE(nullptr, aubManager);

        EXPECT_EQ(aub_stream::mode::tbxShm, aubCenter.aubStreamMode);
        EXPECT_EQ(aub_stream::mode::tbxShm, aubManager->options.mode);
        EXPECT_EQ(systemMemory, aubManager->options.sharedMemoryInfo.sysMemBase);
        EXPECT_EQ(physicalMemorySize, aubManager->options.sharedMemoryInfo.sysMemSize);
        EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.localMemBase);
        EXPECT_EQ(0u, aubManager->options.sharedMemoryInfo.localMemSize);
        EXPECT_EQ(1u, sysCallsMock.getOpenCount());
        EXPECT_EQ(0u, sysCallsMock.getUnmapCount());
    }
    EXPECT_EQ(1u, sysCallsMock.getUnmapCount());
}

TEST_F(AubCenterTbxSharedMemoryTests, givenMappableSharedSystemAndLocalMemoryFilesWhenCreatingTbxWithAubAubCenterThenAubFileAndShmModeWithBothSharedMemoriesIsUsed) {
    debugManager.flags.TbxSharedLocalMemoryFile.set("local_memory.bin");
    sysCallsMock.makeFileMappable(0u, systemMemory, sizeof(systemMemory));
    sysCallsMock.makeFileMappable(1u, localMemory, physicalMemorySize);

    {
        MockAubCenter aubCenter(rootDeviceEnvironment, true, "", CommandStreamReceiverType::tbxWithAub);
        auto aubManager = getAubManager(aubCenter);
        ASSERT_NE(nullptr, aubManager);

        EXPECT_EQ(aub_stream::mode::aubFileAndShm, aubCenter.aubStreamMode);
        EXPECT_EQ(aub_stream::mode::aubFileAndShm, aubManager->options.mode);
        EXPECT_EQ(systemMemory, aubManager->options.sharedMemoryInfo.sysMemBase);
        EXPECT_EQ(sizeof(systemMemory), aubManager->options.sharedMemoryInfo.sysMemSize);
        EXPECT_EQ(localMemory, aubManager->options.sharedMemoryInfo.localMemBase);
        EXPECT_EQ(physicalMemorySize, aubManager->options.sharedMemoryInfo.localMemSize);
        EXPECT_EQ(2u, sysCallsMock.getOpenCount());
    }
    EXPECT_EQ(2u, sysCallsMock.getUnmapCount());
}

TEST_F(AubCenterTbxSharedMemoryTests, givenNotOpenableSharedSystemMemoryFileWhenCreatingTbxAubCenterThenTbxModeWithoutSharedMemoryIsUsed) {
    debugManager.flags.TbxSharedLocalMemoryFile.set("local_memory.bin");
    sysCallsMock.makeFileNotOpenable(0u);

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, true, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(std::string("Failed to map TBX shared system memory file: system_memory.bin, falling back to TBX socket mode\n"), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubCenter.aubStreamMode);
    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.sysMemBase);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.localMemBase);
    EXPECT_EQ(1u, sysCallsMock.getOpenCount());
}

TEST_F(AubCenterTbxSharedMemoryTests, givenPrintDebugMessagesDisabledAndNotOpenableSharedSystemMemoryFileWhenCreatingTbxAubCenterThenNothingIsPrintedAndTbxModeIsUsed) {
    debugManager.flags.PrintDebugMessages.set(false);
    sysCallsMock.makeFileNotOpenable(0u);

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
}

TEST_F(AubCenterTbxSharedMemoryTests, givenNotOpenableSharedLocalMemoryFileWhenCreatingTbxAubCenterThenSharedSystemMemoryIsUnmappedAndTbxModeIsUsed) {
    debugManager.flags.TbxSharedLocalMemoryFile.set("local_memory.bin");
    sysCallsMock.makeFileMappable(0u, systemMemory, sizeof(systemMemory));
    sysCallsMock.makeFileNotOpenable(1u);

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, true, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(std::string("Failed to map TBX shared local memory file: local_memory.bin, falling back to TBX socket mode\n"), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubCenter.aubStreamMode);
    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.sysMemBase);
    EXPECT_EQ(0u, aubManager->options.sharedMemoryInfo.sysMemSize);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.localMemBase);
    EXPECT_EQ(2u, sysCallsMock.getOpenCount());
    EXPECT_EQ(1u, sysCallsMock.getUnmapCount());
}

TEST_F(AubCenterTbxSharedMemoryTests, givenLocalMemoryEnabledAndSharedLocalMemoryFileNotSetWhenCreatingTbxAubCenterThenSharedSystemMemoryIsUnmappedAndTbxModeIsUsed) {
    sysCallsMock.makeFileMappable(0u, systemMemory, physicalMemorySize);

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, true, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(std::string("TbxSharedLocalMemoryFile is required with local memory, falling back to TBX socket mode\n"), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.sysMemBase);
    EXPECT_EQ(1u, sysCallsMock.getOpenCount());
    EXPECT_EQ(1u, sysCallsMock.getUnmapCount());
}

TEST_F(AubCenterTbxSharedMemoryTests, givenLocalMemoryEnabledAndTooSmallSharedLocalMemoryFileWhenCreatingTbxAubCenterThenSharedMemoriesAreUnmappedAndTbxModeIsUsed) {
    debugManager.flags.TbxSharedLocalMemoryFile.set("local_memory.bin");
    sysCallsMock.makeFileMappable(0u, systemMemory, sizeof(systemMemory));
    sysCallsMock.makeFileMappable(1u, localMemory, physicalMemorySize - MemoryConstants::pageSize);

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, true, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(tooSmallFileMessage("local", physicalMemorySize - MemoryConstants::pageSize, physicalMemorySize), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.sysMemBase);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.localMemBase);
    EXPECT_EQ(2u, sysCallsMock.getUnmapCount());
}

TEST_F(AubCenterTbxSharedMemoryTests, givenLocalMemoryDisabledAndTooSmallSharedSystemMemoryFileWhenCreatingTbxAubCenterThenSharedSystemMemoryIsUnmappedAndTbxModeIsUsed) {
    sysCallsMock.makeFileMappable(0u, systemMemory, sizeof(systemMemory));

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(tooSmallFileMessage("system", sizeof(systemMemory), physicalMemorySize), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.sysMemBase);
    EXPECT_EQ(1u, sysCallsMock.getUnmapCount());
}

TEST_F(AubCenterTbxSharedMemoryTests, givenSecondRootDeviceWhenCreatingTbxAubCenterThenSharedMemoryFilesWithRootDeviceIndexAreUsed) {
    MockExecutionEnvironment multiRootDeviceExecutionEnvironment(defaultHwInfo.get(), true, 2u);
    debugManager.flags.TbxSharedLocalMemoryFile.set("local_memory.bin");
    sysCallsMock.makeFileMappable(0u, systemMemory, sizeof(systemMemory));
    sysCallsMock.makeFileNotOpenable(1u);

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(*multiRootDeviceExecutionEnvironment.rootDeviceEnvironments[1], true, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(std::string("Failed to map TBX shared local memory file: local_memory_1.bin, falling back to TBX socket mode\n"), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_EQ(2u, sysCallsMock.getOpenCount());
}

TEST_F(AubCenterTbxSharedMemoryTests, givenSecondRootDeviceAndNotOpenableSharedSystemMemoryFileWhenCreatingTbxAubCenterThenFileNameWithRootDeviceIndexIsReported) {
    MockExecutionEnvironment multiRootDeviceExecutionEnvironment(defaultHwInfo.get(), true, 2u);
    sysCallsMock.makeFileNotOpenable(0u);

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(*multiRootDeviceExecutionEnvironment.rootDeviceEnvironments[1], false, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(std::string("Failed to map TBX shared system memory file: system_memory_1.bin, falling back to TBX socket mode\n"), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
}

TEST_F(AubCenterTbxSharedMemoryTests, givenSharedSystemMemoryFileNotSetWhenCreatingTbxAubCenterThenNoFileIsOpenedAndTbxModeIsUsedSilently) {
    debugManager.flags.TbxSharedSystemMemoryFile.set("unk");
    debugManager.flags.TbxSharedLocalMemoryFile.set("local_memory.bin");

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, true, "", CommandStreamReceiverType::tbx);
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.sysMemBase);
    EXPECT_EQ(0u, sysCallsMock.getOpenCount());
}

TEST_F(AubCenterTbxSharedMemoryTests, givenSharedSystemMemoryFileSetWhenCreatingAubFileAubCenterThenNoFileIsOpenedAndAubFileModeIsUsed) {
    sysCallsMock.makeFileMappable(0u, systemMemory, physicalMemorySize);

    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::aub);
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::aubFile, aubManager->options.mode);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.sysMemBase);
    EXPECT_EQ(0u, sysCallsMock.getOpenCount());
}

struct AubCenterTbxInProcessSimulatorTests : public AubCenterTbxSharedMemoryTests {
    void SetUp() override {
        AubCenterTbxSharedMemoryTests::SetUp();
        threadFunction = nullptr;
        threadArgument = nullptr;
        debugManager.flags.TbxInProcessSimulatorLibrary.set("simulator.dll");
        debugManager.flags.TbxInProcessSimulatorArguments.set("-socket tcp:5000");
        sysCallsMock.makeFileMappable(0u, systemMemory, translationTableSize);
    }

    void TearDown() override {
        passedArguments.clear();
        passedArguments.shrink_to_fit();
        AubCenterTbxSharedMemoryTests::TearDown();
    }

    static int simulatorMain(int argc, const char **argv) {
        for (int i = 0; i < argc; i++) {
            passedArguments.push_back(argv[i]);
        }
        return 0;
    }

    static OsLibrary *loadSimulatorLibrary(const OsLibraryCreateProperties &properties) {
        return new MockOsLibrary(reinterpret_cast<void *>(&AubCenterTbxInProcessSimulatorTests::simulatorMain), true);
    }

    static OsLibrary *failToLoadSimulatorLibrary(const OsLibraryCreateProperties &properties) {
        return nullptr;
    }

    static std::unique_ptr<Thread> createThread(void *(*function)(void *), void *argument) {
        threadFunction = function;
        threadArgument = argument;
        return std::make_unique<MockThread>();
    }

    static inline void *(*threadFunction)(void *) = nullptr;
    static inline void *threadArgument = nullptr;
    static inline std::vector<std::string> passedArguments;

    VariableBackup<decltype(Thread::createFunc)> createThreadBackup{&Thread::createFunc, AubCenterTbxInProcessSimulatorTests::createThread};
};

TEST_F(AubCenterTbxInProcessSimulatorTests, givenInProcessSimulatorWhichStartsWhenCreatingTbxAubCenterThenTranslationTableModeIsUsedAndAllocationMemoryIsShared) {
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, AubCenterTbxInProcessSimulatorTests::loadSimulatorLibrary};

    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbxShm4, aubManager->options.mode);
    EXPECT_EQ(systemMemory, aubManager->options.sharedMemoryInfo.sysMemBase);
    EXPECT_TRUE(aubCenter.isAllocationMemorySharedWithSimulator());
}

TEST_F(AubCenterTbxInProcessSimulatorTests, givenFirstRootDeviceWhenCreatingTbxAubCenterThenSimulatorArgumentsAreNotChanged) {
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, AubCenterTbxInProcessSimulatorTests::loadSimulatorLibrary};
    debugManager.flags.CreateMultipleRootDevices.set(2);
    debugManager.flags.TbxPort.set(5000);
    debugManager.flags.TbxInProcessSimulatorArguments.set("-socket;tcp:6000;-shmname;other.bin");
    MockExecutionEnvironment multiRootDeviceExecutionEnvironment(defaultHwInfo.get(), true, 2u);

    MockAubCenter aubCenter(*multiRootDeviceExecutionEnvironment.rootDeviceEnvironments[0], false, "", CommandStreamReceiverType::tbx);
    ASSERT_NE(nullptr, threadFunction);
    threadFunction(threadArgument);

    std::vector<std::string> expectedArguments = {"simulator.dll", "-socket", "tcp:6000", "-shmname", "other.bin"};
    EXPECT_EQ(expectedArguments, passedArguments);
}

TEST_F(AubCenterTbxInProcessSimulatorTests, givenSecondRootDeviceWhenCreatingTbxAubCenterThenSimulatorUsesRootDevicePortAndSharedMemoryFiles) {
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, AubCenterTbxInProcessSimulatorTests::loadSimulatorLibrary};
    debugManager.flags.CreateMultipleRootDevices.set(2);
    debugManager.flags.TbxPort.set(5000);
    debugManager.flags.TbxSharedLocalMemoryFile.set("local_memory.bin");
    debugManager.flags.TbxInProcessSimulatorArguments.set("-socket;tcp:5000;-shmname;local_memory.bin;system_memory.bin;-shmversion;4");
    sysCallsMock.makeFileMappable(1u, localMemory, translationTableSize);
    MockExecutionEnvironment multiRootDeviceExecutionEnvironment(defaultHwInfo.get(), true, 2u);

    MockAubCenter aubCenter(*multiRootDeviceExecutionEnvironment.rootDeviceEnvironments[1], true, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(aub_stream::mode::tbxShm4, getAubManager(aubCenter)->options.mode);
    ASSERT_NE(nullptr, threadFunction);
    threadFunction(threadArgument);

    std::vector<std::string> expectedArguments = {"simulator.dll", "-socket", "tcp:5001", "-shmname", "local_memory_1.bin", "system_memory_1.bin", "-shmversion", "4"};
    EXPECT_EQ(expectedArguments, passedArguments);
}

TEST_F(AubCenterTbxInProcessSimulatorTests, givenInProcessSimulatorWithDefaultArgumentsWhenCreatingTbxWithAubAubCenterThenAubFileTranslationTableModeIsUsedAndAllocationMemoryIsNotShared) {
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, AubCenterTbxInProcessSimulatorTests::loadSimulatorLibrary};
    debugManager.flags.TbxInProcessSimulatorArguments.set("unk");

    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbxWithAub);
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::aubFileAndShm4, aubManager->options.mode);
    EXPECT_FALSE(aubCenter.isAllocationMemorySharedWithSimulator());
}

TEST_F(AubCenterTbxInProcessSimulatorTests, givenInProcessSimulatorAndTooSmallTranslationTableWhenCreatingTbxAubCenterThenSimulatorIsNotStartedAndTbxModeIsUsed) {
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, AubCenterTbxInProcessSimulatorTests::loadSimulatorLibrary};
    sysCallsMock.makeFileMappable(0u, systemMemory, translationTableSize - MemoryConstants::pageSize);

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(tooSmallFileMessage("system", translationTableSize - MemoryConstants::pageSize, translationTableSize), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_FALSE(aubCenter.isAllocationMemorySharedWithSimulator());
    EXPECT_EQ(1u, sysCallsMock.getUnmapCount());
}

TEST_F(AubCenterTbxInProcessSimulatorTests, givenInProcessSimulatorAndSharedSystemMemoryFileNotSetWhenCreatingTbxAubCenterThenFallbackIsReportedAndTbxModeIsUsed) {
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, AubCenterTbxInProcessSimulatorTests::loadSimulatorLibrary};
    debugManager.flags.TbxSharedSystemMemoryFile.set("unk");

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(std::string("TbxInProcessSimulatorLibrary requires TbxSharedSystemMemoryFile, falling back to TBX socket mode\n"), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_EQ(0u, sysCallsMock.getOpenCount());
}

TEST_F(AubCenterTbxInProcessSimulatorTests, givenInProcessSimulatorWhichFailsToStartWhenCreatingTbxAubCenterThenSharedMemoryIsUnmappedAndTbxModeIsUsed) {
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, AubCenterTbxInProcessSimulatorTests::failToLoadSimulatorLibrary};

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(std::string("Failed to start TBX in-process simulator: simulator.dll, falling back to TBX socket mode\n"), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
    EXPECT_EQ(nullptr, aubManager->options.sharedMemoryInfo.sysMemBase);
    EXPECT_EQ(0u, aubManager->options.sharedMemoryInfo.sysMemSize);
    EXPECT_EQ(1u, sysCallsMock.getUnmapCount());
    EXPECT_FALSE(aubCenter.isAllocationMemorySharedWithSimulator());
}

TEST_F(AubCenterTbxInProcessSimulatorTests, givenInProcessSimulatorAndNotMappableSharedMemoryWhenCreatingTbxAubCenterThenSimulatorIsNotStartedAndTbxModeIsUsed) {
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, AubCenterTbxInProcessSimulatorTests::failToLoadSimulatorLibrary};
    sysCallsMock.makeFileNotOpenable(0u);

    testing::internal::CaptureStderr();
    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(std::string("Failed to map TBX shared system memory file: system_memory.bin, falling back to TBX socket mode\n"), testing::internal::GetCapturedStderr());
    auto aubManager = getAubManager(aubCenter);
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(aub_stream::mode::tbx, aubManager->options.mode);
}

} // namespace NEO
