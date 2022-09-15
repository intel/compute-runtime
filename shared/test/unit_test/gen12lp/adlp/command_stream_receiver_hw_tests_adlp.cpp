/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gen12lp/hw_cmds_adlp.h"
#include "shared/test/common/fixtures/command_stream_receiver_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen12LpCommandStreamReceiverHwTests = Test<CommandStreamReceiverFixture>;

ADLPTEST_F(Gen12LpCommandStreamReceiverHwTests, givenSystolicModeChangedWhenFlushTaskCalledThenSystolicStateIsUpdated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    StreamProperties &streamProperties = commandStreamReceiver.getStreamProperties();

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
    EXPECT_EQ(true, commandStreamReceiver.lastSystolicPipelineSelectMode);
    EXPECT_EQ(1, streamProperties.pipelineSelect.systolicMode.value);

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
    EXPECT_EQ(0, streamProperties.pipelineSelect.systolicMode.value);
}
