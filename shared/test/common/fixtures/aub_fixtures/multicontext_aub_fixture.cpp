/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/aub_fixtures/multicontext_aub_fixture.h"

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/tests_configuration.h"

namespace NEO {
void MulticontextAubFixture::setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool enableCompression) {
    this->numberOfEnabledTiles = numberOfTiles;
    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();

    debugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(dispatchMode));
    debugManager.flags.CreateMultipleSubDevices.set(numberOfTiles);

    HardwareInfo localHwInfo = *defaultHwInfo;

    if (debugManager.flags.BlitterEnableMaskOverride.get() > 0) {
        localHwInfo.featureTable.ftrBcsInfo = debugManager.flags.BlitterEnableMaskOverride.get();
    }

    if (numberOfEnabledTiles > 1 && localHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid == 0) {
        skipped = true;
        GTEST_SKIP();
    }

    if (EnabledCommandStreamers::single != enabledCommandStreamers) {
        overridePlatformConfigForAllEnginesSupport(localHwInfo);
    }

    if (numberOfTiles > 1) {
        localHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = (numberOfEnabledTiles > 1) ? 1 : 0;
        localHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = numberOfEnabledTiles;
        localHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0;
        for (uint32_t i = 0; i < numberOfEnabledTiles; i++) {
            localHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask |= (1 << i);
        }
    }
    debugManager.flags.RenderCompressedBuffersEnabled.set(enableCompression);
    debugManager.flags.RenderCompressedImagesEnabled.set(enableCompression);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(false);

    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(&localHwInfo, rootDeviceIndex));
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setRcsExposure();
    localHwInfo = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHelper<GfxCoreHelper>();
    auto renderEngine = aub_stream::NUM_ENGINES;
    isCcs1Supported = false;
    for (auto &engine : gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex])) {
        if (!EngineHelpers::isCcs(engine.first)) {
            renderEngine = engine.first;
        }
        if (engine.first == aub_stream::ENGINE_CCS1) {
            isCcs1Supported = true;
        }
    }
    isRenderEngineSupported = (renderEngine != aub_stream::NUM_ENGINES);
    auto firstEngine = isRenderEngineSupported ? renderEngine : aub_stream::ENGINE_CCS;
    if (isFirstEngineBcs) {
        firstEngine = aub_stream::ENGINE_BCS;
    }

    std::stringstream strfilename;
    strfilename << ApiSpecificConfig::getAubPrefixForSpecificApi();
    strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_";
    auto firstEngineName = gfxCoreHelper.getCsTraits(firstEngine).name;
    auto secondEngineName = gfxCoreHelper.getCsTraits(aub_stream::ENGINE_CCS).name;
    if (EnabledCommandStreamers::single == enabledCommandStreamers) { // name_RCS.aub or name_CCCS.aub or name_CCS.aub
        strfilename << firstEngineName;
    } else if (EnabledCommandStreamers::dual == enabledCommandStreamers) { // name_RCS_CCS.aub or name_CCCS_CCS.aub or name_CCS0_1.aub
        strfilename << firstEngineName;
        if (isRenderEngineSupported) {
            strfilename << "_" << secondEngineName;
        } else {
            strfilename << "0_1";
        }
    } else if (EnabledCommandStreamers::all == enabledCommandStreamers) { // name_RCS_CCS0_3.aub or name_CCCS_CCS0_3.aub or name_CCS0_3.aub
        strfilename << firstEngineName;
        if (isRenderEngineSupported) {
            strfilename << "_" << secondEngineName;
        }
        strfilename << "0_3";
    }

    auto filename = AUBCommandStreamReceiver::createFullFilePath(localHwInfo, strfilename.str(), rootDeviceIndex);

    debugManager.flags.AUBDumpCaptureFileName.set(filename);

    localHwInfo.capabilityTable.blitterOperationsSupported = true;
    if (debugManager.flags.EnableBlitterOperationsSupport.get() != -1) {
        localHwInfo.capabilityTable.blitterOperationsSupported = !!debugManager.flags.EnableBlitterOperationsSupport.get();
    }

    createDevices(localHwInfo, numberOfTiles);
}

void MulticontextAubFixture::overridePlatformConfigForAllEnginesSupport(HardwareInfo &localHwInfo) {
    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    printf("\nWARNING: Platform configuration for %s_%s test forced to %dtx8x4x16\n",
           testInfo->test_case_name(), testInfo->name(), numberOfEnabledTiles);

    bool setupCalled = false;

    auto releaseHelper = ReleaseHelper::create(localHwInfo.ipVersion);

    if (localHwInfo.platform.eRenderCoreFamily == IGFX_XE_HPG_CORE ||
        localHwInfo.platform.eRenderCoreFamily == IGFX_XE_HPC_CORE ||
        localHwInfo.platform.eRenderCoreFamily == IGFX_XE2_HPG_CORE ||
        localHwInfo.platform.eRenderCoreFamily == IGFX_XE3_CORE) {

        setupCalled = true;
        hardwareInfoSetup[localHwInfo.platform.eProductFamily](&localHwInfo, true, 0u, releaseHelper.get());

#ifdef SUPPORT_DG2
        if (localHwInfo.platform.eProductFamily == IGFX_DG2) {
            ASSERT_TRUE(numberOfEnabledTiles == 1);

            // Mock values
            localHwInfo.gtSystemInfo.SliceCount = 8;
            localHwInfo.gtSystemInfo.SubSliceCount = 32;
            localHwInfo.gtSystemInfo.EUCount = 512;

            localHwInfo.gtSystemInfo.CCSInfo.IsValid = true;
            localHwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
            localHwInfo.gtSystemInfo.CCSInfo.Instances.CCSEnableMask = 0b1111;
        }
#endif
#ifdef SUPPORT_PVC
        if (localHwInfo.platform.eProductFamily == IGFX_PVC) {
            // Mock values
            localHwInfo.gtSystemInfo.SliceCount = 8;
            localHwInfo.gtSystemInfo.SubSliceCount = 64;
            localHwInfo.gtSystemInfo.EUCount = 512;

            localHwInfo.gtSystemInfo.CCSInfo.IsValid = true;
            localHwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
            localHwInfo.gtSystemInfo.CCSInfo.Instances.CCSEnableMask = 0b1111;
        }
#endif
    }

    adjustPlatformOverride(localHwInfo, setupCalled);

    ASSERT_TRUE(setupCalled);
}

bool MulticontextAubFixture::isMemoryCompressed(CommandStreamReceiver *csr, void *gfxAddress) {
    auto releaseHelper = csr->getReleaseHelper();
    if (!releaseHelper || !releaseHelper->getFtrXe2Compression()) {
        return false;
    }
    auto svmAllocs = svmAllocsManager->getSVMAlloc(gfxAddress);
    if (!svmAllocs) {
        return false;
    }
    auto alloc = svmAllocs->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
    return alloc->isCompressionEnabled();
}

} // namespace NEO
