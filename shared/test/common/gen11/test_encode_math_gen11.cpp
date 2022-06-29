/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using CommandEncoderMathTestGen11 = Test<DeviceFixture>;

GEN11TEST_F(CommandEncoderMathTestGen11, WhenAppendsAGreaterThanThenPredicateCorrectlySet) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);

    EncodeMathMMIO<FamilyType>::encodeGreaterThanPredicate(cmdContainer, 0xDEADBEEFCAF0u, 17u);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_LOAD_REGISTER_MEM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdMEM = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmdMEM->getRegisterAddress(), CS_GPR_R0);
    EXPECT_EQ(cmdMEM->getMemoryAddress(), 0xDEADBEEFCAF0u);

    itor = find<MI_LOAD_REGISTER_IMM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdIMM = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    EXPECT_EQ(cmdIMM->getRegisterOffset(), CS_GPR_R1);
    EXPECT_EQ(cmdIMM->getDataDword(), 17u);

    itor = find<MI_MATH *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdMATH = genCmdCast<MI_MATH *>(*itor);
    EXPECT_EQ(cmdMATH->DW0.BitField.DwordLength, 3u);

    itor = find<MI_LOAD_REGISTER_REG *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdREG = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmdREG->getSourceRegisterAddress(), CS_GPR_R2);
    EXPECT_EQ(cmdREG->getDestinationRegisterAddress(), CS_PREDICATE_RESULT);

    auto cmdALU = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(cmdMATH + 3);
    EXPECT_EQ(cmdALU->DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_SUB));
}
