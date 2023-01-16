/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;
using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesTest, givenEventAddressWhenEncodeAndMtlThenMocsIndex2IsSet, IsMTL) {
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.eventAddress = eventAddress;
    dispatchArgs.isTimestampEvent = true;
    dispatchArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getHardwareInfo());

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);

    auto gmmHelper = pDevice->getGmmHelper();

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), cmd->getPostSync().getMocs());
}
