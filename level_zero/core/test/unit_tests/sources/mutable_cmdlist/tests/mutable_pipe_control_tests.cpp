/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

namespace L0 {

namespace ult {

using MutablePipeControlTest = Test<MutableHwCommandFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutablePipeControlTest,
            givenMutablePipeControlCommandWhenCommandIsMutatedThenAddressIsUpdated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    uint64_t address = 0x261B000;
    uint64_t immediateData = 2;

    // initialize command and mutable object
    NEO::PipeControlArgs args;
    NEO::MemorySynchronizationCommands<FamilyType>::setSingleBarrier(this->cmdBufferGpuPtr,
                                                                     NEO::PostSyncMode::immediateData,
                                                                     address,
                                                                     immediateData,
                                                                     args);

    L0::MCL::MutablePipeControlHw<FamilyType> mutablePipeControl(this->cmdBufferGpuPtr);

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(this->cmdBufferGpuPtr);
    EXPECT_EQ(address, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));

    address = 0x482B000;
    mutablePipeControl.setPostSyncAddress(address);

    EXPECT_EQ(address, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
}

} // namespace ult
} // namespace L0
