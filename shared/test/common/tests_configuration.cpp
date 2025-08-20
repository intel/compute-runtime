/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/tests_configuration.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/options.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

void adjustHwInfoForTests(HardwareInfo &hwInfoForTests, uint32_t euPerSubSlice, uint32_t sliceCount, uint32_t subSlicePerSliceCount, int dieRecovery) {
    auto compilerProductHelper = CompilerProductHelper::create(hwInfoForTests.platform.eProductFamily);
    hwInfoForTests.ipVersion.value = compilerProductHelper->getHwIpVersion(hwInfoForTests);
    auto releaseHelper = ReleaseHelper::create(hwInfoForTests.ipVersion);

    auto hwInfoConfig = compilerProductHelper->getHwInfoConfig(hwInfoForTests);
    setHwInfoValuesFromConfig(hwInfoConfig, hwInfoForTests);

    // set Gt and FeatureTable to initial state
    bool setupFeatureTableAndWorkaroundTable = isAubTestMode(testMode);
    hardwareInfoSetup[hwInfoForTests.platform.eProductFamily](&hwInfoForTests, setupFeatureTableAndWorkaroundTable, hwInfoConfig, releaseHelper.get());
    GT_SYSTEM_INFO &gtSystemInfo = hwInfoForTests.gtSystemInfo;

    // and adjust dynamic values if not specified
    sliceCount = sliceCount > 0 ? sliceCount : gtSystemInfo.SliceCount;
    subSlicePerSliceCount = subSlicePerSliceCount > 0 ? subSlicePerSliceCount : (gtSystemInfo.SubSliceCount / sliceCount);
    euPerSubSlice = euPerSubSlice > 0 ? euPerSubSlice : gtSystemInfo.MaxEuPerSubSlice;

    gtSystemInfo.SliceCount = sliceCount;
    for (uint32_t slice = 0; slice < sliceCount; slice++) {
        gtSystemInfo.SliceInfo[slice].Enabled = true;
        gtSystemInfo.SliceInfo[slice].SubSliceInfo[0].EuEnabledCount = 1;
        gtSystemInfo.SliceInfo[slice].SubSliceInfo[0].Enabled = true;
    }
    for (uint32_t slice = sliceCount; slice < GT_MAX_SLICE; slice++) {
        gtSystemInfo.SliceInfo[slice].Enabled = false;
    }
    gtSystemInfo.SubSliceCount = gtSystemInfo.SliceCount * subSlicePerSliceCount;
    gtSystemInfo.DualSubSliceCount = gtSystemInfo.SubSliceCount;
    gtSystemInfo.EUCount = gtSystemInfo.SubSliceCount * euPerSubSlice - dieRecovery;
    gtSystemInfo.NumThreadsPerEu = 8;
    gtSystemInfo.ThreadCount = gtSystemInfo.EUCount * gtSystemInfo.NumThreadsPerEu;
    gtSystemInfo.MaxEuPerSubSlice = std::max(gtSystemInfo.MaxEuPerSubSlice, euPerSubSlice);
    gtSystemInfo.MaxSlicesSupported = std::max(gtSystemInfo.MaxSlicesSupported, gtSystemInfo.SliceCount);
    gtSystemInfo.MaxSubSlicesSupported = std::max(gtSystemInfo.MaxSubSlicesSupported, gtSystemInfo.SubSliceCount);
    gtSystemInfo.SLMSizeInKb = 1;
}
void adjustCsrType(TestMode testMode) {
    if (testMode == TestMode::aubTestsWithTbx) {
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbxWithAub));
    } else if (testMode == TestMode::aubTests) {
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::aub));
    } else if (testMode == TestMode::tbxTests) {
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    } else if (testMode == TestMode::aubTestsWithoutOutputFiles) {
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::nullAub));
    }
}
} // namespace NEO
