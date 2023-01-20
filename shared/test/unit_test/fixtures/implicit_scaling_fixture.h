/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

#include <memory>

using namespace NEO;

struct ImplicitScalingFixture : public CommandEncodeStatesFixture {
    void setUp();
    void tearDown();

    static constexpr uint64_t gpuVa = (1ull << 48);
    static constexpr size_t bufferSize = 1024u;
    DebugManagerStateRestore restorer;
    LinearStream commandStream;
    MockGraphicsAllocation cmdBufferAlloc;
    HardwareInfo testHardwareInfo = {};
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
    DeviceBitfield singleTile;
    DeviceBitfield twoTile;
    void *alignedMemory = nullptr;
    bool dcFlushFlag = false;
    bool forceExecutionOnSingleTileFlag = false;
};

using ImplicitScalingTests = Test<ImplicitScalingFixture>;
