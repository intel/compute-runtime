/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen8CoherencyRequirements = ::testing::Test;

GEN8TEST_F(Gen8CoherencyRequirements, WhenMemoryManagerIsInitializedThenNoCoherencyProgramming) {
    UltDeviceFactory deviceFactory{1, 0};
    LinearStream stream;
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    auto &csr = deviceFactory.rootDevices[0]->getUltCommandStreamReceiver<FamilyType>();

    auto retSize = csr.getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);
    csr.programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(0u, stream.getUsed());

    flags.requiresCoherency = true;
    retSize = csr.getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);
    csr.programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(0u, stream.getUsed());
}
