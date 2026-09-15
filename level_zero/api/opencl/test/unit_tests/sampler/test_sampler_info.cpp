/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sampler/leo_sampler.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/sampler/sampler.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <map>
#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct MockL0Sampler : public L0::Sampler {
    MockL0Sampler(ze_sampler_address_mode_t addressMode, ze_sampler_filter_mode_t filterMode, ze_bool_t isNormalized) {
        this->samplerDesc.stype = ZE_STRUCTURE_TYPE_SAMPLER_DESC;
        this->samplerDesc.addressMode = addressMode;
        this->samplerDesc.filterMode = filterMode;
        this->samplerDesc.isNormalized = isNormalized;
    }

    ze_result_t destroy() override {
        destroyCalled++;
        return ZE_RESULT_SUCCESS;
    }

    void copySamplerStateToDSH(ArrayRef<uint8_t>, const uint32_t) override {}

    uint32_t destroyCalled = 0u;
};

struct SamplerInfoFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        sampler.reset();
        l0Samplers.clear();
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    MockL0Sampler *addL0Sampler(ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_NONE,
                                ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_NEAREST,
                                ze_bool_t isNormalized = false) {
        l0Samplers.push_back(std::make_unique<MockL0Sampler>(addressMode, filterMode, isNormalized));
        return l0Samplers.back().get();
    }

    Sampler *createSampler(ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_NONE,
                           ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_NEAREST,
                           ze_bool_t isNormalized = false,
                           const cl_sampler_properties *properties = nullptr) {
        std::map<uint32_t, ze_sampler_handle_t> handles{};
        handles[clDevice->getRootDeviceIndex()] = addL0Sampler(addressMode, filterMode, isNormalized)->toHandle();
        sampler = std::make_unique<Sampler>(leoContext.get(), std::move(handles), properties);
        return sampler.get();
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
    std::vector<std::unique_ptr<MockL0Sampler>> l0Samplers{};
    std::unique_ptr<Sampler> sampler{};
};

TEST_F(SamplerInfoFixture, givenSamplerWhenQueryingContextThenOwningContextIsReturned) {
    auto sampler = createSampler();

    cl_context queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_CONTEXT, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(cl_context), retSize);
    EXPECT_EQ(static_cast<cl_context>(leoContext.get()), queried);
}

TEST_F(SamplerInfoFixture, givenNormalizedSamplerWhenQueryingNormalizedCoordsThenReturnsTrue) {
    auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_NONE, ZE_SAMPLER_FILTER_MODE_NEAREST, true);

    cl_bool normalized = CL_FALSE;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_NORMALIZED_COORDS, sizeof(normalized), &normalized, &retSize));
    EXPECT_EQ(sizeof(cl_bool), retSize);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), normalized);
}

TEST_F(SamplerInfoFixture, givenNonNormalizedSamplerWhenQueryingNormalizedCoordsThenReturnsFalse) {
    auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_NONE, ZE_SAMPLER_FILTER_MODE_NEAREST, false);

    cl_bool normalized = CL_TRUE;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_NORMALIZED_COORDS, sizeof(normalized), &normalized, nullptr));
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), normalized);
}

TEST_F(SamplerInfoFixture, givenEveryL0AddressModeWhenQueryingAddressingModeThenItIsMappedToItsClCounterpart) {
    const std::pair<ze_sampler_address_mode_t, cl_addressing_mode> expectations[] = {
        {ZE_SAMPLER_ADDRESS_MODE_NONE, CL_ADDRESS_NONE},
        {ZE_SAMPLER_ADDRESS_MODE_CLAMP, CL_ADDRESS_CLAMP_TO_EDGE},
        {ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, CL_ADDRESS_CLAMP},
        {ZE_SAMPLER_ADDRESS_MODE_REPEAT, CL_ADDRESS_REPEAT},
        {ZE_SAMPLER_ADDRESS_MODE_MIRROR, CL_ADDRESS_MIRRORED_REPEAT}};

    for (const auto &[l0Mode, expectedClMode] : expectations) {
        auto sampler = createSampler(l0Mode);

        cl_addressing_mode queried = 0;
        size_t retSize = 0;
        EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_ADDRESSING_MODE, sizeof(queried), &queried, &retSize));
        EXPECT_EQ(sizeof(cl_addressing_mode), retSize);
        EXPECT_EQ(expectedClMode, queried) << "l0 address mode " << static_cast<uint32_t>(l0Mode);
    }
}

TEST_F(SamplerInfoFixture, givenUnknownL0AddressModeWhenQueryingAddressingModeThenNoModeIsReported) {
    auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_FORCE_UINT32);

    cl_addressing_mode queried = CL_ADDRESS_REPEAT;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_ADDRESSING_MODE, sizeof(queried), &queried, nullptr));
    EXPECT_EQ(0u, queried);
}

TEST_F(SamplerInfoFixture, givenEveryL0FilterModeWhenQueryingFilterModeThenItIsMappedToItsClCounterpart) {
    const std::pair<ze_sampler_filter_mode_t, cl_filter_mode> expectations[] = {
        {ZE_SAMPLER_FILTER_MODE_NEAREST, CL_FILTER_NEAREST},
        {ZE_SAMPLER_FILTER_MODE_LINEAR, CL_FILTER_LINEAR}};

    for (const auto &[l0Mode, expectedClMode] : expectations) {
        auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_NONE, l0Mode);

        cl_filter_mode queried = 0;
        size_t retSize = 0;
        EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_FILTER_MODE, sizeof(queried), &queried, &retSize));
        EXPECT_EQ(sizeof(cl_filter_mode), retSize);
        EXPECT_EQ(expectedClMode, queried) << "l0 filter mode " << static_cast<uint32_t>(l0Mode);
    }
}

TEST_F(SamplerInfoFixture, givenUnknownL0FilterModeWhenQueryingFilterModeThenNoModeIsReported) {
    auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_NONE, ZE_SAMPLER_FILTER_MODE_FORCE_UINT32);

    cl_filter_mode queried = CL_FILTER_LINEAR;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_FILTER_MODE, sizeof(queried), &queried, nullptr));
    EXPECT_EQ(0u, queried);
}

TEST_F(SamplerInfoFixture, givenNewSamplerWhenQueryingReferenceCountThenReturnsOne) {
    auto sampler = createSampler();

    cl_uint refCount = 0;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_REFERENCE_COUNT, sizeof(refCount), &refCount, &retSize));
    EXPECT_EQ(sizeof(cl_uint), retSize);
    EXPECT_EQ(1u, refCount);
}

TEST_F(SamplerInfoFixture, givenRetainedSamplerWhenQueryingReferenceCountThenItReflectsTheRetain) {
    auto sampler = createSampler();
    sampler->retain();

    cl_uint refCount = 0;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_REFERENCE_COUNT, sizeof(refCount), &refCount, nullptr));
    EXPECT_EQ(2u, refCount);

    sampler->release();
}

TEST_F(SamplerInfoFixture, givenNoPropertiesWhenQueryingPropertiesThenNothingIsReturned) {
    auto sampler = createSampler();

    size_t retSize = 123u;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_PROPERTIES, 0, nullptr, &retSize));
    EXPECT_EQ(0u, retSize);
}

TEST_F(SamplerInfoFixture, givenPropertiesWhenQueryingPropertiesThenTheyAreReturnedNullTerminated) {
    cl_sampler_properties properties[] = {CL_SAMPLER_NORMALIZED_COORDS, CL_TRUE,
                                          CL_SAMPLER_ADDRESSING_MODE, CL_ADDRESS_REPEAT,
                                          0};
    auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_REPEAT, ZE_SAMPLER_FILTER_MODE_NEAREST, true, properties);

    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_PROPERTIES, 0, nullptr, &retSize));
    ASSERT_EQ(5u * sizeof(cl_sampler_properties), retSize);

    std::vector<cl_sampler_properties> stored(retSize / sizeof(cl_sampler_properties));
    ASSERT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_PROPERTIES, retSize, stored.data(), nullptr));
    EXPECT_EQ(static_cast<cl_sampler_properties>(CL_SAMPLER_NORMALIZED_COORDS), stored[0]);
    EXPECT_EQ(static_cast<cl_sampler_properties>(CL_TRUE), stored[1]);
    EXPECT_EQ(static_cast<cl_sampler_properties>(CL_SAMPLER_ADDRESSING_MODE), stored[2]);
    EXPECT_EQ(static_cast<cl_sampler_properties>(CL_ADDRESS_REPEAT), stored[3]);
    EXPECT_EQ(static_cast<cl_sampler_properties>(0), stored[4]);
}

TEST_F(SamplerInfoFixture, givenPropertiesWhenResettingThemThenQueryingPropertiesReturnsNothing) {
    cl_sampler_properties properties[] = {CL_SAMPLER_FILTER_MODE, CL_FILTER_LINEAR, 0};
    auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_NONE, ZE_SAMPLER_FILTER_MODE_LINEAR, false, properties);

    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_PROPERTIES, 0, nullptr, &retSize));
    ASSERT_EQ(3u * sizeof(cl_sampler_properties), retSize);

    sampler->resetProperties();

    retSize = 123u;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_PROPERTIES, 0, nullptr, &retSize));
    EXPECT_EQ(0u, retSize);
}

TEST_F(SamplerInfoFixture, givenResetPropertiesWhenQueryingOtherParamsThenTheyAreStillAvailable) {
    cl_sampler_properties properties[] = {CL_SAMPLER_FILTER_MODE, CL_FILTER_LINEAR, 0};
    auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_REPEAT, ZE_SAMPLER_FILTER_MODE_LINEAR, true, properties);
    sampler->resetProperties();

    cl_filter_mode filterMode = 0;
    cl_addressing_mode addressingMode = 0;
    cl_bool normalized = CL_FALSE;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_FILTER_MODE, sizeof(filterMode), &filterMode, nullptr));
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_ADDRESSING_MODE, sizeof(addressingMode), &addressingMode, nullptr));
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_NORMALIZED_COORDS, sizeof(normalized), &normalized, nullptr));

    EXPECT_EQ(static_cast<cl_filter_mode>(CL_FILTER_LINEAR), filterMode);
    EXPECT_EQ(static_cast<cl_addressing_mode>(CL_ADDRESS_REPEAT), addressingMode);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), normalized);
}

TEST_F(SamplerInfoFixture, givenEmptyPropertiesListWhenQueryingPropertiesThenOnlyTerminatorIsStored) {
    cl_sampler_properties properties[] = {0};
    auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_NONE, ZE_SAMPLER_FILTER_MODE_NEAREST, false, properties);

    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_PROPERTIES, 0, nullptr, &retSize));
    ASSERT_EQ(sizeof(cl_sampler_properties), retSize);

    cl_sampler_properties stored = 0xDEAD;
    ASSERT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_PROPERTIES, retSize, &stored, nullptr));
    EXPECT_EQ(static_cast<cl_sampler_properties>(0), stored);
}

TEST_F(SamplerInfoFixture, givenUnsupportedMipParamsWhenQueryingSamplerInfoThenReturnsInvalidValue) {
    auto sampler = createSampler();

    size_t retSize = 77u;
    EXPECT_EQ(CL_INVALID_VALUE, sampler->getInfo(CL_SAMPLER_MIP_FILTER_MODE, 0, nullptr, &retSize));
    EXPECT_EQ(77u, retSize);
    EXPECT_EQ(CL_INVALID_VALUE, sampler->getInfo(CL_SAMPLER_LOD_MAX, 0, nullptr, &retSize));
    EXPECT_EQ(77u, retSize);
}

TEST_F(SamplerInfoFixture, givenUnknownParamWhenQueryingSamplerInfoThenReturnsInvalidValue) {
    auto sampler = createSampler();

    size_t retSize = 77u;
    EXPECT_EQ(CL_INVALID_VALUE, sampler->getInfo(0xDEAD0000u, 0, nullptr, &retSize));
    EXPECT_EQ(77u, retSize);
}

TEST_F(SamplerInfoFixture, givenTooSmallBufferWhenQueryingSamplerInfoThenReturnsInvalidValue) {
    auto sampler = createSampler();

    cl_addressing_mode addressingMode = 0;
    EXPECT_EQ(CL_INVALID_VALUE, sampler->getInfo(CL_SAMPLER_ADDRESSING_MODE, sizeof(cl_addressing_mode) - 1u, &addressingMode, nullptr));
}

TEST_F(SamplerInfoFixture, givenLargerBufferWhenQueryingSamplerInfoThenItStillSucceeds) {
    auto sampler = createSampler(ZE_SAMPLER_ADDRESS_MODE_MIRROR);

    cl_ulong oversized = 0;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, sampler->getInfo(CL_SAMPLER_ADDRESSING_MODE, sizeof(oversized), &oversized, &retSize));
    EXPECT_EQ(sizeof(cl_addressing_mode), retSize);
    EXPECT_EQ(static_cast<cl_addressing_mode>(CL_ADDRESS_MIRRORED_REPEAT), static_cast<cl_addressing_mode>(oversized));
}

TEST_F(SamplerInfoFixture, givenSamplerWhenQueryingL0ObjectThenTheUnderlyingSamplerIsReturned) {
    std::map<uint32_t, ze_sampler_handle_t> handles{};
    auto l0Sampler = addL0Sampler();
    handles[clDevice->getRootDeviceIndex()] = l0Sampler->toHandle();
    sampler = std::make_unique<Sampler>(leoContext.get(), std::move(handles), nullptr);

    EXPECT_EQ(l0Sampler->toHandle(), sampler->getL0Handle());
    EXPECT_EQ(static_cast<L0::Sampler *>(l0Sampler), sampler->getL0Object());
}

TEST_F(SamplerInfoFixture, givenMultiDeviceSamplerWhenQueryingHandleByRootDeviceIndexThenMatchingHandleIsReturned) {
    auto firstL0Sampler = addL0Sampler();
    auto secondL0Sampler = addL0Sampler();
    auto thirdL0Sampler = addL0Sampler();

    std::map<uint32_t, ze_sampler_handle_t> handles{};
    handles[0] = firstL0Sampler->toHandle();
    handles[1] = secondL0Sampler->toHandle();
    handles[2] = thirdL0Sampler->toHandle();
    sampler = std::make_unique<Sampler>(leoContext.get(), std::move(handles), nullptr);

    EXPECT_EQ(firstL0Sampler->toHandle(), sampler->getL0Handle(0));
    EXPECT_EQ(secondL0Sampler->toHandle(), sampler->getL0Handle(1));
    EXPECT_EQ(thirdL0Sampler->toHandle(), sampler->getL0Handle(2));
}

TEST_F(SamplerInfoFixture, givenMultiDeviceSamplerWhenQueryingHandleForUnknownRootDeviceIndexThenFirstHandleIsReturned) {
    auto firstL0Sampler = addL0Sampler();
    auto secondL0Sampler = addL0Sampler();

    std::map<uint32_t, ze_sampler_handle_t> handles{};
    handles[3] = firstL0Sampler->toHandle();
    handles[7] = secondL0Sampler->toHandle();
    sampler = std::make_unique<Sampler>(leoContext.get(), std::move(handles), nullptr);

    EXPECT_EQ(firstL0Sampler->toHandle(), sampler->getL0Handle(42));
    EXPECT_EQ(firstL0Sampler->toHandle(), sampler->getL0Handle());
}

TEST_F(SamplerInfoFixture, givenMultiDeviceSamplerWhenDestroyedThenEveryUnderlyingSamplerIsDestroyed) {
    auto firstL0Sampler = addL0Sampler();
    auto secondL0Sampler = addL0Sampler();
    auto thirdL0Sampler = addL0Sampler();

    std::map<uint32_t, ze_sampler_handle_t> handles{};
    handles[0] = firstL0Sampler->toHandle();
    handles[1] = secondL0Sampler->toHandle();
    handles[2] = thirdL0Sampler->toHandle();
    sampler = std::make_unique<Sampler>(leoContext.get(), std::move(handles), nullptr);

    sampler.reset();

    EXPECT_EQ(1u, firstL0Sampler->destroyCalled);
    EXPECT_EQ(1u, secondL0Sampler->destroyCalled);
    EXPECT_EQ(1u, thirdL0Sampler->destroyCalled);
}

TEST_F(SamplerInfoFixture, givenSamplerWhenCreatedAndDestroyedThenContextInternalReferenceIsBalanced) {
    const auto refCountBefore = leoContext->getRefInternalCount();

    createSampler();
    EXPECT_EQ(refCountBefore + 1, leoContext->getRefInternalCount());

    sampler.reset();
    EXPECT_EQ(refCountBefore, leoContext->getRefInternalCount());
}

TEST_F(SamplerInfoFixture, givenSamplerWhenCastFromHandleThenObjectIsRecovered) {
    auto sampler = createSampler();

    cl_sampler clSampler = sampler;
    EXPECT_EQ(sampler, castToObject<Sampler>(clSampler));
    EXPECT_EQ(static_cast<cl_ulong>(Sampler::objectMagic), sampler->getMagic() & Sampler::maskMagic);
}

TEST_F(SamplerInfoFixture, givenSamplerWhenTakingOwnershipTwiceFromTheSameThreadThenItIsRecursive) {
    auto sampler = createSampler();

    auto firstLock = sampler->takeOwnership();
    auto secondLock = sampler->takeOwnership();
    EXPECT_TRUE(firstLock.owns_lock());
    EXPECT_TRUE(secondLock.owns_lock());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
