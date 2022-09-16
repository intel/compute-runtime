/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/command_stream_receiver_fixture.h"

#include "shared/source/command_stream/preemption.h"

using namespace NEO;

void CommandStreamReceiverFixture::setUp() {
    DeviceFixture::setUp();

    commandStream.replaceBuffer(cmdBuffer, bufferSize);
    auto graphicsAllocation = new MockGraphicsAllocation(cmdBuffer, bufferSize);
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
}

void CommandStreamReceiverFixture::tearDown() {
    DeviceFixture::tearDown();

    delete dsh.getGraphicsAllocation();
    delete ioh.getGraphicsAllocation();
    delete ssh.getGraphicsAllocation();
    delete commandStream.getGraphicsAllocation();
}
