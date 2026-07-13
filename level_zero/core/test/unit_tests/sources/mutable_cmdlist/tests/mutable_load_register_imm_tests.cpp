/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

namespace L0 {

namespace ult {

using MutableLoadRegisterImmTest = Test<MutableHwCommandFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableLoadRegisterImmTest,
            givenMutableLoadRegisterCommandWhenCommandIsNoopedThenBufferSpaceIsZeroed) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    uint32_t registerAddress = 0x2600;
    uint32_t value = 0x8;

    alignas(uint32_t) uint8_t noopSpace[sizeof(MI_LOAD_REGISTER_IMM)];

    // prepare noop buffer for comparison
    memset(noopSpace, 0, sizeof(MI_LOAD_REGISTER_IMM));

    // initialize command and mutable object
    NEO::LriHelper<FamilyType>::program(&this->commandStream, registerAddress, value, true, false);
    L0::MCL::MutableLoadRegisterImmHw<FamilyType> mutableLoadRegisterImm(0, nullptr, this->cmdBufferGpuPtr, registerAddress);

    mutableLoadRegisterImm.noop();

    EXPECT_EQ(0, memcmp(noopSpace, this->cmdBufferGpuPtr, sizeof(MI_LOAD_REGISTER_IMM)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableLoadRegisterImmTest,
            givenMutableLoadRegisterCommandWhenCommandIsRestoredThenCommandIsProgrammed) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    uint32_t registerAddress = 0x2600;
    uint32_t value = 0x0;

    MI_LOAD_REGISTER_IMM cmdLri;

    // prepare buffer for comparison
    NEO::LriHelper<FamilyType>::program(&cmdLri, registerAddress, value, true, false);

    // noop command buffer and create mutable object
    memset(this->cmdBufferGpuPtr, 0, sizeof(MI_LOAD_REGISTER_IMM));
    L0::MCL::MutableLoadRegisterImmHw<FamilyType> mutableLoadRegisterImm(0, nullptr, this->cmdBufferGpuPtr, registerAddress);

    mutableLoadRegisterImm.restore();

    EXPECT_EQ(0, memcmp(&cmdLri, this->cmdBufferGpuPtr, sizeof(MI_LOAD_REGISTER_IMM)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableLoadRegisterImmTest,
            givenMutableLoadRegisterCommandWhenCommandIsSetThenCommandValueIsChanged) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    uint32_t registerAddress = 0x2600;
    uint32_t value = 0x0;

    // prepare buffer for comparison
    NEO::LriHelper<FamilyType>::program(reinterpret_cast<MI_LOAD_REGISTER_IMM *>(this->cmdBufferGpuPtr), registerAddress, value, true, false);

    auto lriCommand = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(this->cmdBufferGpuPtr);
    EXPECT_EQ(value, lriCommand->getDataDword());

    L0::MCL::MutableLoadRegisterImmHw<FamilyType> mutableLoadRegisterImm(0, nullptr, this->cmdBufferGpuPtr, registerAddress);

    value = 0x8;
    mutableLoadRegisterImm.setValue(value);
    EXPECT_EQ(value, lriCommand->getDataDword());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableLoadRegisterImmTest,
            givenMutableLoadRegisterCommandWhenUsingCommandViewAndCommandIsNoopedThenCommandViewSpaceIsZeroed) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    uint32_t registerAddress = 0x2600;
    uint32_t value = 0x8;

    void *cmdView = NEO::EncodeSetMMIO<FamilyType>::allocateLoadRegisterImmCommand();

    alignas(uint32_t) uint8_t noopSpace[sizeof(MI_LOAD_REGISTER_IMM)];
    // prepare noop buffer for comparison
    memset(noopSpace, 0, sizeof(MI_LOAD_REGISTER_IMM));

    // initialize command and mutable object
    NEO::LriHelper<FamilyType>::program(reinterpret_cast<MI_LOAD_REGISTER_IMM *>(cmdView), registerAddress, value, true, false);
    L0::MCL::MutableLoadRegisterImmHw<FamilyType> mutableLoadRegisterImm(0x1000, cmdView, this->cmdBufferGpuPtr, registerAddress);

    EXPECT_EQ(cmdView, mutableLoadRegisterImm.getCommandView());
    EXPECT_EQ(0x1000u, mutableLoadRegisterImm.getGpuDestinationAddress());
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), mutableLoadRegisterImm.getCommandSize());

    mutableLoadRegisterImm.noop();

    EXPECT_EQ(0, memcmp(noopSpace, cmdView, sizeof(MI_LOAD_REGISTER_IMM)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableLoadRegisterImmTest,
            givenMutableLoadRegisterCommandWhenUsingCommandViewAndCommandIsRestoredThenCommandIsProgrammed) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    uint32_t registerAddress = 0x2600;
    uint32_t value = 0x0;

    void *cmdView = NEO::EncodeSetMMIO<FamilyType>::allocateLoadRegisterImmCommand();

    // prepare buffer for comparison
    MI_LOAD_REGISTER_IMM cmdLri;
    NEO::LriHelper<FamilyType>::program(&cmdLri, registerAddress, value, true, false);

    // noop command buffer and create mutable object
    memset(cmdView, 0, sizeof(MI_LOAD_REGISTER_IMM));
    L0::MCL::MutableLoadRegisterImmHw<FamilyType> mutableLoadRegisterImm(0x1000, cmdView, this->cmdBufferGpuPtr, registerAddress);

    mutableLoadRegisterImm.restore();

    EXPECT_EQ(0, memcmp(&cmdLri, cmdView, sizeof(MI_LOAD_REGISTER_IMM)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableLoadRegisterImmTest,
            givenMutableLoadRegisterCommandWhenUsingCommandViewAndCommandIsSetThenCommandValueIsChanged) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    uint32_t registerAddress = 0x2600;
    uint32_t value = 0x0;

    void *cmdView = NEO::EncodeSetMMIO<FamilyType>::allocateLoadRegisterImmCommand();

    // prepare buffer for comparison
    NEO::LriHelper<FamilyType>::program(reinterpret_cast<MI_LOAD_REGISTER_IMM *>(cmdView), registerAddress, value, true, false);

    auto lriCommand = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(cmdView);
    EXPECT_EQ(value, lriCommand->getDataDword());

    L0::MCL::MutableLoadRegisterImmHw<FamilyType> mutableLoadRegisterImm(0, cmdView, this->cmdBufferGpuPtr, registerAddress);

    value = 0x8;
    mutableLoadRegisterImm.setValue(value);
    EXPECT_EQ(value, lriCommand->getDataDword());
}

} // namespace ult
} // namespace L0
