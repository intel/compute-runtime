/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/scheduler/scheduler_kernel.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_ostime.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

using namespace NEO;
namespace NEO {
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchDataParameterStream &token);
}

class MockSchedulerKernel : public SchedulerKernel {
  public:
    MockSchedulerKernel(Program *program, const KernelInfoContainer &info) : SchedulerKernel(program, info) {
    }

    static MockSchedulerKernel *create(Program &program, KernelInfo *&info) {
        info = new KernelInfo;
        SPatchDataParameterStream dataParameterStream;
        dataParameterStream.DataParameterStreamSize = 8;
        dataParameterStream.Size = 8;
        populateKernelDescriptor(info->kernelDescriptor, dataParameterStream);

        info->kernelDescriptor.kernelAttributes.simdSize = 32;
        info->kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = false;

        KernelArgInfo bufferArg;
        bufferArg.isBuffer = true;

        for (uint32_t i = 0; i < 9; i++) {
            bufferArg.kernelArgPatchInfoVector.resize(1);
            bufferArg.kernelArgPatchInfoVector[0].crossthreadOffset = 0;
            bufferArg.kernelArgPatchInfoVector[0].size = 0;
            bufferArg.kernelArgPatchInfoVector[0].sourceOffset = 0;
            info->kernelArgInfo.push_back(std::move(bufferArg));
        }

        KernelInfoContainer kernelInfos;
        auto rootDeviceIndex = program.getDevices()[0]->getRootDeviceIndex();
        kernelInfos.resize(rootDeviceIndex + 1);
        kernelInfos[rootDeviceIndex] = info;

        MockSchedulerKernel *mock = Kernel::create<MockSchedulerKernel>(&program, kernelInfos, nullptr);
        return mock;
    }
};

TEST(SchedulerKernelTest, WhenSchedulerKernelIsCreatedThenLwsIs24) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    KernelInfo info;
    KernelInfoContainer kernelInfos;
    kernelInfos.push_back(&info);
    MockSchedulerKernel kernel(&program, kernelInfos);

    size_t lws = kernel.getLws();
    EXPECT_EQ((size_t)24u, lws);
}

TEST(SchedulerKernelTest, WhenSchedulerKernelIsCreatedThenGwsIs24) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    KernelInfo info;
    KernelInfoContainer kernelInfos;
    kernelInfos.push_back(&info);
    MockSchedulerKernel kernel(&program, kernelInfos);

    const size_t hwThreads = 3;
    const size_t simdSize = 8;

    size_t maxGws = defaultHwInfo->gtSystemInfo.EUCount * hwThreads * simdSize;

    size_t gws = kernel.getGws();
    EXPECT_GE(maxGws, gws);
    EXPECT_LT(hwThreads * simdSize, gws);
}

TEST(SchedulerKernelTest, WhenSettingGwsThenGetGwsReturnedSetValue) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    KernelInfo info;
    KernelInfoContainer kernelInfos;
    kernelInfos.push_back(&info);
    MockSchedulerKernel kernel(&program, kernelInfos);

    kernel.setGws(24);

    size_t gws = kernel.getGws();

    EXPECT_EQ(24u, gws);
}

TEST(SchedulerKernelTest, WhenSchedulerKernelIsCreatedThenCurbeSizeIsCorrect) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    KernelInfo info;
    uint32_t crossTrheadDataSize = 32;
    uint32_t dshSize = 48;

    SPatchDataParameterStream dataParameterStream;
    dataParameterStream.DataParameterStreamSize = crossTrheadDataSize;
    populateKernelDescriptor(info.kernelDescriptor, dataParameterStream);

    info.heapInfo.DynamicStateHeapSize = dshSize;

    KernelInfoContainer kernelInfos;
    kernelInfos.push_back(&info);
    MockSchedulerKernel kernel(&program, kernelInfos);

    uint32_t expectedCurbeSize = alignUp(crossTrheadDataSize, 64) + alignUp(dshSize, 64) + alignUp(SCHEDULER_DYNAMIC_PAYLOAD_SIZE, 64);
    EXPECT_GE((size_t)expectedCurbeSize, kernel.getCurbeSize());
}

TEST(SchedulerKernelTest, WhenSettingArgsForSchedulerKernelThenAllocationsAreCorrect) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));
    std::unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    std::unique_ptr<MockSchedulerKernel> scheduler = std::unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, infoPtr));
    info.reset(infoPtr);
    std::unique_ptr<MockGraphicsAllocation> allocs[9];

    for (uint32_t i = 0; i < 9; i++) {
        allocs[i] = std::unique_ptr<MockGraphicsAllocation>(new MockGraphicsAllocation((void *)0x1234, 10));
    }

    scheduler->setArgs(allocs[0].get(),
                       allocs[1].get(),
                       allocs[2].get(),
                       allocs[3].get(),
                       allocs[4].get(),
                       allocs[5].get(),
                       allocs[6].get(),
                       allocs[7].get(),
                       allocs[8].get());

    for (uint32_t i = 0; i < 9; i++) {
        EXPECT_EQ(allocs[i].get(), scheduler->getKernelArg(i));
    }
}

TEST(SchedulerKernelTest, GivenNullDebugQueueWhenSettingArgsForSchedulerKernelThenAllocationsAreCorrect) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));

    std::unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    std::unique_ptr<MockSchedulerKernel> scheduler = std::unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, infoPtr));
    info.reset(infoPtr);
    std::unique_ptr<MockGraphicsAllocation> allocs[9];

    for (uint32_t i = 0; i < 9; i++) {
        allocs[i] = std::unique_ptr<MockGraphicsAllocation>(new MockGraphicsAllocation((void *)0x1234, 10));
    }

    scheduler->setArgs(allocs[0].get(),
                       allocs[1].get(),
                       allocs[2].get(),
                       allocs[3].get(),
                       allocs[4].get(),
                       allocs[5].get(),
                       allocs[6].get(),
                       allocs[7].get());

    for (uint32_t i = 0; i < 8; i++) {
        EXPECT_EQ(allocs[i].get(), scheduler->getKernelArg(i));
    }
    EXPECT_EQ(nullptr, scheduler->getKernelArg(8));
}

TEST(SchedulerKernelTest, givenGraphicsAllocationWithDifferentCpuAndGpuAddressesWhenCallSetArgsThenGpuAddressesAreTaken) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));

    std::unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    auto scheduler = std::unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, infoPtr));
    info.reset(infoPtr);
    std::unique_ptr<MockGraphicsAllocation> allocs[9];

    for (uint32_t i = 0; i < 9; i++) {
        allocs[i] = std::make_unique<MockGraphicsAllocation>(reinterpret_cast<void *>(0x1234), 0x4321, 10);
    }

    scheduler->setArgs(allocs[0].get(),
                       allocs[1].get(),
                       allocs[2].get(),
                       allocs[3].get(),
                       allocs[4].get(),
                       allocs[5].get(),
                       allocs[6].get(),
                       allocs[7].get(),
                       allocs[8].get());

    for (uint32_t i = 0; i < 9; i++) {
        auto argAddr = reinterpret_cast<uint64_t>(scheduler->getKernelArgInfo(i).value);
        EXPECT_EQ(allocs[i]->getGpuAddress(), argAddr);
    }
}

TEST(SchedulerKernelTest, GivenForceDispatchSchedulerWhenCreatingKernelReflectionThenKernelReflectSurfaceIsNotNull) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.ForceDispatchScheduler.set(true);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));

    std::unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    std::unique_ptr<MockSchedulerKernel> scheduler = std::unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, infoPtr));
    info.reset(infoPtr);

    scheduler->createReflectionSurface();

    EXPECT_NE(nullptr, scheduler->getKernelReflectionSurface());
}

TEST(SchedulerKernelTest, GivenForceDispatchSchedulerWhenCreatingKernelReflectionTwiceThenTheSameAllocationIsUsed) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.ForceDispatchScheduler.set(true);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));

    std::unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    std::unique_ptr<MockSchedulerKernel> scheduler = std::unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, infoPtr));
    info.reset(infoPtr);

    scheduler->createReflectionSurface();

    auto *allocation = scheduler->getKernelReflectionSurface();
    scheduler->createReflectionSurface();
    auto *allocation2 = scheduler->getKernelReflectionSurface();

    EXPECT_EQ(allocation, allocation2);
}

TEST(SchedulerKernelTest, GivenNoForceDispatchSchedulerWhenCreatingKernelReflectionThenKernelReflectionSurfaceIsNotCreated) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.ForceDispatchScheduler.set(false);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));

    std::unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    std::unique_ptr<MockSchedulerKernel> scheduler = std::unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, infoPtr));
    info.reset(infoPtr);

    scheduler->createReflectionSurface();

    EXPECT_EQ(nullptr, scheduler->getKernelReflectionSurface());
}

TEST(SchedulerKernelTest, GivenNullKernelInfoWhenGettingCurbeSizeThenSizeIsCorrect) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    KernelInfo info;

    KernelInfoContainer kernelInfos;
    kernelInfos.push_back(&info);
    MockSchedulerKernel kernel(&program, kernelInfos);

    uint32_t expectedCurbeSize = alignUp(SCHEDULER_DYNAMIC_PAYLOAD_SIZE, 64);
    EXPECT_GE((size_t)expectedCurbeSize, kernel.getCurbeSize());
}

TEST(SchedulerKernelTest, givenForcedSchedulerGwsByDebugVariableWhenSchedulerKernelIsCreatedThenGwsIsSetToForcedValue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SchedulerGWS.set(48);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    KernelInfo info;
    KernelInfoContainer kernelInfos;
    kernelInfos.push_back(&info);
    MockSchedulerKernel kernel(&program, kernelInfos);

    size_t gws = kernel.getGws();
    EXPECT_EQ(static_cast<size_t>(48u), gws);
}

TEST(SchedulerKernelTest, givenSimulationModeWhenSchedulerKernelIsCreatedThenGwsIsSetToOneWorkgroup) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrSimulationMode = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    MockProgram program(toClDeviceVector(*device));

    KernelInfo info;
    KernelInfoContainer kernelInfos;
    kernelInfos.push_back(&info);
    MockSchedulerKernel kernel(&program, kernelInfos);
    size_t gws = kernel.getGws();
    EXPECT_EQ(static_cast<size_t>(24u), gws);
}

TEST(SchedulerKernelTest, givenForcedSchedulerGwsByDebugVariableAndSimulationModeWhenSchedulerKernelIsCreatedThenGwsIsSetToForcedValue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SchedulerGWS.set(48);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrSimulationMode = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    MockProgram program(toClDeviceVector(*device));

    KernelInfo info;
    KernelInfoContainer kernelInfos;
    kernelInfos.push_back(&info);
    MockSchedulerKernel kernel(&program, kernelInfos);
    size_t gws = kernel.getGws();
    EXPECT_EQ(static_cast<size_t>(48u), gws);
}
