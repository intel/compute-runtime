/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/gen12lp/special_ult_helper_gen12lp.h"
#include "unit_tests/helpers/hw_helper_tests.h"
#include "unit_tests/mocks/mock_context.h"

#include "engine_node.h"

using HwHelperTestGen12Lp = HwHelperTest;

GEN12LPTEST_F(HwHelperTestGen12Lp, givenTglLpThenAuxTranslationIsRequired) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(helper.requiresAuxResolves());
}

GEN12LPTEST_F(HwHelperTestGen12Lp, getMaxBarriersPerSliceReturnsCorrectSize) {
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

GEN12LPTEST_F(HwHelperTestGen12Lp, getPitchAlignmentForImage) {
    auto &helper = HwHelper::get(renderCoreFamily);
    auto stepping = hardwareInfo.platform.usRevId;

    if (SpecialUltHelperGen12lp::shouldPerformimagePitchAlignment(hardwareInfo.platform.eProductFamily) && stepping == 0) {
        EXPECT_EQ(64u, helper.getPitchAlignmentForImage(&hardwareInfo));
    } else {
        EXPECT_EQ(4u, helper.getPitchAlignmentForImage(&hardwareInfo));
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, adjustDefaultEngineTypeNoCcs) {
    hardwareInfo.featureTable.ftrCCSNode = false;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, adjustDefaultEngineTypeCcs) {
    hardwareInfo.featureTable.ftrCCSNode = true;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_CCS, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenGen12LpPlatformWhenSetupHardwareCapabilitiesIsCalledThenDefaultImplementationIsUsed) {
    if (SpecialUltHelperGen12lp::shouldTestDefaultImplementationOfSetupHardwareCapabilities(hardwareInfo.platform.eProductFamily)) {
        auto &helper = HwHelper::get(renderCoreFamily);
        testDefaultImplementationOfSetupHardwareCapabilities(helper, hardwareInfo);
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, whenGetConfigureAddressSpaceModeThenReturnOne) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(1u, helper.getConfigureAddressSpaceMode());
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenCompressionFtrEnabledWhenAskingForPageTableManagerThenReturnCorrectValue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    hardwareInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    hardwareInfo.capabilityTable.ftrRenderCompressedImages = false;
    bool expectedPageTableManagerSupport = SpecialUltHelperGen12lp::isPageTableManagerSupported(hardwareInfo);
    EXPECT_EQ(expectedPageTableManagerSupport, helper.isPageTableManagerSupported(hardwareInfo));

    hardwareInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    hardwareInfo.capabilityTable.ftrRenderCompressedImages = false;
    expectedPageTableManagerSupport = SpecialUltHelperGen12lp::isPageTableManagerSupported(hardwareInfo);
    EXPECT_EQ(expectedPageTableManagerSupport, helper.isPageTableManagerSupported(hardwareInfo));

    hardwareInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    hardwareInfo.capabilityTable.ftrRenderCompressedImages = true;
    expectedPageTableManagerSupport = SpecialUltHelperGen12lp::isPageTableManagerSupported(hardwareInfo);
    EXPECT_EQ(expectedPageTableManagerSupport, helper.isPageTableManagerSupported(hardwareInfo));

    hardwareInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    hardwareInfo.capabilityTable.ftrRenderCompressedImages = true;
    expectedPageTableManagerSupport = SpecialUltHelperGen12lp::isPageTableManagerSupported(hardwareInfo);
    EXPECT_EQ(expectedPageTableManagerSupport, helper.isPageTableManagerSupported(hardwareInfo));
}

GEN12LPTEST_F(HwHelperTestGen12Lp, givenDifferentSizesOfAllocationWhenCheckingCompressionPreferenceThenReturnCorrectValue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    const size_t sizesToCheck[] = {128, 256, 512, 1023, 1024, 1025};
    for (size_t size : sizesToCheck) {
        EXPECT_EQ(SpecialUltHelperGen12lp::isRenderBufferCompressionPreferred(hardwareInfo, size),
                  helper.obtainRenderBufferCompressionPreference(hardwareInfo, size));
    }
}

GEN12LPTEST_F(HwHelperTestGen12Lp, whenGetGpgpuEnginesThenReturnTwoRcsEnginesAndOneCcsEngine) {
    EXPECT_EQ(3u, pDevice->engines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances();
    EXPECT_EQ(3u, engines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0]);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1]);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[2]);
}

class HwHelperTestsGen12LpBuffer : public ::testing::Test {
  public:
    void SetUp() override {
        ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
        device.reset(Device::create<MockDevice>(executionEnvironment, 0u));
        context = std::make_unique<MockContext>(device.get(), true);
        context->contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;
};

GEN12LPTEST_F(HwHelperTestsGen12LpBuffer, givenCompressedBufferThenCheckResourceCompatibilityReturnsFalse) {
    auto &helper = HwHelper::get(renderCoreFamily);

    buffer.reset(Buffer::create(context.get(), 0, MemoryConstants::cacheLineSize, nullptr, retVal));

    buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);

    EXPECT_FALSE(helper.checkResourceCompatibility(*buffer->getGraphicsAllocation()));
}

GEN12LPTEST_F(HwHelperTestsGen12LpBuffer, givenBufferThenCheckResourceCompatibilityReturnsTrue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    buffer.reset(Buffer::create(context.get(), 0, MemoryConstants::cacheLineSize, nullptr, retVal));

    buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);

    EXPECT_TRUE(helper.checkResourceCompatibility(*buffer->getGraphicsAllocation()));
}

using LriHelperTestsGen12Lp = ::testing::Test;

GEN12LPTEST_F(LriHelperTestsGen12Lp, whenProgrammingLriCommandThenExpectMmioRemapEnable) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);
    expectedLri.setMmioRemapEnable(true);

    auto lri = LriHelper<FamilyType>::program(&stream, address, data);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(lri, stream.getCpuBase());
    EXPECT_TRUE(memcmp(lri, &expectedLri, sizeof(MI_LOAD_REGISTER_IMM)) == 0);
}
