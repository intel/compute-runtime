/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/device_queue/device_queue.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

struct ParentKernelCommandQueueFixture : public CommandQueueHwFixture,
                                         testing::Test {

    void SetUp() override {
        device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, rootDeviceIndex)};
        CommandQueueHwFixture::SetUp(device, 0);
    }
    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        delete device;
    }

    std::unique_ptr<KernelOperation> createBlockedCommandsData(CommandQueue &commandQueue) {
        auto commandStream = new LinearStream();

        auto &gpgpuCsr = commandQueue.getGpgpuCommandStreamReceiver();
        gpgpuCsr.ensureCommandBufferAllocation(*commandStream, 1, 1);

        return std::make_unique<KernelOperation>(commandStream, *gpgpuCsr.getInternalAllocationStorage());
    }
    const uint32_t rootDeviceIndex = 0u;
};
