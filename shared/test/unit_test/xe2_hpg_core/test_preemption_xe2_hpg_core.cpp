/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "per_product_test_definitions.h"

using namespace NEO;

template <>
PreemptionTestHwDetails getPreemptionTestHwDetails<Xe2HpgCoreFamily>() {
    PreemptionTestHwDetails ret;
    return ret;
}

using Xe2HpgPreemptionTests = DevicePreemptionTests;

XE2_HPG_CORETEST_F(Xe2HpgPreemptionTests, givenXe2HpgInterfaceDescriptorDataWhenPreemptionModeIsMidThreadThenThreadPreemptionBitIsEnabled) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::Disabled);
    EXPECT_FALSE(iddArg.getThreadPreemption());

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidBatch);
    EXPECT_FALSE(iddArg.getThreadPreemption());

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::ThreadGroup);
    EXPECT_FALSE(iddArg.getThreadPreemption());

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    EXPECT_TRUE(iddArg.getThreadPreemption());
}
