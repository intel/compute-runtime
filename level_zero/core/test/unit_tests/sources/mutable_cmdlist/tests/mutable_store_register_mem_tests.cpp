/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_store_register_mem_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

namespace L0 {

namespace ult {

using MutableStoreRegisterMemTest = Test<MutableHwCommandFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableStoreRegisterMemTest,
            givenMutableStoreRegisterMemCommandWhenCommandIsMutatedThenAddressIsUpdated) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    uint64_t address = 0x481B000;
    uint32_t registerAddress = 0x2600;
    size_t offset = 0x40;

    // initialize command and mutable object
    auto storeRegMemCmd = reinterpret_cast<MI_STORE_REGISTER_MEM *>(this->cmdBufferGpuPtr);
    NEO::EncodeStoreMMIO<FamilyType>::encode(storeRegMemCmd, registerAddress, address + offset, false, false);
    EXPECT_EQ(address + offset, storeRegMemCmd->getMemoryAddress());

    L0::MCL::MutableStoreRegisterMemHw<FamilyType> mutableStoreRegisterMem(this->cmdBufferGpuPtr, offset);

    address = 0x78AB000;
    mutableStoreRegisterMem.setMemoryAddress(address);

    EXPECT_EQ(address + offset, storeRegMemCmd->getMemoryAddress());
}

} // namespace ult
} // namespace L0
