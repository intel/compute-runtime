/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/reg_configs.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"

namespace L0 {
namespace ult {

using CommandListAppendLaunchKernel = Test<ModuleFixture>;
using IsGen9Core = IsWithinProducts<IGFX_SKYLAKE, IGFX_COFFEELAKE>;

HWTEST2_F(CommandListAppendLaunchKernel, givenKernelWithSLMThenL3IsProgrammedWithSLMValue, IsGen9Core) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    createKernel();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_group_count_t groupCount{1, 1, 1};

    EXPECT_LE(0u, kernel->kernelImmData->getDescriptor().kernelAttributes.slmInlineSize);

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    bool foundL3 = false;
    for (auto it = cmdList.begin(); it != cmdList.end(); it++) {
        auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (lri) {
            if (lri->getRegisterOffset() == NEO::L3CNTLRegisterOffset<FamilyType>::registerOffset) {
                auto value = lri->getDataDword();
                auto dataSlm = NEO::PreambleHelper<FamilyType>::getL3Config(commandList->commandContainer.getDevice()->getHardwareInfo(), true);
                EXPECT_EQ(dataSlm, value);
                foundL3 = true;
                break;
            }
        }
    }

    EXPECT_TRUE(foundL3);
}

} // namespace ult
} // namespace L0
