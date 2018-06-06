/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include <cstdint>
#include "runtime/scheduler/scheduler_kernel.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_ostime.h"
#include "runtime/helpers/options.h"
#include "gtest/gtest.h"
#include "test.h"
#include <memory>

using namespace OCLRT;
using namespace std;

class MockSchedulerKernel : public SchedulerKernel {
  public:
    MockSchedulerKernel(Program *program, const KernelInfo &info, const Device &device) : SchedulerKernel(program, info, device) {
    }

    ~MockSchedulerKernel() override {
        if (kernelInfoOwner)
            delete &kernelInfo;
    }

    static MockSchedulerKernel *create(Program &program, Device &device) {
        KernelInfo *info = new KernelInfo;
        SPatchDataParameterStream dataParametrStream;
        dataParametrStream.DataParameterStreamSize = 8;
        dataParametrStream.Size = 8;

        SPatchExecutionEnvironment executionEnvironment;
        executionEnvironment.CompiledSIMD8 = 1;
        executionEnvironment.HasDeviceEnqueue = 0;

        info->patchInfo.dataParameterStream = &dataParametrStream;
        info->patchInfo.executionEnvironment = &executionEnvironment;
        KernelArgInfo bufferArg;
        bufferArg.isBuffer = true;

        for (uint32_t i = 0; i < 9; i++) {
            bufferArg.kernelArgPatchInfoVector.resize(1);
            bufferArg.kernelArgPatchInfoVector[0].crossthreadOffset = 0;
            bufferArg.kernelArgPatchInfoVector[0].size = 0;
            bufferArg.kernelArgPatchInfoVector[0].sourceOffset = 0;
            info->kernelArgInfo.push_back(bufferArg);
        }

        MockSchedulerKernel *mock = Kernel::create<MockSchedulerKernel>(&program, *info, nullptr);
        mock->kernelInfoOwner = true;
        return mock;
    }

    bool kernelInfoOwner = false;
};

TEST(SchedulerKernelTest, getLws) {
    MockProgram program;
    MockDevice device(*platformDevices[0]);
    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, device);

    size_t lws = kernel.getLws();
    EXPECT_EQ((size_t)24u, lws);
}

TEST(SchedulerKernelTest, getGws) {
    MockProgram program;
    MockDevice device(*platformDevices[0]);
    device.setOSTime(new MockOSTime());
    device.initializeCaps();

    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, device);

    const size_t hwThreads = 3;
    const size_t simdSize = 8;

    size_t maxGws = platformDevices[0]->pSysInfo->EUCount * hwThreads * simdSize;

    size_t gws = kernel.getGws();
    EXPECT_GE(maxGws, gws);
    EXPECT_LT(hwThreads * simdSize, gws);
}

TEST(SchedulerKernelTest, setGws) {
    MockProgram program;
    MockDevice device(*platformDevices[0]);
    device.setOSTime(new MockOSTime());
    device.initializeCaps();

    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, device);

    kernel.setGws(24);

    size_t gws = kernel.getGws();

    EXPECT_EQ(24u, gws);
}

TEST(SchedulerKernelTest, getCurbeSize) {
    MockProgram program;
    MockDevice device(*platformDevices[0]);
    KernelInfo info;
    uint32_t crossTrheadDataSize = 32;
    uint32_t dshSize = 48;

    SPatchDataParameterStream dataParameterStream;
    dataParameterStream.DataParameterStreamSize = crossTrheadDataSize;

    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.DynamicStateHeapSize = dshSize;

    info.patchInfo.dataParameterStream = &dataParameterStream;
    info.heapInfo.pKernelHeader = &kernelHeader;

    MockSchedulerKernel kernel(&program, info, device);

    uint32_t expectedCurbeSize = alignUp(crossTrheadDataSize, 64) + alignUp(dshSize, 64) + alignUp(SCHEDULER_DYNAMIC_PAYLOAD_SIZE, 64);
    EXPECT_GE((size_t)expectedCurbeSize, kernel.getCurbeSize());
}

TEST(SchedulerKernelTest, setArgsForSchedulerKernel) {
    unique_ptr<MockDevice> device = unique_ptr<MockDevice>(DeviceHelper<>::create());
    MockProgram program;
    program.setDevice(device.get());

    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(program, *device.get()));
    unique_ptr<MockGraphicsAllocation> allocs[9];

    for (uint32_t i = 0; i < 9; i++) {
        allocs[i] = unique_ptr<MockGraphicsAllocation>(new MockGraphicsAllocation((void *)0x1234, 10));
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

TEST(SchedulerKernelTest, setArgsForSchedulerKernelWithNullDebugQueue) {
    unique_ptr<MockDevice> device = unique_ptr<MockDevice>(DeviceHelper<>::create());
    MockProgram program;
    program.setDevice(device.get());

    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(program, *device.get()));
    unique_ptr<MockGraphicsAllocation> allocs[9];

    for (uint32_t i = 0; i < 9; i++) {
        allocs[i] = unique_ptr<MockGraphicsAllocation>(new MockGraphicsAllocation((void *)0x1234, 10));
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

TEST(SchedulerKernelTest, createKernelReflectionForForcedSchedulerDispatch) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.ForceDispatchScheduler.set(true);
    unique_ptr<MockDevice> device = unique_ptr<MockDevice>(DeviceHelper<>::create());
    MockProgram program;
    program.setDevice(device.get());

    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(program, *device.get()));

    scheduler->createReflectionSurface();

    EXPECT_NE(nullptr, scheduler->getKernelReflectionSurface());
}

TEST(SchedulerKernelTest, createKernelReflectionSecondTimeForForcedSchedulerDispatchReturnsExistingAllocation) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.ForceDispatchScheduler.set(true);
    unique_ptr<MockDevice> device = unique_ptr<MockDevice>(DeviceHelper<>::create());
    MockProgram program;
    program.setDevice(device.get());

    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(program, *device.get()));

    scheduler->createReflectionSurface();

    auto *allocation = scheduler->getKernelReflectionSurface();
    scheduler->createReflectionSurface();
    auto *allocation2 = scheduler->getKernelReflectionSurface();

    EXPECT_EQ(allocation, allocation2);
}

TEST(SchedulerKernelTest, createKernelReflectionForSchedulerDoesNothing) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.ForceDispatchScheduler.set(false);
    unique_ptr<MockDevice> device = unique_ptr<MockDevice>(DeviceHelper<>::create());
    MockProgram program;
    program.setDevice(device.get());

    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(program, *device.get()));

    scheduler->createReflectionSurface();

    EXPECT_EQ(nullptr, scheduler->getKernelReflectionSurface());
}

TEST(SchedulerKernelTest, getCurbeSizeWithNullKernelInfo) {
    MockProgram program;
    MockDevice device(*platformDevices[0]);
    KernelInfo info;

    info.patchInfo.dataParameterStream = nullptr;
    info.heapInfo.pKernelHeader = nullptr;

    MockSchedulerKernel kernel(&program, info, device);

    uint32_t expectedCurbeSize = alignUp(SCHEDULER_DYNAMIC_PAYLOAD_SIZE, 64);
    EXPECT_GE((size_t)expectedCurbeSize, kernel.getCurbeSize());
}

TEST(SchedulerKernelTest, givenForcedSchedulerGwsByDebugVariableWhenSchedulerKernelIsCreatedThenGwsIsSetToForcedValue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SchedulerGWS.set(48);

    MockProgram program;
    MockDevice device(*platformDevices[0]);
    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, device);

    size_t gws = kernel.getGws();
    EXPECT_EQ(static_cast<size_t>(48u), gws);
}
