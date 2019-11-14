/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue_hw.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/mocks/mock_memory_manager.h"

namespace NEO {

struct CommandDeviceFixture : public DeviceFixture,
                              public CommandQueueHwFixture {
    using CommandQueueHwFixture::SetUp;
    void SetUp(cl_command_queue_properties cmdQueueProperties = 0) {
        DeviceFixture::SetUp();
        CommandQueueHwFixture::SetUp(pDevice, cmdQueueProperties);
    }

    void TearDown() {
        CommandQueueHwFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

struct CommandEnqueueBaseFixture : CommandDeviceFixture,
                                   public IndirectHeapFixture,
                                   public HardwareParse {
    using IndirectHeapFixture::SetUp;
    void SetUp(cl_command_queue_properties cmdQueueProperties = 0) {
        CommandDeviceFixture::SetUp(cmdQueueProperties);
        IndirectHeapFixture::SetUp(pCmdQ);
        HardwareParse::SetUp();
    }

    void TearDown() {
        HardwareParse::TearDown();
        IndirectHeapFixture::TearDown();
        CommandDeviceFixture::TearDown();
    }
};

struct CommandEnqueueFixture : public CommandEnqueueBaseFixture,
                               public CommandStreamFixture {
    void SetUp(cl_command_queue_properties cmdQueueProperties = 0) {
        CommandEnqueueBaseFixture::SetUp(cmdQueueProperties);
        CommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() {
        CommandEnqueueBaseFixture::TearDown();
        CommandStreamFixture::TearDown();
    }
};

struct NegativeFailAllocationCommandEnqueueBaseFixture : public CommandEnqueueBaseFixture {
    void SetUp() override {
        CommandEnqueueBaseFixture::SetUp();
        failMemManager.reset(new FailMemoryManager(*pDevice->getExecutionEnvironment()));

        BufferDefaults::context = context;
        Image2dDefaults::context = context;
        buffer.reset(BufferHelper<>::create());
        image.reset(ImageHelper<Image2dDefaults>::create());
        ptr = static_cast<void *>(array);
        oldMemManager = pDevice->getExecutionEnvironment()->memoryManager.release();
        pDevice->injectMemoryManager(failMemManager.release());
    }

    void TearDown() override {
        pDevice->injectMemoryManager(oldMemManager);
        buffer.reset(nullptr);
        image.reset(nullptr);
        BufferDefaults::context = nullptr;
        Image2dDefaults::context = nullptr;
        CommandEnqueueBaseFixture::TearDown();
    }

    std::unique_ptr<Buffer> buffer;
    std::unique_ptr<Image> image;
    std::unique_ptr<FailMemoryManager> failMemManager;
    char array[MemoryConstants::cacheLineSize];
    void *ptr;
    MemoryManager *oldMemManager;
};

template <typename FamilyType>
struct CommandQueueStateless : public CommandQueueHw<FamilyType> {
    CommandQueueStateless(Context *context, Device *device) : CommandQueueHw<FamilyType>(context, device, nullptr){};

    void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo) override {
        auto kernel = dispatchInfo.begin()->getKernel();
        EXPECT_TRUE(kernel->getKernelInfo().patchInfo.executionEnvironment->CompiledForGreaterThan4GBBuffers);
        EXPECT_FALSE(kernel->getKernelInfo().kernelArgInfo[0].pureStatefulBufferAccess);
    }
};

template <typename FamilyType>
struct CommandQueueStateful : public CommandQueueHw<FamilyType> {
    CommandQueueStateful(Context *context, Device *device) : CommandQueueHw<FamilyType>(context, device, nullptr){};

    void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo) override {
        auto kernel = dispatchInfo.begin()->getKernel();
        auto &device = kernel->getDevice();
        if (!device.areSharedSystemAllocationsAllowed()) {
            EXPECT_FALSE(kernel->getKernelInfo().patchInfo.executionEnvironment->CompiledForGreaterThan4GBBuffers);
            if (device.getHardwareCapabilities().isStatelesToStatefullWithOffsetSupported) {
                EXPECT_TRUE(kernel->allBufferArgsStateful);
            }
        } else {
            EXPECT_TRUE(kernel->getKernelInfo().patchInfo.executionEnvironment->CompiledForGreaterThan4GBBuffers);
            EXPECT_FALSE(kernel->getKernelInfo().kernelArgInfo[0].pureStatefulBufferAccess);
        }
    }
};

} // namespace NEO
