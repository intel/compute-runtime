/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using XeHPAndLaterEncodeMiFlushDWTest = Test<CommandEncodeStatesFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterEncodeMiFlushDWTest, whenMiFlushDwIsProgrammedThenSetFlushCcsAndLlc) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    MiFlushArgs args;
    args.commandWithPostSync = true;
    EncodeMiFlushDW<FamilyType>::programMiFlushDw(linearStream, 0x1230000, 456, args, *defaultHwInfo);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);
    EXPECT_EQ(0u, miFlushDwCmd->getFlushCcs());
    EXPECT_EQ(0u, miFlushDwCmd->getFlushLlc());

    miFlushDwCmd++;
    EXPECT_EQ(1u, miFlushDwCmd->getFlushCcs());
    EXPECT_EQ(1u, miFlushDwCmd->getFlushLlc());
}
