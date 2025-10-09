/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using XeHPAndLaterCommandEncoderMathTest = Test<DeviceFixture>;

HWTEST2_F(XeHPAndLaterCommandEncoderMathTest, WhenAppendsAGreaterThanThenPredicateCorrectlySetAndRemapEnabled, IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    EncodeMathMMIO<FamilyType>::encodeGreaterThanPredicate(cmdContainer, 0xDEADBEEFCAF0u, 17u, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_LOAD_REGISTER_MEM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdMEM = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    EXPECT_EQ(RegisterOffsets::csGprR0, cmdMEM->getRegisterAddress());
    EXPECT_EQ(0xDEADBEEFCAF0u, cmdMEM->getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_IMM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdIMM = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    EXPECT_EQ(RegisterOffsets::csGprR1, cmdIMM->getRegisterOffset());
    EXPECT_EQ(17u, cmdIMM->getDataDword());
    EXPECT_TRUE(cmdIMM->getMmioRemapEnable());

    itor = find<MI_MATH *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdMATH = genCmdCast<MI_MATH *>(*itor);
    EXPECT_EQ(cmdMATH->DW0.BitField.DwordLength, 3u);

    itor = find<MI_LOAD_REGISTER_REG *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdREG = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmdREG->getSourceRegisterAddress(), RegisterOffsets::csGprR2);
    EXPECT_EQ(cmdREG->getDestinationRegisterAddress(), RegisterOffsets::csPredicateResult);

    auto cmdALU = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(cmdMATH + 3);
    EXPECT_EQ(cmdALU->DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeSub));
}
