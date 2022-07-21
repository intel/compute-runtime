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
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

using namespace NEO;

using CommandEncodeEnableRayTracing = Test<CommandEncodeStatesFixture>;

HWTEST_F(CommandEncodeEnableRayTracing, programEnableRayTracing) {
    uint32_t pCmdBuffer[1024];
    uint32_t pMemoryBackedBuffer[1024];

    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    uint64_t memoryBackedBuffer = reinterpret_cast<uint64_t>(&pMemoryBackedBuffer);

    EncodeEnableRayTracing<FamilyType>::programEnableRayTracing(stream, memoryBackedBuffer);
}
