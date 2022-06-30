/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "engine_node.h"

using HwHelperTestGen12Lp = HwHelperTest;

GEN12LPTEST_F(HwHelperTestGen12Lp, givenTglLpThenAuxTranslationIsRequired) {
    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);

    for (auto accessedUsingStatelessAddressingMode : {true, false}) {
        KernelInfo kernelInfo{};

        ArgDescriptor arg;
        arg.as<ArgDescPointer>(true).accessedUsingStatelessAddressingMode = accessedUsingStatelessAddressingMode;
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(std::move(arg));

        EXPECT_EQ(accessedUsingStatelessAddressingMode, clHwHelper.requiresAuxResolves(kernelInfo, hardwareInfo));
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, WhenGettingMaxBarriersPerSliceThenCorrectSizeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
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
    hardwareInfo.featureTable.flags.ftrCCSNode = false;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenDifferentSizesOfAllocationWhenCheckingCompressionPreferenceThenReturnCorrectValue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    const size_t sizesToCheck[] = {128, 256, 512, 1023, 1024, 1025};
    for (size_t size : sizesToCheck) {
        EXPECT_FALSE(helper.isBufferSizeSuitableForCompression(size, *defaultHwInfo));
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeNotSetAndBcsInfoSetWhenGetGpgpuEnginesThenReturnThreeRcsEnginesAndOneBcsEngine) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(4u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[3].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeNotSetAndCcsDefualtEngineWhenGetGpgpuEnginesThenReturnTwoRcsEnginesAndOneCcs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(3u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[2].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeNotSetWhenGetGpgpuEnginesThenReturnThreeRcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);

    EXPECT_EQ(3u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeSetWhenGetGpgpuEnginesThenReturnTwoRcsAndCcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(4u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[3].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeSetFtrGpGpuMidThreadLevelPreemptSetWhenGetGpgpuEnginesThenReturn2RcsAndCcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.featureTable.flags.ftrGpGpuMidThreadLevelPreempt = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(3u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[2].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeSetFtrGpGpuMidThreadLevelPreemptNotSetWhenGetGpgpuEnginesThenReturn2RcsAnd2CcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.featureTable.flags.ftrGpGpuMidThreadLevelPreempt = false;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(4u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[3].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenFtrCcsNodeSetAndDefaultRcsWhenGetGpgpuEnginesThenReturnAppropriateNumberOfRcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(3u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenTgllpWhenIsFusedEuDispatchEnabledIsCalledThenResultIsCorrect) {
    DebugManagerStateRestore restorer;
    auto &helper = HwHelper::get(renderCoreFamily);
    auto &waTable = hardwareInfo.workaroundTable;

    std::tuple<bool, bool, int32_t> testParams[]{
        {true, false, -1},
        {false, true, -1},
        {true, false, 0},
        {true, true, 0},
        {false, false, 1},
        {false, true, 1}};

    for (auto &[expectedResult, wa, debugKey] : testParams) {
        waTable.flags.waDisableFusedThreadScheduling = wa;
        DebugManager.flags.CFEFusedEUDispatch.set(debugKey);
        EXPECT_EQ(expectedResult, helper.isFusedEuDispatchEnabled(hardwareInfo, false));
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

    MockBuffer::setAllocationType(buffer->getGraphicsAllocation(rootDeviceIndex), context->getDevice(0)->getRootDeviceEnvironment().getGmmHelper(), true);

    EXPECT_FALSE(helper.checkResourceCompatibility(*buffer->getGraphicsAllocation(rootDeviceIndex)));
}

GEN12LPTEST_F(HwHelperTestsGen12LpBuffer, givenBufferThenCheckResourceCompatibilityReturnsTrue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    buffer.reset(Buffer::create(context.get(), 0, MemoryConstants::cacheLineSize, nullptr, retVal));

    buffer->getGraphicsAllocation(rootDeviceIndex)->setAllocationType(AllocationType::BUFFER);

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

GEN12LPTEST_F(HwHelperTestGen12Lp, givenAllocationTypeWithCpuAccessRequiredWhenCpuAccessIsDisallowedThenSystemMemoryIsRequested) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));

    const AllocationType allocationTypesToUseSystemMemory[] = {
        AllocationType::COMMAND_BUFFER,
        AllocationType::CONSTANT_SURFACE,
        AllocationType::GLOBAL_SURFACE,
        AllocationType::INTERNAL_HEAP,
        AllocationType::LINEAR_STREAM,
        AllocationType::PIPE,
        AllocationType::PRINTF_SURFACE,
        AllocationType::TIMESTAMP_PACKET_TAG_BUFFER,
        AllocationType::RING_BUFFER,
        AllocationType::SEMAPHORE_BUFFER};

    MockMemoryManager mockMemoryManager;
    for (auto allocationType : allocationTypesToUseSystemMemory) {
        AllocationData allocData{};
        AllocationProperties properties(mockRootDeviceIndex, true, 10, allocationType, false, mockDeviceBitfield);
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

        EXPECT_TRUE(allocData.flags.requiresCpuAccess);
        EXPECT_TRUE(allocData.flags.useSystemMemory);
    }

    AllocationData allocData{};
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::BUFFER, false, mockDeviceBitfield);
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.requiresCpuAccess);
    EXPECT_FALSE(allocData.flags.useSystemMemory);
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

HWTEST2_F(HwHelperTestGen12Lp, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion, IsTGLLP) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(12, 0, 0), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

HWTEST2_F(HwHelperTestGen12Lp, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion, IsRKL) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(12, 0, 0), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

HWTEST2_F(HwHelperTestGen12Lp, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion, IsADLS) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(12, 0, 0), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

GEN12LPTEST_F(HwHelperTestGen12Lp, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    cl_device_feature_capabilities_intel expectedCapabilities = CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
    EXPECT_EQ(expectedCapabilities, ClHwHelper::get(renderCoreFamily).getSupportedDeviceFeatureCapabilities(hardwareInfo));
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenLocalMemoryFeatureDisabledWhenIsLocalMemoryEnabledIsCalledThenTrueIsReturned) {
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    auto &helper = reinterpret_cast<HwHelperHw<FamilyType> &>(HwHelperHw<FamilyType>::get());
    EXPECT_TRUE(helper.isLocalMemoryEnabled(hardwareInfo));
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenLocalMemoryFeatureEnabledWhenIsLocalMemoryEnabledIsCalledThenFalseIsReturned) {
    hardwareInfo.featureTable.flags.ftrLocalMemory = false;

    auto &helper = reinterpret_cast<HwHelperHw<FamilyType> &>(HwHelperHw<FamilyType>::get());
    EXPECT_FALSE(helper.isLocalMemoryEnabled(hardwareInfo));
}
