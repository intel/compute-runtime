/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/utilities/numeric.h"

#include "opencl/source/helpers/sampler_helpers.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

using namespace NEO;

namespace NEO {
class Surface;
};

class SamplerSetArgFixture : public DeviceFixture {
  public:
    SamplerSetArgFixture()

    {
        memset(&kernelHeader, 0, sizeof(kernelHeader));
    }

  protected:
    void SetUp() {
        DeviceFixture::SetUp();
        pKernelInfo = std::make_unique<KernelInfo>();

        // define kernel info
        kernelHeader.DynamicStateHeapSize = sizeof(samplerStateHeap);
        pKernelInfo->heapInfo.pDsh = samplerStateHeap;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

        // setup kernel arg offsets
        pKernelInfo->kernelArgInfo.resize(2);
        pKernelInfo->kernelArgInfo[0].offsetHeap = 0x40;
        pKernelInfo->kernelArgInfo[0].isSampler = true;

        pKernelInfo->kernelArgInfo[0].offsetObjectId = 0x0;
        pKernelInfo->kernelArgInfo[0].offsetSamplerSnapWa = 0x4;
        pKernelInfo->kernelArgInfo[0].offsetSamplerAddressingMode = 0x8;
        pKernelInfo->kernelArgInfo[0].offsetSamplerNormalizedCoords = 0x10;

        pKernelInfo->kernelArgInfo[1].offsetHeap = 0x40;
        pKernelInfo->kernelArgInfo[1].isSampler = true;

        program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());
        pKernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);
        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        pKernel->setKernelArgHandler(0, &Kernel::setArgSampler);
        pKernel->setKernelArgHandler(1, &Kernel::setArgSampler);

        uint32_t crossThreadData[crossThreadDataSize] = {};
        pKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));
        context = new MockContext(pClDevice);
        retVal = CL_INVALID_VALUE;
    }

    void TearDown() {
        delete pKernel;

        delete sampler;
        delete context;
        DeviceFixture::TearDown();
    }

    bool crossThreadDataUnchanged() {
        for (uint32_t i = 0; i < crossThreadDataSize; i++) {
            if (pKernel->mockCrossThreadData[i] != 0u) {
                return false;
            }
        }

        return true;
    }

    void createSampler() {
        sampler = Sampler::create(
            context,
            CL_TRUE,
            CL_ADDRESS_MIRRORED_REPEAT,
            CL_FILTER_NEAREST,
            retVal);
    }

    static const uint32_t crossThreadDataSize = 0x40;

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockProgram> program;
    MockKernel *pKernel = nullptr;
    SKernelBinaryHeaderCommon kernelHeader;
    std::unique_ptr<KernelInfo> pKernelInfo;
    char samplerStateHeap[0x80];
    MockContext *context;
    Sampler *sampler = nullptr;
};

typedef Test<SamplerSetArgFixture> SamplerSetArgTest;
HWTEST_F(SamplerSetArgTest, clSetKernelArgSampler) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    createSampler();

    cl_sampler samplerObj = sampler;

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto samplerState = reinterpret_cast<const SAMPLER_STATE *>(
        ptrOffset(pKernel->getDynamicStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), static_cast<cl_bool>(!samplerState->getNonNormalizedCoordinateEnable()));
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR, samplerState->getTcxAddressControlMode());
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR, samplerState->getTcyAddressControlMode());
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR, samplerState->getTczAddressControlMode());
    EXPECT_EQ(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST, samplerState->getMinModeFilter());
    EXPECT_EQ(SAMPLER_STATE::MAG_MODE_FILTER_NEAREST, samplerState->getMagModeFilter());
    EXPECT_EQ(SAMPLER_STATE::MIP_MODE_FILTER_NEAREST, samplerState->getMipModeFilter());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(0u, surfaces.size());
}

HWTEST_F(SamplerSetArgTest, getKernelArgShouldReturnSampler) {
    createSampler();
    cl_sampler samplerObj = sampler;

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(samplerObj, pKernel->getKernelArg(0));
}

HWTEST_F(SamplerSetArgTest, GivenSamplerObjectWhenSetKernelArgIsCalledThenIncreaseSamplerRefcount) {
    cl_sampler samplerObj = Sampler::create(
        context,
        CL_TRUE,
        CL_ADDRESS_MIRRORED_REPEAT,
        CL_FILTER_NEAREST,
        retVal);

    auto pSampler = castToObject<Sampler>(samplerObj);
    auto refCountBefore = pSampler->getRefInternalCount();

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);
    auto refCountAfter = pSampler->getRefInternalCount();

    EXPECT_EQ(refCountBefore + 1, refCountAfter);

    retVal = clReleaseSampler(samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(SamplerSetArgTest, GivenSamplerObjectWhenSetKernelArgIsCalledThenSamplerObjectSurvivesClReleaseSampler) {
    cl_sampler samplerObj = Sampler::create(
        context,
        CL_TRUE,
        CL_ADDRESS_MIRRORED_REPEAT,
        CL_FILTER_NEAREST,
        retVal);

    auto pSampler = castToObject<Sampler>(samplerObj);
    auto refCountBefore = pSampler->getRefInternalCount();

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseSampler(samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto refCountAfter = pSampler->getRefInternalCount();

    EXPECT_EQ(refCountBefore, refCountAfter);
}

HWTEST_F(SamplerSetArgTest, GivenSamplerObjectWhenSetKernelArgIsCalledAndKernelIsDeletedThenRefCountIsUnchanged) {
    auto myKernel = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_NE(nullptr, myKernel.get());
    ASSERT_EQ(CL_SUCCESS, myKernel->initialize());

    myKernel->setKernelArgHandler(0, &Kernel::setArgSampler);
    myKernel->setKernelArgHandler(1, &Kernel::setArgSampler);

    uint32_t crossThreadData[crossThreadDataSize] = {};
    myKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));
    cl_sampler samplerObj = Sampler::create(
        context,
        CL_TRUE,
        CL_ADDRESS_MIRRORED_REPEAT,
        CL_FILTER_NEAREST,
        retVal);

    auto pSampler = castToObject<Sampler>(samplerObj);
    auto refCountBefore = pSampler->getRefInternalCount();

    retVal = myKernel->setArg(
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    myKernel.reset();

    auto refCountAfter = pSampler->getRefInternalCount();

    EXPECT_EQ(refCountBefore, refCountAfter);

    retVal = clReleaseSampler(samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(SamplerSetArgTest, GivenNewSamplerObjectWhensSetKernelArgIsCalledThenDecreaseOldSamplerRefcount) {
    cl_sampler samplerObj = Sampler::create(
        context,
        CL_TRUE,
        CL_ADDRESS_MIRRORED_REPEAT,
        CL_FILTER_NEAREST,
        retVal);

    auto pSampler = castToObject<Sampler>(samplerObj);

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto refCountBefore = pSampler->getRefInternalCount();

    cl_sampler samplerObj2 = Sampler::create(
        context,
        CL_TRUE,
        CL_ADDRESS_MIRRORED_REPEAT,
        CL_FILTER_NEAREST,
        retVal);

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj2),
        &samplerObj2);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto refCountAfter = pSampler->getRefInternalCount();

    EXPECT_EQ(refCountBefore - 1, refCountAfter);

    retVal = clReleaseSampler(samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseSampler(samplerObj2);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(SamplerSetArgTest, GivenIncorrentSamplerObjectWhenSetKernelArgSamplerIsCalledThenLeaveRefcountAsIs) {
    auto notSamplerObj = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(context));

    auto pNotSampler = castToObject<Image>(notSamplerObj.get());
    auto refCountBefore = pNotSampler->getRefInternalCount();

    retVal = pKernel->setArgSampler(
        0,
        sizeof(notSamplerObj.get()),
        notSamplerObj.get());
    auto refCountAfter = pNotSampler->getRefInternalCount();

    EXPECT_EQ(refCountBefore, refCountAfter);
}

HWTEST_F(SamplerSetArgTest, WithFilteringNearestAndAddressingClClampSetAsKernelArgumentSetsConstantBuffer) {

    sampler = Sampler::create(
        context,
        CL_TRUE,
        CL_ADDRESS_CLAMP,
        CL_FILTER_NEAREST,
        retVal);

    cl_sampler samplerObj = sampler;

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(samplerObj, pKernel->getKernelArg(0));

    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto snapWaCrossThreadData = ptrOffset(crossThreadData, 0x4);

    unsigned int snapWaValue = 0xffffffff;
    unsigned int objectId = SAMPLER_OBJECT_ID_SHIFT + pKernelInfo->kernelArgInfo[0].offsetHeap;

    EXPECT_EQ(snapWaValue, *snapWaCrossThreadData);
    EXPECT_EQ(objectId, *crossThreadData);
}

HWTEST_F(SamplerSetArgTest, GIVENkernelWithoutObjIdOffsetWHENsetArgTHENdoesntPatchObjId) {

    sampler = Sampler::create(
        context,
        CL_TRUE,
        CL_ADDRESS_CLAMP,
        CL_FILTER_NEAREST,
        retVal);

    cl_sampler samplerObj = sampler;

    retVal = pKernel->setArg(
        1,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(samplerObj, pKernel->getKernelArg(1));
    EXPECT_TRUE(crossThreadDataUnchanged());
}

HWTEST_F(SamplerSetArgTest, setKernelArgWithNullptrSampler) {
    createSampler();
    cl_sampler samplerObj = sampler;

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        nullptr);
    ASSERT_EQ(CL_INVALID_SAMPLER, retVal);
}

HWTEST_F(SamplerSetArgTest, setKernelArgWithInvalidSampler) {
    createSampler();
    cl_sampler samplerObj = sampler;

    const void *notASampler = reinterpret_cast<const void *>(pKernel);

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        notASampler);
    ASSERT_EQ(CL_INVALID_SAMPLER, retVal);
}

TEST_F(SamplerSetArgTest, givenSamplerTypeStrAndIsSamplerTrueWhenInitializeKernelThenKernelArgumentsTypeIsSamplerObj) {
    pKernelInfo->kernelArgInfo.resize(2);
    pKernelInfo->kernelArgInfo[0].metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    pKernelInfo->kernelArgInfo[0].metadataExtended->type = "sampler*";
    pKernelInfo->kernelArgInfo[0].isSampler = true;
    pKernelInfo->kernelArgInfo[1].metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    pKernelInfo->kernelArgInfo[1].metadataExtended->type = "sampler";
    pKernelInfo->kernelArgInfo[1].isSampler = true;

    auto pMockKernell = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, pMockKernell->initialize());
    EXPECT_EQ(pMockKernell->getKernelArguments()[0].type, MockKernel::SAMPLER_OBJ);
    EXPECT_EQ(pMockKernell->getKernelArguments()[1].type, MockKernel::SAMPLER_OBJ);
}

TEST_F(SamplerSetArgTest, givenSamplerTypeStrAndAndIsSamplerFalseWhenInitializeKernelThenKernelArgumentsTypeIsNotSamplerObj) {
    pKernelInfo->kernelArgInfo.resize(2);
    pKernelInfo->kernelArgInfo[0].metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    pKernelInfo->kernelArgInfo[0].metadataExtended->type = "sampler*";
    pKernelInfo->kernelArgInfo[0].isSampler = false;
    pKernelInfo->kernelArgInfo[1].metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    pKernelInfo->kernelArgInfo[1].metadataExtended->type = "sampler";
    pKernelInfo->kernelArgInfo[1].isSampler = false;

    auto pMockKernell = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, pMockKernell->initialize());
    EXPECT_NE(pMockKernell->getKernelArguments()[0].type, MockKernel::SAMPLER_OBJ);
    EXPECT_NE(pMockKernell->getKernelArguments()[1].type, MockKernel::SAMPLER_OBJ);
}

////////////////////////////////////////////////////////////////////////////////
struct NormalizedTest
    : public SamplerSetArgFixture,
      public ::testing::TestWithParam<uint32_t /*cl_bool*/> {
    void SetUp() override {
        SamplerSetArgFixture::SetUp();
    }
    void TearDown() override {
        SamplerSetArgFixture::TearDown();
    }
};

HWTEST_P(NormalizedTest, setKernelArgSampler) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto normalizedCoordinates = GetParam();
    sampler = Sampler::create(
        context,
        normalizedCoordinates,
        CL_ADDRESS_MIRRORED_REPEAT,
        CL_FILTER_NEAREST,
        retVal);

    cl_sampler samplerObj = sampler;

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto samplerState = reinterpret_cast<const SAMPLER_STATE *>(
        ptrOffset(pKernel->getDynamicStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    EXPECT_EQ(normalizedCoordinates, static_cast<cl_bool>(!samplerState->getNonNormalizedCoordinateEnable()));

    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto normalizedCoordsAddress = ptrOffset(crossThreadData, 0x10);
    unsigned int normalizedCoordsValue = GetNormCoordsEnum(normalizedCoordinates);

    EXPECT_EQ(normalizedCoordsValue, *normalizedCoordsAddress);
}

cl_bool normalizedCoordinatesCases[] = {
    CL_FALSE,
    CL_TRUE};

INSTANTIATE_TEST_CASE_P(SamplerSetArg,
                        NormalizedTest,
                        ::testing::ValuesIn(normalizedCoordinatesCases));

////////////////////////////////////////////////////////////////////////////////
struct AddressingModeTest
    : public SamplerSetArgFixture,
      public ::testing::TestWithParam<uint32_t /*cl_addressing_mode*/> {
    void SetUp() override {
        SamplerSetArgFixture::SetUp();
    }
    void TearDown() override {
        SamplerSetArgFixture::TearDown();
    }
};

HWTEST_P(AddressingModeTest, setKernelArgSampler) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto addressingMode = GetParam();
    sampler = Sampler::create(
        context,
        CL_TRUE,
        addressingMode,
        CL_FILTER_NEAREST,
        retVal);

    cl_sampler samplerObj = sampler;

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto samplerState = reinterpret_cast<const SAMPLER_STATE *>(
        ptrOffset(pKernel->getDynamicStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    auto expectedModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
    auto expectedModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
    auto expectedModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;

    // clang-format off
    switch (addressingMode) {
    case CL_ADDRESS_NONE:
    case CL_ADDRESS_CLAMP:
        expectedModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER;
        expectedModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER;
        expectedModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER;
        break;
    case CL_ADDRESS_CLAMP_TO_EDGE:
        expectedModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
        expectedModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
        expectedModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
        break;
    case CL_ADDRESS_MIRRORED_REPEAT:
        expectedModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
        expectedModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
        expectedModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
        break;
    case CL_ADDRESS_REPEAT:
        expectedModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP;
        expectedModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP;
        expectedModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP;
        break;
    }
    // clang-format on

    EXPECT_EQ(expectedModeX, samplerState->getTcxAddressControlMode());
    EXPECT_EQ(expectedModeY, samplerState->getTcyAddressControlMode());
    EXPECT_EQ(expectedModeZ, samplerState->getTczAddressControlMode());

    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto addressingModeAddress = ptrOffset(crossThreadData, 0x8);

    unsigned int addresingValue = GetAddrModeEnum(addressingMode);

    EXPECT_EQ(addresingValue, *addressingModeAddress);
}

cl_addressing_mode addressingModeCases[] = {
    CL_ADDRESS_NONE,
    CL_ADDRESS_CLAMP_TO_EDGE,
    CL_ADDRESS_CLAMP,
    CL_ADDRESS_REPEAT,
    CL_ADDRESS_MIRRORED_REPEAT};

INSTANTIATE_TEST_CASE_P(SamplerSetArg,
                        AddressingModeTest,
                        ::testing::ValuesIn(addressingModeCases));

HWTEST_F(SamplerSetArgTest, setKernelArgSamplerWithMipMaps) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;

    FixedU4D8 minLod = 2.0f;
    FixedU4D8 maxLod = 3.0f;

    sampler = Sampler::create(
        context,
        CL_TRUE,
        CL_ADDRESS_NONE,
        CL_FILTER_NEAREST,
        CL_FILTER_LINEAR,
        minLod.asFloat(), maxLod.asFloat(),
        retVal);

    cl_sampler samplerObj = sampler;

    retVal = pKernel->setArg(
        0,
        sizeof(samplerObj),
        &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto samplerState = reinterpret_cast<const SAMPLER_STATE *>(
        ptrOffset(pKernel->getDynamicStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    EXPECT_EQ(FamilyType::SAMPLER_STATE::MIP_MODE_FILTER_LINEAR, samplerState->getMipModeFilter());
    EXPECT_EQ(minLod.getRawAccess(), samplerState->getMinLod());
    EXPECT_EQ(maxLod.getRawAccess(), samplerState->getMaxLod());
}

////////////////////////////////////////////////////////////////////////////////
struct FilterModeTest
    : public SamplerSetArgFixture,
      public ::testing::TestWithParam<uint32_t /*cl_filter_mode*/> {
    void SetUp() override {
        SamplerSetArgFixture::SetUp();
    }
    void TearDown() override {
        SamplerSetArgFixture::TearDown();
    }
};

HWTEST_P(FilterModeTest, setKernelArgSampler) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto filterMode = GetParam();
    sampler = Sampler::create(
        context,
        CL_TRUE,
        CL_ADDRESS_MIRRORED_REPEAT,
        filterMode,
        retVal);

    auto samplerState = reinterpret_cast<const SAMPLER_STATE *>(
        ptrOffset(pKernel->getDynamicStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    sampler->setArg(const_cast<SAMPLER_STATE *>(samplerState));

    if (CL_FILTER_NEAREST == filterMode) {
        EXPECT_EQ(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST, samplerState->getMinModeFilter());
        EXPECT_EQ(SAMPLER_STATE::MAG_MODE_FILTER_NEAREST, samplerState->getMagModeFilter());
        EXPECT_EQ(SAMPLER_STATE::MIP_MODE_FILTER_NEAREST, samplerState->getMipModeFilter());
        EXPECT_FALSE(samplerState->getUAddressMinFilterRoundingEnable());
        EXPECT_FALSE(samplerState->getUAddressMagFilterRoundingEnable());
        EXPECT_FALSE(samplerState->getVAddressMinFilterRoundingEnable());
        EXPECT_FALSE(samplerState->getVAddressMagFilterRoundingEnable());
        EXPECT_FALSE(samplerState->getRAddressMagFilterRoundingEnable());
        EXPECT_FALSE(samplerState->getRAddressMinFilterRoundingEnable());
    } else {
        EXPECT_EQ(SAMPLER_STATE::MIN_MODE_FILTER_LINEAR, samplerState->getMinModeFilter());
        EXPECT_EQ(SAMPLER_STATE::MAG_MODE_FILTER_LINEAR, samplerState->getMagModeFilter());
        EXPECT_EQ(SAMPLER_STATE::MIP_MODE_FILTER_NEAREST, samplerState->getMipModeFilter());
        EXPECT_TRUE(samplerState->getUAddressMinFilterRoundingEnable());
        EXPECT_TRUE(samplerState->getUAddressMagFilterRoundingEnable());
        EXPECT_TRUE(samplerState->getVAddressMinFilterRoundingEnable());
        EXPECT_TRUE(samplerState->getVAddressMagFilterRoundingEnable());
        EXPECT_TRUE(samplerState->getRAddressMagFilterRoundingEnable());
        EXPECT_TRUE(samplerState->getRAddressMinFilterRoundingEnable());
    }
}

cl_filter_mode filterModeCase[] = {
    CL_FILTER_NEAREST,
    CL_FILTER_LINEAR};

INSTANTIATE_TEST_CASE_P(SamplerSetArg,
                        FilterModeTest,
                        ::testing::ValuesIn(filterModeCase));
