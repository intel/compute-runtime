/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/encoders/test_encode_dispatch_kernel_dg2_and_later.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/fixtures/implicit_scaling_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;

using CommandEncodeStatesTestXe2AndLater = Test<CommandEncodeStatesFixture>;

using CommandEncoderTestXe2AndLater = ::testing::Test;

HWTEST2_F(CommandEncoderTestXe2AndLater, whenAdjustCompressionFormatForPlanarImageThenNothingHappens, IsAtLeastXe2HpgCore) {
    for (auto plane : {GMM_NO_PLANE, GMM_PLANE_Y, GMM_PLANE_U, GMM_PLANE_V}) {
        uint32_t compressionFormat = 0u;
        EncodeWA<FamilyType>::adjustCompressionFormatForPlanarImage(compressionFormat, plane);
        EXPECT_EQ(0u, compressionFormat);

        compressionFormat = 0xFFu;
        EncodeWA<FamilyType>::adjustCompressionFormatForPlanarImage(compressionFormat, plane);
        EXPECT_EQ(0xFFu, compressionFormat);
    }
}

HWTEST2_F(CommandEncodeStatesTestXe2AndLater, whenDebugFlagIsEnabledForAdjustPipelineSelectThenCommandIsAdded, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PipelinedPipelineSelect.set(true);

    const auto usedSpaceBefore = cmdContainer->getCommandStream()->getUsed();
    NEO::EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer, descriptor);
    const auto usedSpaceAfter = cmdContainer->getCommandStream()->getUsed();

    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);
}

HWTEST2_F(CommandEncodeStatesTestXe2AndLater, whenDebugFlagIsDisabledForAdjustPipelineSelectThenNoCommandIsAdded, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PipelinedPipelineSelect.set(false);

    const auto usedSpaceBefore = cmdContainer->getCommandStream()->getUsed();
    NEO::EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer, descriptor);
    const auto usedSpaceAfter = cmdContainer->getCommandStream()->getUsed();

    EXPECT_EQ(usedSpaceAfter, usedSpaceBefore);
}

HWTEST2_F(ImplicitScalingTests, GivenXeAtLeastHpg2WhenCheckingPipeControlStallRequiredThenExpectTrue, IsAtLeastXe2HpgCore) {
    EXPECT_FALSE(ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired());
}
