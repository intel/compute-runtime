/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeEnableRayTracing = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeEnableRayTracing, whenEnableRayTracingIsProgrammedThen3DStateBtdIsEncodedInStream, IsAtLeastXeHpcCore) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;

    uint32_t pCmdBuffer[1024];
    uint32_t pMemoryBackedBuffer[1024];

    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    EncodeEnableRayTracing<FamilyType>::programEnableRayTracing(stream, reinterpret_cast<uint64_t>(&pMemoryBackedBuffer));

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto itor = commands.begin();
    itor = find<_3DSTATE_BTD *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
}
