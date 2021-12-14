/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using AubHelperHwTestXeHPAndLater = Test<ClDeviceFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, AubHelperHwTestXeHPAndLater, givenAubHelperWhenGetDataHintForPml4EntryIsCalledThenTracePpgttLevel4IsReturned) {
    AubHelperHw<FamilyType> aubHelper(true);
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TracePpgttLevel4, aubHelper.getDataHintForPml4Entry());
}
HWCMDTEST_F(IGFX_XE_HP_CORE, AubHelperHwTestXeHPAndLater, givenAubHelperWhenGetDataHintForPml4EntryIsCalledThenTracePpgttLevel3IsReturned) {
    AubHelperHw<FamilyType> aubHelper(true);
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TracePpgttLevel3, aubHelper.getDataHintForPdpEntry());
}
HWCMDTEST_F(IGFX_XE_HP_CORE, AubHelperHwTestXeHPAndLater, givenAubHelperWhenGetDataHintForPml4EntryIsCalledThenTracePpgttLevel2IsReturned) {
    AubHelperHw<FamilyType> aubHelper(true);
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TracePpgttLevel2, aubHelper.getDataHintForPdEntry());
}
HWCMDTEST_F(IGFX_XE_HP_CORE, AubHelperHwTestXeHPAndLater, givenAubHelperWhenGetDataHintForPml4EntryIsCalledThenTracePpgttLevel1IsReturned) {
    AubHelperHw<FamilyType> aubHelper(true);
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TracePpgttLevel1, aubHelper.getDataHintForPtEntry());
}
