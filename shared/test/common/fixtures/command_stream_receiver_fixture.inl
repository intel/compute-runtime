/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/common/fixtures/command_stream_receiver_fixture.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"

#include "gtest/gtest.h"

template <typename FamilyType>
void CommandStreamReceiverSystolicFixture::testBody() {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    StreamProperties &streamProperties = commandStreamReceiver.getStreamProperties();

    bool systolicModeSupported = commandStreamReceiver.pipelineSupportFlags.systolicMode;

    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastMediaSamplerConfig = false;

    flushTaskFlags.pipelineSelectArgs.systolicPipelineSelectMode = true;
    flushTaskFlags.pipelineSelectArgs.mediaSamplerRequired = false;

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);
    if (systolicModeSupported) {
        EXPECT_EQ(true, commandStreamReceiver.lastSystolicPipelineSelectMode);
        EXPECT_EQ(1, streamProperties.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(false, commandStreamReceiver.lastSystolicPipelineSelectMode);
        EXPECT_EQ(-1, streamProperties.pipelineSelect.systolicMode.value);
    }

    flushTaskFlags.pipelineSelectArgs.systolicPipelineSelectMode = false;

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);
    EXPECT_EQ(false, commandStreamReceiver.lastSystolicPipelineSelectMode);

    if (systolicModeSupported) {
        EXPECT_EQ(0, streamProperties.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(-1, streamProperties.pipelineSelect.systolicMode.value);
    }
}
