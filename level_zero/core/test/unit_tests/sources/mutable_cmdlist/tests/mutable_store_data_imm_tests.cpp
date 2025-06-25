/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_store_data_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

namespace L0 {

namespace ult {

using MutableStoreDataImmTest = Test<MutableHwCommandFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableStoreDataImmTest,
            givenMutableStoreDataCommandWhenCommandIsNoopedThenBufferSpaceIsZeroed) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint64_t address = 0xAF5000;
    uint32_t value = Event::STATE_SIGNALED;
    size_t offset = 0x10;

    // prepare noop buffer for comparison
    uint8_t noopSpace[sizeof(MI_STORE_DATA_IMM)];
    memset(noopSpace, 0, sizeof(MI_STORE_DATA_IMM));

    // initialize command and mutable object
    NEO::EncodeStoreMemory<FamilyType>::programStoreDataImm(reinterpret_cast<MI_STORE_DATA_IMM *>(this->cmdBufferGpuPtr), address + offset, value, 0, false, false);
    L0::MCL::MutableStoreDataImmHw<FamilyType> mutableStoreDataImm(this->cmdBufferGpuPtr, offset, false);

    mutableStoreDataImm.noop();

    EXPECT_EQ(0, memcmp(noopSpace, this->cmdBufferGpuPtr, sizeof(MI_STORE_DATA_IMM)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableStoreDataImmTest,
            givenMutableStoreDataCommandWhenCommandIsRestoredThenCommandIsProgrammed) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint64_t address = 0xAF5000;
    uint32_t value = Event::STATE_SIGNALED;
    size_t offset = 0x20;

    MI_STORE_DATA_IMM cmdSdi;

    // prepare buffer for comparison
    NEO::EncodeStoreMemory<FamilyType>::programStoreDataImm(&cmdSdi, address + offset, value, 0, false, false);

    // noop command buffer and create mutable object
    memset(this->cmdBufferGpuPtr, 0, sizeof(MI_STORE_DATA_IMM));
    L0::MCL::MutableStoreDataImmHw<FamilyType> mutableStoreDataImm(this->cmdBufferGpuPtr, offset, false);

    mutableStoreDataImm.restoreWithAddress(address);

    EXPECT_EQ(0, memcmp(&cmdSdi, this->cmdBufferGpuPtr, sizeof(MI_STORE_DATA_IMM)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableStoreDataImmTest,
            givenMutableStoreDataCommandWhenCommandIsSetThenCommandValueIsChanged) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint64_t address = 0xAF5000;
    uint32_t value = Event::STATE_SIGNALED;
    size_t offset = 0x20;

    auto sdiCommand = reinterpret_cast<MI_STORE_DATA_IMM *>(this->cmdBufferGpuPtr);

    // prepare buffer for comparison
    NEO::EncodeStoreMemory<FamilyType>::programStoreDataImm(sdiCommand, address + offset, value, 0, false, false);
    EXPECT_EQ(address + offset, sdiCommand->getAddress());

    L0::MCL::MutableStoreDataImmHw<FamilyType> mutableStoreDataImm(this->cmdBufferGpuPtr, offset, false);

    address = 0x101B40000;
    mutableStoreDataImm.setAddress(address);
    EXPECT_EQ(address + offset, sdiCommand->getAddress());
}

} // namespace ult
} // namespace L0
