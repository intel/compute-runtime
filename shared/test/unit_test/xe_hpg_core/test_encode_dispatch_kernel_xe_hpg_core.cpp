/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/xe_hpg_core/hw_cmds_base.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using CommandEncodeStatesTestXeHpgCore = ::testing::Test;

HWTEST2_F(CommandEncodeStatesTestXeHpgCore, givenVariousValuesWhenCallingSetBarrierEnableThenCorrectValuesAreSet, IsXeHpgCore) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;

    uint32_t barrierCounts[] = {0, 1};

    for (auto barrierCount : barrierCounts) {
        EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, barrierCount, *defaultHwInfo);

        EXPECT_EQ(barrierCount, idd.getNumberOfBarriers());
    }
}
