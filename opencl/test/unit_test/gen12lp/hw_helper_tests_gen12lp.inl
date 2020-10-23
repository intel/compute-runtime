/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/gen12lp/special_ult_helper_gen12lp.h"
#include "opencl/test/unit_test/helpers/hw_helper_tests.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "engine_node.h"

using HwHelperTestGen12Lp = HwHelperTest;

GEN12LPTEST_F(HwHelperTestGen12Lp, givenTglLpThenAuxTranslationIsRequired) {
    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);

    for (auto isPureStateful : {false, true}) {
        KernelInfo kernelInfo{};
        KernelArgInfo argInfo{};
        argInfo.isBuffer = true;
        argInfo.pureStatefulBufferAccess = isPureStateful;
        kernelInfo.kernelArgInfo.push_back(std::move(argInfo));

        EXPECT_EQ(!isPureStateful, clHwHelper.requiresAuxResolves(kernelInfo));
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, WhenGettingMaxBarriersPerSliceThenCorrectSizeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenGen12LpSkuWhenGettingCapabilityCoherencyFlagThenExpectValidValue) {
    auto &helper = HwHelper::get(renderCoreFamily);
    bool coherency = false;
    helper.setCapabilityCoherencyFlag(&hardwareInfo, coherency);

    const bool checkDone = SpecialUltHelperGen12lp::additionalCoherencyCheck(hardwareInfo.platform.eProductFamily, coherency);
    if (checkDone) {
        return;
    }

    if (hardwareInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP) {
        hardwareInfo.platform.usRevId = 0x1;
        helper.setCapabilityCoherencyFlag(&hardwareInfo, coherency);
        EXPECT_TRUE(coherency);
        hardwareInfo.platform.usRevId = 0x0;
        helper.setCapabilityCoherencyFlag(&hardwareInfo, coherency);
        EXPECT_FALSE(coherency);
    } else {
        EXPECT_TRUE(coherency);
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, WhenGettingPitchAlignmentForImageThenCorrectValueIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    auto stepping = hardwareInfo.platform.usRevId;

    if (SpecialUltHelperGen12lp::shouldPerformimagePitchAlignment(hardwareInfo.platform.eProductFamily) && stepping == 0) {
        EXPECT_EQ(64u, helper.getPitchAlignmentForImage(&hardwareInfo));
    } else {
        EXPECT_EQ(4u, helper.getPitchAlignmentForImage(&hardwareInfo));
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, WhenAdjustingDefaultEngineTypeThenRcsIsSet) {
    hardwareInfo.featureTable.ftrCCSNode = false;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenGen12LpPlatformWhenSetupHardwareCapabilitiesIsCalledThenShouldSetCorrectValues) {
    HardwareCapabilities hwCaps = {0};

    auto &hwHelper = HwHelper::get(renderCoreFamily);
    hwHelper.setupHardwareCapabilities(&hwCaps, hardwareInfo);

    EXPECT_EQ(2048u, hwCaps.image3DMaxHeight);
    EXPECT_EQ(2048u, hwCaps.image3DMaxWidth);
    EXPECT_TRUE(hwCaps.isStatelesToStatefullWithOffsetSupported);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenCompressionFtrEnabledWhenAskingForPageTableManagerThenReturnCorrectValue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    hardwareInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    hardwareInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_FALSE(helper.isPageTableManagerSupported(hardwareInfo));

    hardwareInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    hardwareInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_TRUE(helper.isPageTableManagerSupported(hardwareInfo));

    hardwareInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    hardwareInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(helper.isPageTableManagerSupported(hardwareInfo));

    hardwareInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    hardwareInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(helper.isPageTableManagerSupported(hardwareInfo));
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenDifferentSizesOfAllocationWhenCheckingCompressionPreferenceThenReturnCorrectValue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    const size_t sizesToCheck[] = {128, 256, 512, 1023, 1024, 1025};
    for (size_t size : sizesToCheck) {
        EXPECT_FALSE(helper.obtainRenderBufferCompressionPreference(hardwareInfo, size));
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeNotSetAndBcsInfoSetWhenGetGpgpuEnginesThenReturnThreeRcsEnginesAndOneBcsEngine) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(4u, device->engines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[3].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeNotSetWhenGetGpgpuEnginesThenReturnThreeRcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    const auto expectedEnginesCount = HwInfoConfig::get(hwInfo.platform.eProductFamily)->isEvenContextCountRequired() ? 4u : 3u;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(expectedEnginesCount, device->engines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);

    EXPECT_EQ(expectedEnginesCount, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenEvenContextCountRequiredWhenGetGpgpuEnginesIsCalledThenInsertAdditionalEngineAtTheEndIfNeeded) {
    struct MockHwInfoConfig : HwInfoConfigHw<IGFX_UNKNOWN> {
        MockHwInfoConfig() {}
        bool evenContextCountRequired = false;
        bool isEvenContextCountRequired() override {
            return evenContextCountRequired;
        }
    };

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    MockHwInfoConfig hwInfoConfig;
    VariableBackup<HwInfoConfig *> hwInfoConfigBackup{&hwInfoConfigFactory[hwInfo.platform.eProductFamily], &hwInfoConfig};

    hwInfoConfig.evenContextCountRequired = false;
    auto engines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(3u, engines.size());

    hwInfoConfig.evenContextCountRequired = true;
    engines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[engines.size() - 1].first);

    hwInfo.featureTable.ftrCCSNode = true;
    engines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());

    hwInfo.featureTable.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    engines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());

    hwInfo.featureTable.ftrCCSNode = false;
    engines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeSetWhenGetGpgpuEnginesThenReturnTwoRcsAndCcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(4u, device->engines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[3].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeSetFtrGpGpuMidThreadLevelPreemptSetWhenGetGpgpuEnginesThenReturn2RcsAndCcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.featureTable.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.featureTable.ftrGpGpuMidThreadLevelPreempt = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    size_t retDeivices = 3u + hwInfoConfig->isEvenContextCountRequired();
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(retDeivices, device->engines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(retDeivices, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[2].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeSetFtrGpGpuMidThreadLevelPreemptNotSetWhenGetGpgpuEnginesThenReturn2RcsAnd2CcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.featureTable.ftrGpGpuMidThreadLevelPreempt = false;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(4u, device->engines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[3].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeSetAndDefaultRcsWhenGetGpgpuEnginesThenReturnAppropriateNumberOfRcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    const auto expectedEnginesCount = HwInfoConfig::get(hwInfo.platform.eProductFamily)->isEvenContextCountRequired() ? 4u : 3u;
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(expectedEnginesCount, device->engines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(expectedEnginesCount, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    if (expectedEnginesCount == 4) {
        EXPECT_EQ(aub_stream::ENGINE_RCS, engines[3].first);
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenTgllpWhenIsFusedEuDispatchEnabledIsCalledThenResultIsCorrect) {
    DebugManagerStateRestore restorer;
    auto &helper = HwHelper::get(renderCoreFamily);
    auto &waTable = hardwareInfo.workaroundTable;
    bool wa;
    int32_t debugKey;
    size_t expectedResult;

    const std::array<std::tuple<bool, bool, int32_t>, 6> testParams{std::make_tuple(true, false, -1),
                                                                    std::make_tuple(false, true, -1),
                                                                    std::make_tuple(true, false, 0),
                                                                    std::make_tuple(true, true, 0),
                                                                    std::make_tuple(false, false, 1),
                                                                    std::make_tuple(false, true, 1)};

    for (const auto &params : testParams) {
        std::tie(expectedResult, wa, debugKey) = params;
        waTable.waDisableFusedThreadScheduling = wa;
        DebugManager.flags.CFEFusedEUDispatch.set(debugKey);

        EXPECT_EQ(expectedResult, helper.isFusedEuDispatchEnabled(hardwareInfo));
    }
}

class HwHelperTestsGen12LpBuffer : public ::testing::Test {
  public:
    void SetUp() override {
        ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
        device = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, rootDeviceIndex));
        context = std::make_unique<MockContext>(device.get(), true);
        context->contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    }

    const uint32_t rootDeviceIndex = 0u;
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;
};

GEN12LPTEST_F(HwHelperTestsGen12LpBuffer, givenCompressedBufferThenCheckResourceCompatibilityReturnsFalse) {
    auto &helper = HwHelper::get(renderCoreFamily);

    buffer.reset(Buffer::create(context.get(), 0, MemoryConstants::cacheLineSize, nullptr, retVal));

    buffer->getGraphicsAllocation(rootDeviceIndex)->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);

    EXPECT_FALSE(helper.checkResourceCompatibility(*buffer->getGraphicsAllocation(rootDeviceIndex)));
}

GEN12LPTEST_F(HwHelperTestsGen12LpBuffer, givenBufferThenCheckResourceCompatibilityReturnsTrue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    buffer.reset(Buffer::create(context.get(), 0, MemoryConstants::cacheLineSize, nullptr, retVal));

    buffer->getGraphicsAllocation(rootDeviceIndex)->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);

    EXPECT_TRUE(helper.checkResourceCompatibility(*buffer->getGraphicsAllocation(rootDeviceIndex)));
}

using LriHelperTestsGen12Lp = ::testing::Test;

GEN12LPTEST_F(LriHelperTestsGen12Lp, whenProgrammingLriCommandThenExpectMmioRemapEnable) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    EXPECT_TRUE(expectedLri.getMmioRemapEnable());
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);
    expectedLri.setMmioRemapEnable(false);

    LriHelper<FamilyType>::program(&stream, address, data, false);
    MI_LOAD_REGISTER_IMM *lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(buffer.get());
    ASSERT_NE(nullptr, lri);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(lri, stream.getCpuBase());
    EXPECT_TRUE(memcmp(lri, &expectedLri, sizeof(MI_LOAD_REGISTER_IMM)) == 0);
}

using MemorySynchronizatiopCommandsTests = ::testing::Test;

GEN12LPTEST_F(MemorySynchronizatiopCommandsTests, whenSettingCacheFlushExtraFieldsThenExpectHdcFlushSet) {
    PipeControlArgs args;
    args.constantCacheInvalidationEnable = true;

    MemorySynchronizationCommands<FamilyType>::setCacheFlushExtraProperties(args);
    EXPECT_TRUE(args.hdcPipelineFlush);
    EXPECT_FALSE(args.constantCacheInvalidationEnable);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenUnknownProductFamilyWhenGettingIsWorkaroundRequiredThenFalseIsReturned) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping};
    hardwareInfo.platform.eProductFamily = IGFX_UNKNOWN;

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(stepping, hardwareInfo);

        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenGen12WhenCallIsPackedSupportedThenReturnTrue) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(helper.packedFormatsSupported());
}

GEN12LPTEST_F(HwHelperTestGen12Lp, whenRequestingMocsThenProperMocsIndicesAreBeingReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    auto gmmHelper = this->pDevice->getGmmHelper();

    const auto mocsNoCache = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
    const auto mocsL3 = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;

    EXPECT_EQ(mocsNoCache, helper.getMocsIndex(*gmmHelper, false, false));
    EXPECT_EQ(mocsNoCache, helper.getMocsIndex(*gmmHelper, false, true));
    EXPECT_EQ(mocsL3, helper.getMocsIndex(*gmmHelper, true, false));
    EXPECT_EQ(mocsL3, helper.getMocsIndex(*gmmHelper, true, true));
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenL1ForceEnabledWhenRequestingMocsThenProperMocsIndicesAreBeingReturned) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.ForceL1Caching.set(1);

    auto &helper = HwHelper::get(renderCoreFamily);
    auto gmmHelper = this->pDevice->getGmmHelper();

    const auto mocsNoCache = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
    const auto mocsL3 = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;
    const auto mocsL1 = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST) >> 1;

    EXPECT_EQ(mocsNoCache, helper.getMocsIndex(*gmmHelper, false, false));
    EXPECT_EQ(mocsNoCache, helper.getMocsIndex(*gmmHelper, false, true));
    EXPECT_EQ(mocsL3, helper.getMocsIndex(*gmmHelper, true, false));
    EXPECT_EQ(mocsL1, helper.getMocsIndex(*gmmHelper, true, true));
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenL1ForceDisabledWhenRequestingMocsThenProperMocsIndicesAreBeingReturned) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.ForceL1Caching.set(0);

    auto &helper = HwHelper::get(renderCoreFamily);
    auto gmmHelper = this->pDevice->getGmmHelper();

    const auto mocsNoCache = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
    const auto mocsL3 = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;

    EXPECT_EQ(mocsNoCache, helper.getMocsIndex(*gmmHelper, false, false));
    EXPECT_EQ(mocsNoCache, helper.getMocsIndex(*gmmHelper, false, true));
    EXPECT_EQ(mocsL3, helper.getMocsIndex(*gmmHelper, true, false));
    EXPECT_EQ(mocsL3, helper.getMocsIndex(*gmmHelper, true, true));
}

HWTEST2_F(HwHelperTestGen12Lp, givenRevisionEnumThenProperValueForIsWorkaroundRequiredIsReturned, IsRKL) {
    std::vector<unsigned short> steppings;
    HardwareInfo hardwareInfo = *defaultHwInfo;

    steppings.push_back(0x0); //A0
    steppings.push_back(0x4); //B0
    steppings.push_back(0x5); //undefined

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = stepping;
        HwHelper &hwHelper = HwHelper::get(renderCoreFamily);

        if (stepping == 0x0) {
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        } else if (stepping == 0x1 || stepping == 0x5) {
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
        }
    }
}

HWTEST2_F(HwHelperTestGen12Lp, givenRevisionEnumThenProperValueForIsWorkaroundRequiredIsReturned, IsADLS) {
    std::vector<unsigned short> steppings;
    HardwareInfo hardwareInfo = *defaultHwInfo;

    steppings.push_back(0x0); //A0
    steppings.push_back(0x4); //B0
    steppings.push_back(0x5); //undefined

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = stepping;
        HwHelper &hwHelper = HwHelper::get(renderCoreFamily);

        if (stepping == 0x0) {
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        } else if (stepping == 0x4 || stepping == 0x5) {
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        }
    }
}
