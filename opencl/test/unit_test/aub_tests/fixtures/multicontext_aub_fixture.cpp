/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_aub_fixture.h"

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/tests_configuration.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {

void MulticontextAubFixture::setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool enableCompression) {
    this->numberOfEnabledTiles = numberOfTiles;
    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();

    cl_int retVal = CL_SUCCESS;
    DebugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::BatchedDispatch));
    DebugManager.flags.CreateMultipleSubDevices.set(numberOfTiles);
    if (testMode == TestMode::AubTestsWithTbx) {
        DebugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::CSR_TBX_WITH_AUB));
    } else {
        DebugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::CSR_AUB));
    }

    HardwareInfo localHwInfo = *defaultHwInfo;
    if ((EnabledCommandStreamers::All == enabledCommandStreamers) && localHwInfo.gtSystemInfo.SliceCount < 8) {
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
    if (DebugManager.flags.EnableBlitterOperationsSupport.get() != -1) {
        localHwInfo.capabilityTable.blitterOperationsSupported = !!DebugManager.flags.EnableBlitterOperationsSupport.get();
    }

    auto &hwHelper = HwHelper::get(localHwInfo.platform.eRenderCoreFamily);
    auto engineType = getChosenEngineType(localHwInfo);
    auto renderEngine = aub_stream::NUM_ENGINES;
    for (auto &engine : hwHelper.getGpgpuEngineInstances(localHwInfo)) {
        if (!EngineHelpers::isCcs(engine.first)) {
            renderEngine = engine.first;
            break;
        }
    }
    ASSERT_NE(aub_stream::NUM_ENGINES, renderEngine);

    auto renderEngineName = hwHelper.getCsTraits(renderEngine).name;

    std::stringstream strfilename;
    strfilename << ApiSpecificConfig::getAubPrefixForSpecificApi();
    strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_";
    if (EnabledCommandStreamers::Single == enabledCommandStreamers) {
        strfilename << renderEngineName;
    } else if (EnabledCommandStreamers::Dual == enabledCommandStreamers) {
        strfilename << renderEngineName << "_CCS0";
    } else if (EnabledCommandStreamers::All == enabledCommandStreamers) {
        strfilename << renderEngineName << "_CCS0_3"; // xehp_config_name_RCS_CCS0_3.aub
    }

    auto filename = AUBCommandStreamReceiver::createFullFilePath(localHwInfo, strfilename.str(), rootDeviceIndex);

    DebugManager.flags.AUBDumpCaptureFileName.set(filename);

    auto createCommandQueueForEngine = [&](uint32_t tileNumber, size_t engineFamily, size_t engineIndex) {
        cl_queue_properties properties[] = {CL_QUEUE_FAMILY_INTEL, engineFamily, CL_QUEUE_INDEX_INTEL, engineIndex, 0};
        auto clQueue = clCreateCommandQueueWithProperties(context.get(), tileDevices[tileNumber], properties, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        return std::unique_ptr<CommandQueue>(castToObject<CommandQueue>(clQueue));
    };

    DebugManager.flags.RenderCompressedBuffersEnabled.set(enableCompression);
    DebugManager.flags.RenderCompressedImagesEnabled.set(enableCompression);
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(false);

    platformsImpl->clear();
    VariableBackup<UltHwConfig> backup(&ultHwConfig);

    ultHwConfig.useHwCsr = true;
    constructPlatform()->peekExecutionEnvironment()->prepareRootDeviceEnvironments(1u);
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(&localHwInfo);
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    initPlatform();

    rootDevice = platform()->getClDevice(0);
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
        if (EnabledCommandStreamers::Single == enabledCommandStreamers) {
            if (EngineHelpers::isCcs(engineType)) {
                auto familyQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::Compute);
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, familyQueue, 0));
            } else {
                auto familyQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::RenderCompute);
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, familyQueue, 0));
            }
        } else if (EnabledCommandStreamers::Dual == enabledCommandStreamers) {
            auto rcsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::RenderCompute);
            auto ccsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::Compute);
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, rcsQueue, 0));
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 0));
        } else if (EnabledCommandStreamers::All == enabledCommandStreamers) {
            auto rcsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::RenderCompute);
            auto ccsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::Compute);
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, rcsQueue, 0));
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

void MulticontextAubFixture::tearDown() {
    auto filename = DebugManager.flags.AUBDumpCaptureFileName.get();

    std::string tileString = std::to_string(numberOfEnabledTiles) + "tx";

    if (numberOfEnabledTiles > 1) {
        EXPECT_NE(std::string::npos, filename.find(tileString));
    } else {
        EXPECT_EQ(std::string::npos, filename.find(tileString));
    }
}

void MulticontextAubFixture::overridePlatformConfigForAllEnginesSupport(HardwareInfo &localHwInfo) {
    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    printf("\nWARNING: Platform configuration for %s_%s test forced to %dtx8x4x16\n",
           testInfo->test_case_name(), testInfo->name(), numberOfEnabledTiles);

    bool setupCalled = false;

    if (localHwInfo.platform.eRenderCoreFamily == IGFX_XE_HP_CORE) {
#ifdef SUPPORT_XE_HP_SDV
        if (localHwInfo.platform.eProductFamily == IGFX_XE_HP_SDV) {
            setupCalled = true;
            XehpSdvHwConfig::setupHardwareInfo(&localHwInfo, true);

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

    if (localHwInfo.platform.eRenderCoreFamily == IGFX_XE_HPG_CORE) {
#ifdef SUPPORT_DG2
        if (localHwInfo.platform.eProductFamily == IGFX_DG2) {
            ASSERT_TRUE(numberOfEnabledTiles == 1);
            setupCalled = true;

            Dg2HwConfig::setupHardwareInfo(&localHwInfo, true);

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

            PvcHwConfig::setupHardwareInfo(&localHwInfo, true);

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
