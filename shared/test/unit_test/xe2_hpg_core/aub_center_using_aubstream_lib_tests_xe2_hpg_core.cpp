/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "aubstream/aubstream.h"

#include <array>

using namespace NEO;

namespace aub_stream_stubs {
extern aub_stream::MMIOList mmioListInjected; // NOLINT(readability-identifier-naming)
} // namespace aub_stream_stubs
using AubHelperCompressionXe2AndLaterTests = ::testing::Test;

HWTEST2_F(AubHelperCompressionXe2AndLaterTests, givenCompressionEnabledAndWhenCreatingAubCenterThenPassAdditionalMmioList, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(true);
    MockExecutionEnvironment executionEnvironment{};
    RootDeviceEnvironment &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    const std::string aubName("file.aub");

    {
        debugManager.flags.ForceBufferCompressionFormat.set(0xE);

        VariableBackup<aub_stream::MMIOList> backup(&aub_stream_stubs::mmioListInjected);
        AubCenter aubCenter(rootDeviceEnvironment, false, aubName, CommandStreamReceiverType::aub);

        ASSERT_EQ(1u, aub_stream_stubs::mmioListInjected.size());

        EXPECT_EQ(0x4148u, aub_stream_stubs::mmioListInjected[0].first);
        EXPECT_EQ(0xEu, aub_stream_stubs::mmioListInjected[0].second);
    }
}

HWTEST2_F(AubHelperCompressionXe2AndLaterTests, givenCompressionEnabledAndAddMmioRegisterListDebugFlagsWhenCreatingAubCenterThenPassAdditionalMmioList, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(true);

    const std::string aubName("file.aub");

    MockExecutionEnvironment executionEnvironment{};
    RootDeviceEnvironment &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &gmmHelper = *rootDeviceEnvironment.getGmmHelper();
    uint32_t compressionFormat = gmmHelper.getClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);

    {
        VariableBackup<aub_stream::MMIOList> backup(&aub_stream_stubs::mmioListInjected);

        debugManager.flags.AubDumpAddMmioRegistersList.set("0x123;0x1;0x234;0x2");
        AubCenter aubCenter(rootDeviceEnvironment, false, aubName, CommandStreamReceiverType::aub);

        ASSERT_EQ(3u, aub_stream_stubs::mmioListInjected.size());

        EXPECT_EQ(0x4148u, aub_stream_stubs::mmioListInjected[0].first);
        EXPECT_EQ(compressionFormat, aub_stream_stubs::mmioListInjected[0].second);

        EXPECT_EQ(0x123u, aub_stream_stubs::mmioListInjected[1].first);
        EXPECT_EQ(0x1u, aub_stream_stubs::mmioListInjected[1].second);

        EXPECT_EQ(0x234u, aub_stream_stubs::mmioListInjected[2].first);
        EXPECT_EQ(0x2u, aub_stream_stubs::mmioListInjected[2].second);
    }
}

HWTEST2_F(AubHelperCompressionXe2AndLaterTests, givenCompressionDisabledWhenCreatingAubCenterThenDontPassAdditionalMmioList, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore stateRestore;

    const std::string aubName("file.aub");

    static constexpr std::array<std::pair<bool, bool>, 4> params = {{
        std::make_pair(true, false),
        std::make_pair(false, true),
        std::make_pair(false, false),
        std::make_pair(true, true),
    }};

    MockExecutionEnvironment executionEnvironment{};
    RootDeviceEnvironment &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &gmmHelper = *rootDeviceEnvironment.getGmmHelper();
    uint32_t compressionFormat = gmmHelper.getClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);

    for (auto &param : params) {
        VariableBackup<aub_stream::MMIOList> backup(&aub_stream_stubs::mmioListInjected);

        debugManager.flags.RenderCompressedBuffersEnabled.set(param.first);
        debugManager.flags.RenderCompressedImagesEnabled.set(param.second);
        AubCenter aubCenter(rootDeviceEnvironment, false, aubName, CommandStreamReceiverType::aub);

        if (param.first || param.second) {
            ASSERT_EQ(1u, aub_stream_stubs::mmioListInjected.size());

            EXPECT_EQ(0x4148u, aub_stream_stubs::mmioListInjected[0].first);
            EXPECT_EQ(compressionFormat, aub_stream_stubs::mmioListInjected[0].second);
        } else {
            EXPECT_EQ(0u, aub_stream_stubs::mmioListInjected.size());
        }
    }
}
