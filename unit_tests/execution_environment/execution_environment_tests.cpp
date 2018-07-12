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

#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/mocks/mock_csr.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

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

auto destructorId = 0u;
static_assert(sizeof(ExecutionEnvironment) == (is64bit ? 56 : 32), "New members detected in ExecutionEnvironment, please ensure that destruction sequence of objects is correct");

TEST(ExecutionEnvironment, givenExecutionEnvironmentWithVariousMembersWhenItIsDestroyedThenDeleteSequenceIsSpecified) {
    destructorId = 0u;
    struct GmmHelperMock : public GmmHelper {
        using GmmHelper::GmmHelper;
        ~GmmHelperMock() override {
            EXPECT_EQ(destructorId, 3u);
            destructorId++;
        }
    };
    struct MemoryMangerMock : public OsAgnosticMemoryManager {
        ~MemoryMangerMock() override {
            EXPECT_EQ(destructorId, 2u);
            destructorId++;
        }
    };

    struct CommandStreamReceiverMock : public MockCommandStreamReceiver {
        ~CommandStreamReceiverMock() override {
            EXPECT_EQ(destructorId, 1u);
            destructorId++;
        };
    };

    struct SourceLevelDebuggerMock : public SourceLevelDebugger {
        SourceLevelDebuggerMock() : SourceLevelDebugger(nullptr){};
        ~SourceLevelDebuggerMock() override {
            EXPECT_EQ(destructorId, 0u);
            destructorId++;
        }
    };

    struct MockExecutionEnvironment : ExecutionEnvironment {
        using ExecutionEnvironment::gmmHelper;
    };

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment(new MockExecutionEnvironment);
    executionEnvironment->gmmHelper.reset(new GmmHelperMock(platformDevices[0]));
    executionEnvironment->memoryManager.reset(new MemoryMangerMock);
    executionEnvironment->commandStreamReceiver.reset(new CommandStreamReceiverMock);
    executionEnvironment->sourceLevelDebugger.reset(new SourceLevelDebuggerMock);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(4u, destructorId);
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
