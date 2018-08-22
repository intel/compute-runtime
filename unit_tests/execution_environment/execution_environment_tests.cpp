/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/built_ins/built_ins.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/platform/platform.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

#include "test.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/utilities/destructor_counted.h"

using namespace OCLRT;

TEST(ExecutionEnvironment, givenDefaultConstructorWhenItIsCalledThenExecutionEnvironmentHasInitialRefCountZero) {
    ExecutionEnvironment environment;
    EXPECT_EQ(0, environment.getRefInternalCount());
    EXPECT_EQ(0, environment.getRefApiCount());
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsConstructedThenItCretesExecutionEnvironmentWithOneRefCountInternal) {
    std::unique_ptr<Platform> platform(new Platform);
    ASSERT_NE(nullptr, platform->peekExecutionEnvironment());
    EXPECT_EQ(1, platform->peekExecutionEnvironment()->getRefInternalCount());
}

TEST(ExecutionEnvironment, givenPlatformAndExecutionEnvironmentWithRefCountsWhenPlatformIsDestroyedThenExecutionEnvironmentIsNotDeleted) {
    std::unique_ptr<Platform> platform(new Platform);
    auto executionEnvironment = platform->peekExecutionEnvironment();
    executionEnvironment->incRefInternal();
    platform.reset();
    EXPECT_EQ(1, executionEnvironment->getRefInternalCount());
    executionEnvironment->decRefInternal();
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsInitializedAndCreatesDevicesThenThoseDevicesAddRefcountsToExecutionEnvironment) {
    std::unique_ptr<Platform> platform(new Platform);
    auto executionEnvironment = platform->peekExecutionEnvironment();

    platform->initialize();
    EXPECT_LT(0u, platform->getNumDevices());
    EXPECT_EQ(static_cast<int32_t>(1u + platform->getNumDevices()), executionEnvironment->getRefInternalCount());
}

TEST(ExecutionEnvironment, givenDeviceThatHaveRefferencesAfterPlatformIsDestroyedThenDeviceIsStillUsable) {
    std::unique_ptr<Platform> platform(new Platform);
    auto executionEnvironment = platform->peekExecutionEnvironment();
    platform->initialize();
    auto device = platform->getDevice(0);
    EXPECT_EQ(1, device->getRefInternalCount());
    device->incRefInternal();
    platform.reset(nullptr);
    EXPECT_EQ(1, device->getRefInternalCount());
    EXPECT_EQ(1, executionEnvironment->getRefInternalCount());

    device->decRefInternal();
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsCreatedThenItCreatesCommandStreamReceiverInExecutionEnvironment) {
    Platform platform;
    auto executionEnvironment = platform.peekExecutionEnvironment();
    platform.initialize();
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceiver);
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsCreatedThenItCreatesMemoryManagerInExecutionEnvironment) {
    Platform platform;
    auto executionEnvironment = platform.peekExecutionEnvironment();
    platform.initialize();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, givenDeviceWhenItIsDestroyedThenMemoryManagerIsStillAvailable) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    executionEnvironment->incRefInternal();
    std::unique_ptr<Device> device(Device::create<OCLRT::Device>(nullptr, executionEnvironment.get()));
    device.reset(nullptr);
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeCommandStreamReceiverIsCalledThenItIsInitalized) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0]);
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceiver);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenItIsInitalized) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0]);
    executionEnvironment->initializeMemoryManager(false);
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

static_assert(sizeof(ExecutionEnvironment) == sizeof(std::mutex) + (is64bit ? 80 : 44), "New members detected in ExecutionEnvironment, please ensure that destruction sequence of objects is correct");

TEST(ExecutionEnvironment, givenExecutionEnvironmentWithVariousMembersWhenItIsDestroyedThenDeleteSequenceIsSpecified) {
    uint32_t destructorId = 0u;

    struct MockExecutionEnvironment : ExecutionEnvironment {
        using ExecutionEnvironment::gmmHelper;
    };
    struct GmmHelperMock : public DestructorCounted<GmmHelper, 6> {
        GmmHelperMock(uint32_t &destructorId, const HardwareInfo *hwInfo) : DestructorCounted(destructorId, hwInfo) {}
    };
    struct OsInterfaceMock : public DestructorCounted<OSInterface, 5> {
        OsInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct MemoryMangerMock : public DestructorCounted<OsAgnosticMemoryManager, 4> {
        MemoryMangerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct CommandStreamReceiverMock : public DestructorCounted<MockCommandStreamReceiver, 3> {
        CommandStreamReceiverMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct BuiltinsMock : public DestructorCounted<BuiltIns, 2> {
        BuiltinsMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct CompilerInterfaceMock : public DestructorCounted<CompilerInterface, 1> {
        CompilerInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct SourceLevelDebuggerMock : public DestructorCounted<SourceLevelDebugger, 0> {
        SourceLevelDebuggerMock(uint32_t &destructorId) : DestructorCounted(destructorId, nullptr) {}
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->gmmHelper = std::make_unique<GmmHelperMock>(destructorId, platformDevices[0]);
    executionEnvironment->osInterface = std::make_unique<OsInterfaceMock>(destructorId);
    executionEnvironment->memoryManager = std::make_unique<MemoryMangerMock>(destructorId);
    executionEnvironment->commandStreamReceiver = std::make_unique<CommandStreamReceiverMock>(destructorId);
    executionEnvironment->builtins = std::make_unique<BuiltinsMock>(destructorId);
    executionEnvironment->compilerInterface = std::make_unique<CompilerInterfaceMock>(destructorId);
    executionEnvironment->sourceLevelDebugger = std::make_unique<SourceLevelDebuggerMock>(destructorId);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(7u, destructorId);
}

TEST(ExecutionEnvironment, givenMultipleDevicesWhenTheyAreCreatedTheyAllReuseTheSameMemoryManagerAndCommandStreamReceiver) {
    auto executionEnvironment = new ExecutionEnvironment;
    std::unique_ptr<Device> device(Device::create<OCLRT::Device>(nullptr, executionEnvironment));
    auto &commandStreamReceiver = device->getCommandStreamReceiver();
    auto memoryManager = device->getMemoryManager();

    std::unique_ptr<Device> device2(Device::create<OCLRT::Device>(nullptr, executionEnvironment));
    EXPECT_EQ(&commandStreamReceiver, &device->getCommandStreamReceiver());
    EXPECT_EQ(memoryManager, device2->getMemoryManager());
}

typedef ::testing::Test ExecutionEnvironmentHw;

HWTEST_F(ExecutionEnvironmentHw, givenExecutionEnvironmentWhenCommandStreamReceiverIsInitializedForCompressedBuffersThenCreatePageTableManagerIsCalled) {
    ExecutionEnvironment executionEnvironment;
    HardwareInfo localHwInfo = *platformDevices[0];
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    executionEnvironment.initializeCommandStreamReceiver(&localHwInfo);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceiver.get());
    ASSERT_NE(nullptr, csr);
    EXPECT_TRUE(csr->createPageTableManagerCalled);
}

HWTEST_F(ExecutionEnvironmentHw, givenExecutionEnvironmentWhenCommandStreamReceiverIsInitializedForCompressedImagesThenCreatePageTableManagerIsCalled) {
    ExecutionEnvironment executionEnvironment;
    HardwareInfo localHwInfo = *platformDevices[0];
    localHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    executionEnvironment.initializeCommandStreamReceiver(&localHwInfo);
    EXPECT_NE(nullptr, executionEnvironment.commandStreamReceiver);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceiver.get());
    ASSERT_NE(nullptr, csr);
    EXPECT_TRUE(csr->createPageTableManagerCalled);
}
