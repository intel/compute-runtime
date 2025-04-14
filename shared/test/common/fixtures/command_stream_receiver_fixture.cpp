/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/command_stream_receiver_fixture.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

using namespace NEO;

void CommandStreamReceiverFixture::setUp() {
    DeviceFixture::setUp();

    commandStream.replaceBuffer(cmdBuffer, bufferSize);
    auto graphicsAllocation = new MockGraphicsAllocation(cmdBuffer, cmdBufferGpuAddress, bufferSize);
    commandStream.replaceGraphicsAllocation(graphicsAllocation);

    dsh.replaceBuffer(dshBuffer, bufferSize);
    graphicsAllocation = new MockGraphicsAllocation(dshBuffer, bufferSize);
    dsh.replaceGraphicsAllocation(graphicsAllocation);

    ioh.replaceBuffer(iohBuffer, bufferSize);
    graphicsAllocation = new MockGraphicsAllocation(iohBuffer, bufferSize);
    ioh.replaceGraphicsAllocation(graphicsAllocation);

    ssh.replaceBuffer(sshBuffer, bufferSize);
    graphicsAllocation = new MockGraphicsAllocation(sshBuffer, bufferSize);
    ssh.replaceGraphicsAllocation(graphicsAllocation);

    flushTaskFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    requiredStreamProperties.initSupport(pDevice->getRootDeviceEnvironment());
    immediateFlushTaskFlags.requiredState = &requiredStreamProperties;
    immediateFlushTaskFlags.sshCpuBase = sshBuffer;
    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::kernel;

    if (pDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread) {
        auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
        if (!commandStreamReceiver.getPreemptionAllocation()) {
            ASSERT_TRUE(commandStreamReceiver.createPreemptionAllocation());
        }
    }
}

void CommandStreamReceiverFixture::tearDown() {
    DeviceFixture::tearDown();

    delete dsh.getGraphicsAllocation();
    delete ioh.getGraphicsAllocation();
    delete ssh.getGraphicsAllocation();
    delete commandStream.getGraphicsAllocation();
}
