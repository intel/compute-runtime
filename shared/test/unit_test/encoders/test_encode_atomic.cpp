/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"

using namespace NEO;

using CommandEncodeAtomic = Test<CommandEncodeStatesFixture>;

HWTEST_F(CommandEncodeAtomic, programMiAtomic) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    MI_ATOMIC miAtomic;

    EncodeAtomic<FamilyType>::programMiAtomic(&miAtomic, 0x123400, MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT,
                                              MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD);

    EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT, miAtomic.getAtomicOpcode());
    EXPECT_EQ(MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD, miAtomic.getDataSize());
    EXPECT_EQ(0x123400u, miAtomic.getMemoryAddress());
}
