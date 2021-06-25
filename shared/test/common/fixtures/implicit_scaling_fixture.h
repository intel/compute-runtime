/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "test.h"

#include "hw_cmds.h"

#include <memory>

using namespace NEO;

struct ImplicitScalingFixture : public CommandEncodeStatesFixture {
    void SetUp() {
        CommandEncodeStatesFixture::SetUp();
        apiSupportBackup = std::make_unique<VariableBackup<bool>>(&ImplicitScaling::apiSupport, true);
        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&OSInterface::osEnableLocalMemory, true);

        singleTile = DeviceBitfield(static_cast<uint32_t>(maxNBitValue(1)));
        twoTile = DeviceBitfield(static_cast<uint32_t>(maxNBitValue(2)));

        alignedMemory = alignedMalloc(bufferSize, 4096);

        cmdBufferAlloc.setCpuPtrAndGpuAddress(alignedMemory, gpuVa);

        commandStream.replaceBuffer(alignedMemory, bufferSize);
        commandStream.replaceGraphicsAllocation(&cmdBufferAlloc);
    }

    void TearDown() {
        alignedFree(alignedMemory);
        CommandEncodeStatesFixture::TearDown();
    }

    static constexpr uint64_t gpuVa = (1ull << 48);
    static constexpr size_t bufferSize = 1024u;
    DebugManagerStateRestore restorer;
    LinearStream commandStream;
    MockGraphicsAllocation cmdBufferAlloc;
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
    DeviceBitfield singleTile;
    DeviceBitfield twoTile;
    void *alignedMemory = nullptr;
};

using ImplicitScalingTests = Test<ImplicitScalingFixture>;
