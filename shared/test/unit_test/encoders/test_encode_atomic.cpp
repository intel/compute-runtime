/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeAtomic = Test<CommandEncodeStatesFixture>;

HWTEST_F(CommandEncodeAtomic, WhenProgrammingMiAtomicThenExpectAllFieldsSetCorrectly) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;

    constexpr size_t bufferSize = 128u;
    uint8_t buffer[bufferSize];

    LinearStream cmdbuffer(buffer, bufferSize);

    EncodeAtomic<FamilyType>::programMiAtomic(cmdbuffer,
                                              static_cast<uint64_t>(0x123400),
                                              ATOMIC_OPCODES::ATOMIC_4B_DECREMENT,
                                              DATA_SIZE::DATA_SIZE_DWORD,
                                              0x1u,
                                              0x1u);

    MI_ATOMIC *miAtomicCmd = reinterpret_cast<MI_ATOMIC *>(cmdbuffer.getCpuBase());

    EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_4B_DECREMENT, miAtomicCmd->getAtomicOpcode());
    EXPECT_EQ(DATA_SIZE::DATA_SIZE_DWORD, miAtomicCmd->getDataSize());
    EXPECT_EQ(0x123400u, miAtomicCmd->getMemoryAddress());
    EXPECT_EQ(0x1u, miAtomicCmd->getReturnDataControl());
    EXPECT_EQ(0x1u, miAtomicCmd->getCsStall());
}
