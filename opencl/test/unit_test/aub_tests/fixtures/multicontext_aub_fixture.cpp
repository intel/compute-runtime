/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_aub_fixture.h"

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/tests_configuration.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {

void MulticontextAubFixture::setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool enableCompression) {
    this->numberOfEnabledTiles = numberOfTiles;
    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();

    cl_int retVal = CL_SUCCESS;
    debugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::batchedDispatch));
    debugManager.flags.CreateMultipleSubDevices.set(numberOfTiles);
    if (testMode == TestMode::aubTestsWithTbx) {
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::CSR_TBX_WITH_AUB));
    } else {
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::CSR_AUB));
    }

    HardwareInfo localHwInfo = *defaultHwInfo;
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

    localHwInfo.capabilityTable.blitterOperationsSupported = true;
    if (debugManager.flags.EnableBlitterOperationsSupport.get() != -1) {
        localHwInfo.capabilityTable.blitterOperationsSupported = !!debugManager.flags.EnableBlitterOperationsSupport.get();
    }

    debugManager.flags.RenderCompressedBuffersEnabled.set(enableCompression);
    debugManager.flags.RenderCompressedImagesEnabled.set(enableCompression);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(false);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;

    platformsImpl->clear();
    constructPlatform()->peekExecutionEnvironment()->prepareRootDeviceEnvironments(1u);
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(&localHwInfo);
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    initPlatform();

    rootDevice = platform()->getClDevice(0);

    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->setRcsExposure();
    auto &gfxCoreHelper = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->getHelper<GfxCoreHelper>();
    auto renderEngine = aub_stream::NUM_ENGINES;
    for (auto &engine : gfxCoreHelper.getGpgpuEngineInstances(*platform()->peekExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex])) {
        if (!EngineHelpers::isCcs(engine.first)) {
            renderEngine = engine.first;
            break;
        }
    }
    bool isRenderEngineSupported = (renderEngine != aub_stream::NUM_ENGINES);
    auto firstEngine = isRenderEngineSupported ? renderEngine : aub_stream::ENGINE_CCS;

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

    auto createCommandQueueForEngine = [&](uint32_t tileNumber, size_t engineFamily, size_t engineIndex) {
        cl_queue_properties properties[] = {CL_QUEUE_FAMILY_INTEL, engineFamily, CL_QUEUE_INDEX_INTEL, engineIndex, 0};
        auto clQueue = clCreateCommandQueueWithProperties(context.get(), tileDevices[tileNumber], properties, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        return std::unique_ptr<CommandQueue>(castToObject<CommandQueue>(clQueue));
    };

    EXPECT_EQ(rootDeviceIndex, rootDevice->getRootDeviceIndex());
    {
        cl_device_id deviceId = rootDevice;
        ClDeviceVector clDeviceVector{&deviceId, 1};
        if (numberOfTiles > 1) {
            for (uint32_t i = 0; i < numberOfTiles; i++) {
                clDeviceVector.push_back(rootDevice->getNearestGenericSubDevice(i));
            }
        }
        context.reset(MockContext::create<MockContext>(nullptr, clDeviceVector, nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    commandQueues.resize(numberOfTiles);
    for (uint32_t tile = 0; tile < numberOfTiles; tile++) {
        tileDevices.push_back(rootDevice->getNearestGenericSubDevice(tile));
        EXPECT_NE(nullptr, tileDevices[tile]);
        if (EnabledCommandStreamers::single == enabledCommandStreamers) {
            auto engineGroupType = isRenderEngineSupported ? EngineGroupType::renderCompute : EngineGroupType::compute;
            auto familyQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType);
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, familyQueue, 0));
        } else if (EnabledCommandStreamers::dual == enabledCommandStreamers) {
            auto ccsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
            if (isRenderEngineSupported) {
                auto rcsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::renderCompute);
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, rcsQueue, 0));
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 0));
            } else {
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 0));
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 1));
            }
        } else if (EnabledCommandStreamers::all == enabledCommandStreamers) {
            if (isRenderEngineSupported) {
                auto rcsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::renderCompute);
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, rcsQueue, 0));
            }
            auto ccsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 0));
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 1));
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 2));
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 3));
        }
    }

    {
        cl_int retVal = CL_SUCCESS;
        cl_device_id deviceId = rootDevice;
        multiTileDefaultContext.reset(MockContext::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

void MulticontextAubFixture::overridePlatformConfigForAllEnginesSupport(HardwareInfo &localHwInfo) {
    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    printf("\nWARNING: Platform configuration for %s_%s test forced to %dtx8x4x16\n",
           testInfo->test_case_name(), testInfo->name(), numberOfEnabledTiles);

    bool setupCalled = false;

    auto releaseHelper = ReleaseHelper::create(localHwInfo.ipVersion);

    if (localHwInfo.platform.eRenderCoreFamily == IGFX_XE_HPG_CORE) {
#ifdef SUPPORT_DG2
        if (localHwInfo.platform.eProductFamily == IGFX_DG2) {
            ASSERT_TRUE(numberOfEnabledTiles == 1);
            setupCalled = true;

            Dg2HwConfig::setupHardwareInfo(&localHwInfo, true, releaseHelper.get());

            // Mock values
            localHwInfo.gtSystemInfo.SliceCount = 8;
            localHwInfo.gtSystemInfo.SubSliceCount = 32;
            localHwInfo.gtSystemInfo.EUCount = 512;

            localHwInfo.gtSystemInfo.CCSInfo.IsValid = true;
            localHwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
            localHwInfo.gtSystemInfo.CCSInfo.Instances.CCSEnableMask = 0b1111;
        }
#endif
    }

    if (localHwInfo.platform.eRenderCoreFamily == IGFX_XE_HPC_CORE) {
#ifdef SUPPORT_PVC
        if (localHwInfo.platform.eProductFamily == IGFX_PVC) {
            setupCalled = true;

            PvcHwConfig::setupHardwareInfo(&localHwInfo, true, releaseHelper.get());

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

} // namespace NEO
