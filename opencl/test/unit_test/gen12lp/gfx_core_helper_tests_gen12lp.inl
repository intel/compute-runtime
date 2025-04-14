/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_gfx_core_helper.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "aubstream/engine_node.h"

using ClGfxCoreHelperTestsGen12Lp = Test<ClDeviceFixture>;

GEN12LPTEST_F(ClGfxCoreHelperTestsGen12Lp, givenTglLpThenAuxTranslationIsRequired) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (auto accessedUsingStatelessAddressingMode : {true, false}) {
        KernelInfo kernelInfo{};

        ArgDescriptor arg;
        arg.as<ArgDescPointer>(true).accessedUsingStatelessAddressingMode = accessedUsingStatelessAddressingMode;
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(std::move(arg));

        EXPECT_EQ(accessedUsingStatelessAddressingMode, clGfxCoreHelper.requiresAuxResolves(kernelInfo));
    }
}

GEN12LPTEST_F(ClGfxCoreHelperTestsGen12Lp, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    cl_device_feature_capabilities_intel expectedCapabilities = CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
    EXPECT_EQ(expectedCapabilities, clGfxCoreHelper.getSupportedDeviceFeatureCapabilities(getRootDeviceEnvironment()));
}

using GfxCoreHelperTestGen12Lp = GfxCoreHelperTest;

struct IsGen12LPIntegrated {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsGen12LP::isMatched<productFamily>() && !IsDG1::isMatched<productFamily>();
    }
};

HWTEST2_F(GfxCoreHelperTestGen12Lp, givenFtrCcsNodeNotSetAndBcsInfoSetWhenGetGpgpuEnginesThenReturnThreeRcsEnginesAndOneBcsEngine, IsGen12LPIntegrated) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(4u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[3].first);
}

HWTEST2_F(GfxCoreHelperTestGen12Lp, givenFtrCcsNodeNotSetAndBcsInfoSetWhenGetGpgpuEnginesThenReturnThreeRcsEnginesAndOneBcsEngine, IsDG1) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(5u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(5u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[4].first);
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, WhenGettingMaxBarriersPerSliceThenCorrectSizeIsReturned) {
    auto &helper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, WhenGettingPitchAlignmentForImageThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto stepping = hardwareInfo.platform.usRevId;

    if (SpecialUltHelperGen12lp::shouldPerformimagePitchAlignment(hardwareInfo.platform.eProductFamily) && stepping == 0) {
        EXPECT_EQ(64u, gfxCoreHelper.getPitchAlignmentForImage(pDevice->getRootDeviceEnvironment()));
    } else {
        EXPECT_EQ(4u, gfxCoreHelper.getPitchAlignmentForImage(pDevice->getRootDeviceEnvironment()));
    }
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, WhenAdjustingDefaultEngineTypeThenRcsIsSet) {
    hardwareInfo.featureTable.flags.ftrCCSNode = false;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo, productHelper, nullptr);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenDifferentSizesOfAllocationWhenCheckingCompressionPreferenceThenReturnCorrectValue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    const size_t sizesToCheck[] = {128, 256, 512, 1023, 1024, 1025};
    for (size_t size : sizesToCheck) {
        EXPECT_FALSE(gfxCoreHelper.isBufferSizeSuitableForCompression(size));
    }
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenFtrCcsNodeNotSetAndCcsDefualtEngineWhenGetGpgpuEnginesThenReturnTwoRcsEnginesAndOneCcs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(3u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[2].first);
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenFtrCcsNodeNotSetWhenGetGpgpuEnginesThenReturnThreeRcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());

    EXPECT_EQ(3u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenFtrCcsNodeSetWhenGetGpgpuEnginesThenReturnTwoRcsAndCcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(4u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[3].first);
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenFtrCcsNodeSetWhenGetGpgpuEnginesThenReturn2RcsAnd2CcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(4u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(4u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[3].first);
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenFtrCcsNodeSetAndDefaultRcsWhenGetGpgpuEnginesThenReturnAppropriateNumberOfRcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(3u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenTgllpWhenIsFusedEuDispatchEnabledIsCalledThenResultIsCorrect) {
    DebugManagerStateRestore restorer;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
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
        debugManager.flags.CFEFusedEUDispatch.set(debugKey);
        EXPECT_EQ(expectedResult, gfxCoreHelper.isFusedEuDispatchEnabled(hardwareInfo, false));
    }
}

class GfxCoreHelperTestsGen12LpBuffer : public ::testing::Test {
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

GEN12LPTEST_F(GfxCoreHelperTestsGen12LpBuffer, givenCompressedBufferThenCheckResourceCompatibilityReturnsFalse) {
    auto &gfxCoreHelper = device->getGfxCoreHelper();

    buffer.reset(Buffer::create(context.get(), 0, MemoryConstants::cacheLineSize, nullptr, retVal));

    MockBuffer::setAllocationType(buffer->getGraphicsAllocation(rootDeviceIndex), context->getDevice(0)->getRootDeviceEnvironment().getGmmHelper(), true);

    EXPECT_FALSE(gfxCoreHelper.checkResourceCompatibility(*buffer->getGraphicsAllocation(rootDeviceIndex)));
}

GEN12LPTEST_F(GfxCoreHelperTestsGen12LpBuffer, givenBufferThenCheckResourceCompatibilityReturnsTrue) {
    auto &gfxCoreHelper = device->getGfxCoreHelper();

    buffer.reset(Buffer::create(context.get(), 0, MemoryConstants::cacheLineSize, nullptr, retVal));

    buffer->getGraphicsAllocation(rootDeviceIndex)->setAllocationType(AllocationType::buffer);

    EXPECT_TRUE(gfxCoreHelper.checkResourceCompatibility(*buffer->getGraphicsAllocation(rootDeviceIndex)));
}

using LriHelperTestsGen12Lp = ::testing::Test;

GEN12LPTEST_F(LriHelperTestsGen12Lp, whenProgrammingLriCommandThenExpectMmioRemapEnable) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    EXPECT_TRUE(expectedLri.getMmioRemapEnable());
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);
    expectedLri.setMmioRemapEnable(false);

    LriHelper<FamilyType>::program(&stream, address, data, false, false);
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

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenGen12WhenCallIsPackedSupportedThenReturnTrue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.packedFormatsSupported());
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, whenRequestingMocsThenProperMocsIndicesAreBeingReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmmHelper = this->pDevice->getGmmHelper();

    const auto mocsNoCache = gmmHelper->getUncachedMOCS() >> 1;
    const auto mocsL3 = gmmHelper->getL3EnabledMOCS() >> 1;

    EXPECT_EQ(mocsNoCache, gfxCoreHelper.getMocsIndex(*gmmHelper, false, false));
    EXPECT_EQ(mocsNoCache, gfxCoreHelper.getMocsIndex(*gmmHelper, false, true));
    EXPECT_EQ(mocsL3, gfxCoreHelper.getMocsIndex(*gmmHelper, true, false));
    EXPECT_EQ(mocsL3, gfxCoreHelper.getMocsIndex(*gmmHelper, true, true));
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenL1ForceEnabledWhenRequestingMocsThenProperMocsIndicesAreBeingReturned) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForceL1Caching.set(1);

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmmHelper = this->pDevice->getGmmHelper();

    const auto mocsNoCache = gmmHelper->getUncachedMOCS() >> 1;
    const auto mocsL3 = gmmHelper->getL3EnabledMOCS() >> 1;
    const auto mocsL1 = gmmHelper->getL1EnabledMOCS() >> 1;

    EXPECT_EQ(mocsNoCache, gfxCoreHelper.getMocsIndex(*gmmHelper, false, false));
    EXPECT_EQ(mocsNoCache, gfxCoreHelper.getMocsIndex(*gmmHelper, false, true));
    EXPECT_EQ(mocsL3, gfxCoreHelper.getMocsIndex(*gmmHelper, true, false));
    EXPECT_EQ(mocsL1, gfxCoreHelper.getMocsIndex(*gmmHelper, true, true));
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenL1ForceDisabledWhenRequestingMocsThenProperMocsIndicesAreBeingReturned) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForceL1Caching.set(0);

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmmHelper = this->pDevice->getGmmHelper();

    const auto mocsNoCache = gmmHelper->getUncachedMOCS() >> 1;
    const auto mocsL3 = gmmHelper->getL3EnabledMOCS() >> 1;

    EXPECT_EQ(mocsNoCache, gfxCoreHelper.getMocsIndex(*gmmHelper, false, false));
    EXPECT_EQ(mocsNoCache, gfxCoreHelper.getMocsIndex(*gmmHelper, false, true));
    EXPECT_EQ(mocsL3, gfxCoreHelper.getMocsIndex(*gmmHelper, true, false));
    EXPECT_EQ(mocsL3, gfxCoreHelper.getMocsIndex(*gmmHelper, true, true));
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenAllocationTypeWithCpuAccessRequiredWhenCpuAccessIsDisallowedThenSystemMemoryIsRequested) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::cpuAccessDisallowed));

    const AllocationType allocationTypesToUseSystemMemory[] = {
        AllocationType::commandBuffer,
        AllocationType::constantSurface,
        AllocationType::globalSurface,
        AllocationType::internalHeap,
        AllocationType::linearStream,
        AllocationType::pipe,
        AllocationType::printfSurface,
        AllocationType::timestampPacketTagBuffer,
        AllocationType::ringBuffer,
        AllocationType::semaphoreBuffer};

    MockMemoryManager mockMemoryManager;
    for (auto allocationType : allocationTypesToUseSystemMemory) {
        AllocationData allocData{};
        AllocationProperties properties(mockRootDeviceIndex, true, 10, allocationType, false, mockDeviceBitfield);
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

        EXPECT_TRUE(allocData.flags.requiresCpuAccess);
        EXPECT_TRUE(allocData.flags.useSystemMemory);
    }

    AllocationData allocData{};
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::buffer, false, mockDeviceBitfield);
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.requiresCpuAccess);
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

HWTEST2_F(GfxCoreHelperTestGen12Lp, givenRevisionEnumThenProperValueForIsWorkaroundRequiredIsReturned, IsRKL) {
    using us = unsigned short;
    constexpr us a0 = 0x0;
    constexpr us b0 = 0x4;
    constexpr us undefined = 0x5;
    us steppings[] = {a0, b0, undefined};
    HardwareInfo hardwareInfo = *defaultHwInfo;

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = stepping;
        auto &productHelper = getHelper<ProductHelper>();
        if (stepping == a0) {
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));
        } else if (stepping == b0 || stepping == undefined) {
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
        }
    }
}

HWTEST2_F(GfxCoreHelperTestGen12Lp, givenRevisionEnumThenProperValueForIsWorkaroundRequiredIsReturned, IsADLS) {
    using us = unsigned short;
    constexpr us a0 = 0x0;
    constexpr us b0 = 0x4;
    constexpr us undefined = 0x5;
    us steppings[] = {a0, b0, undefined};
    HardwareInfo hardwareInfo = *defaultHwInfo;

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = stepping;
        auto &productHelper = getHelper<ProductHelper>();

        if (stepping == a0) {
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));
        } else if (stepping == b0 || stepping == undefined) {
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));
        }
    }
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenLocalMemoryFeatureDisabledWhenIsLocalMemoryEnabledIsCalledThenTrueIsReturned) {
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isLocalMemoryEnabled(hardwareInfo));
}

GEN12LPTEST_F(GfxCoreHelperTestGen12Lp, givenLocalMemoryFeatureEnabledWhenIsLocalMemoryEnabledIsCalledThenFalseIsReturned) {
    hardwareInfo.featureTable.flags.ftrLocalMemory = false;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isLocalMemoryEnabled(hardwareInfo));
}
