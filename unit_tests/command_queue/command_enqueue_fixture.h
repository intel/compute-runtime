/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/helpers/hw_parse.h"

namespace OCLRT {

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
        failMemManager.reset(new FailMemoryManager());

        BufferDefaults::context = context;
        Image2dDefaults::context = context;
        buffer.reset(BufferHelper<>::create());
        image.reset(ImageHelper<Image2dDefaults>::create());
        ptr = static_cast<void *>(array);
        oldMemManager = pDevice->getMemoryManager();
        pDevice->getCommandStreamReceiver().setMemoryManager(failMemManager.get());
    }

    void TearDown() override {
        pDevice->getCommandStreamReceiver().setMemoryManager(oldMemManager);
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

} // namespace OCLRT
