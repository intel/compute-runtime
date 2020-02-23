/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "opencl/source/api/cl_types.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/error_mappers.h"
#include "opencl/source/helpers/validators.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

using namespace NEO;

template <typename TypeParam>
struct ValidatorFixture : public ::testing::Test {
};

TYPED_TEST_CASE_P(ValidatorFixture);

TYPED_TEST_P(ValidatorFixture, nullPtr) {
    TypeParam object = nullptr;

    cl_int rv = NullObjectErrorMapper<TypeParam>::retVal;
    EXPECT_EQ(rv, validateObjects(object));
}

TYPED_TEST_P(ValidatorFixture, randomMemory) {
    // 6*uint64_t to satisfy memory requirements
    // we need 2 before object (dispatchTable)
    // and 4 of object (magic)
    uint64_t randomMemory[6] = {
        0xdeadbeef,
    };
    TypeParam object = (TypeParam)(randomMemory + 2);

    cl_int rv = InvalidObjectErrorMapper<TypeParam>::retVal;
    EXPECT_EQ(rv, validateObjects(object));
}

REGISTER_TYPED_TEST_CASE_P(
    ValidatorFixture,
    nullPtr,
    randomMemory);

// Define new command types to run the parameterized tests
typedef ::testing::Types<
    cl_command_queue,
    device_queue, // internal type
    cl_context,
    cl_device_id,
    cl_event,
    cl_kernel,
    cl_mem,
    cl_platform_id,
    cl_program,
    uint64_t /*cl_queue_properties*/ *,
    cl_sampler>
    ValidatorParams;

INSTANTIATE_TYPED_TEST_CASE_P(Validator, ValidatorFixture, ValidatorParams);

TEST(GenericValidator, nullCTXTnullCQ) {
    cl_context context = nullptr;
    cl_command_queue command_queue = nullptr;

    EXPECT_EQ(CL_INVALID_CONTEXT, validateObjects(context, command_queue));
}

TEST(UserPointer, ExpectNonNullUserPtr) {
    void *ptr = nullptr;

    EXPECT_EQ(CL_INVALID_VALUE, validateObjects(ptr));
}

TEST(UserPointer, DontValidateUserPointersForValidity) {
    void *ptr = ptrGarbage;

    EXPECT_EQ(CL_SUCCESS, validateObjects(ptr));
}

TEST(EventWaitList, zeroCount_nonNullPointer) {
    cl_event eventList = (cl_event)ptrGarbage;
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, validateObjects(EventWaitList(0, &eventList)));
}

TEST(EventWaitList, zeroCount_nullPointer) {
    EXPECT_EQ(CL_SUCCESS, validateObjects(EventWaitList(0, nullptr)));
}

TEST(EventWaitList, nonZeroCount_nullPointer) {
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, validateObjects(EventWaitList(1, nullptr)));
}

TEST(EventWaitList, nonZeroCount_noNullPointer) {
    cl_event eventList = (cl_event)ptrGarbage;
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, validateObjects(EventWaitList(1, &eventList)));
}

TEST(DeviceList, zeroCount_nonNullPointer) {
    cl_device_id devList = (cl_device_id)ptrGarbage;
    EXPECT_EQ(CL_INVALID_VALUE, validateObjects(DeviceList(0, &devList)));
}

TEST(DeviceList, zeroCount_nullPointer) {
    EXPECT_EQ(CL_SUCCESS, validateObjects(DeviceList(0, nullptr)));
}

TEST(DeviceList, nonZeroCount_nullPointer) {
    EXPECT_EQ(CL_INVALID_VALUE, validateObjects(DeviceList(1, nullptr)));
}

TEST(DeviceList, nonZeroCount_noNullPointer) {
    cl_device_id devList = (cl_device_id)ptrGarbage;
    EXPECT_EQ(CL_INVALID_DEVICE, validateObjects(DeviceList(1, &devList)));
}

TEST(MemObjList, zeroCount_nonNullPointer) {
    cl_mem memList = static_cast<cl_mem>(ptrGarbage);
    EXPECT_EQ(CL_INVALID_VALUE, validateObjects(MemObjList(0, &memList)));
}

TEST(MemObjList, zeroCount_nullPointer) {
    EXPECT_EQ(CL_SUCCESS, validateObjects(MemObjList(0, nullptr)));
}

TEST(MemObjList, nonZeroCount_nullPointer) {
    EXPECT_EQ(CL_INVALID_VALUE, validateObjects(MemObjList(1, nullptr)));
}

TEST(MemObjList, nonZeroCount_noNullPointer) {
    cl_mem memList = static_cast<cl_mem>(ptrGarbage);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, validateObjects(MemObjList(1, &memList)));
}

TEST(MemObjList, nonZeroCount_validPointer) {
    std::unique_ptr<MockBuffer> buffer(new MockBuffer());
    cl_mem memList = static_cast<cl_mem>(buffer.get());
    EXPECT_EQ(CL_SUCCESS, validateObjects(MemObjList(1, &memList)));
}

TEST(NonZeroBufferSizeValidator, zero) {
    auto bsv = (NonZeroBufferSize)0;
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, validateObjects(bsv));
}

TEST(NonZeroBufferSizeValidator, nonZero) {
    auto bsv = (NonZeroBufferSize)~0;
    EXPECT_EQ(CL_SUCCESS, validateObjects(bsv));
}

TEST(Platform, givenNullPlatformThenReturnInvalidPlatform) {
    cl_platform_id platform = nullptr;
    EXPECT_EQ(CL_INVALID_PLATFORM, validateObjects(platform));
}

TEST(Platform, givenPlatformThenReturnSUCCESS) {
    MockPlatform platform;
    cl_platform_id clPlatformId = &platform;
    EXPECT_EQ(CL_SUCCESS, validateObjects(clPlatformId));
}

typedef ::testing::TestWithParam<size_t> PatternSizeValid;

TEST_P(PatternSizeValid, valid) {
    auto psv = (PatternSize)GetParam();
    EXPECT_EQ(CL_SUCCESS, validateObjects(psv));
}

INSTANTIATE_TEST_CASE_P(PatternSize,
                        PatternSizeValid,
                        ::testing::Values(1, 2, 4, 8, 16, 32, 64, 128));

typedef ::testing::TestWithParam<size_t> PatternSizeInvalid;

TEST_P(PatternSizeInvalid, invalid) {
    auto psv = (PatternSize)GetParam();
    EXPECT_EQ(CL_INVALID_VALUE, validateObjects(psv));
}

INSTANTIATE_TEST_CASE_P(PatternSize,
                        PatternSizeInvalid,
                        ::testing::Values(0, 3, 5, 256, 512, 1024));

TEST(WithCastToInternal, nullpointer) {
    Context *pContext = nullptr;
    cl_context context = nullptr;

    auto ret = WithCastToInternal(context, &pContext);

    EXPECT_EQ(ret, nullptr);
}

TEST(WithCastToInternal, nonnullpointer) {
    Context *pContext = nullptr;
    auto temp = std::unique_ptr<Context>(new MockContext());
    cl_context context = temp.get();

    auto ret = WithCastToInternal(context, &pContext);

    EXPECT_NE(ret, nullptr);
}

TEST(validateYuvOperation, GivenValidateYuvOperationWhenValidOriginAndRegionThenReturnSuccess) {
    size_t origin[3] = {8, 0, 0};
    size_t region[3] = {8, 0, 0};

    auto ret = validateYuvOperation(origin, region);
    EXPECT_EQ(CL_SUCCESS, ret);
}

TEST(validateYuvOperation, GivenValidateYuvOperationWhenInvalidOriginThenReturnFailure) {
    size_t origin[3] = {1, 0, 0};
    size_t region[3] = {8, 0, 0};

    auto ret = validateYuvOperation(origin, region);
    EXPECT_EQ(CL_INVALID_VALUE, ret);
}

TEST(validateYuvOperation, GivenValidateYuvOperationWhenInvalidRegionThenReturnFailure) {
    size_t origin[3] = {8, 0, 0};
    size_t region[3] = {1, 0, 0};

    auto ret = validateYuvOperation(origin, region);
    EXPECT_EQ(CL_INVALID_VALUE, ret);
}

TEST(validateYuvOperation, GivenValidateYuvOperationWhenNullOriginThenReturnFailure) {
    size_t *origin = nullptr;
    size_t region[3] = {1, 0, 0};

    auto ret = validateYuvOperation(origin, region);
    EXPECT_EQ(CL_INVALID_VALUE, ret);
}

TEST(validateYuvOperation, GivenValidateYuvOperationWhenNullRegionThenReturnFailure) {
    size_t origin[3] = {8, 0, 0};
    size_t *region = nullptr;

    auto ret = validateYuvOperation(origin, region);
    EXPECT_EQ(CL_INVALID_VALUE, ret);
}

TEST(areNotNullptr, WhenGivenAllNonNullParamsTheReturnsTrue) {
    int a = 0;
    int b = 0;
    int c = 0;
    EXPECT_TRUE(areNotNullptr(&a));
    EXPECT_TRUE(areNotNullptr(&a, &b));
    EXPECT_TRUE(areNotNullptr(&a, &b, &c));
}

TEST(areNotNullptr, WhenGivenAllNullParamsTheReturnsFalse) {
    int *a = nullptr;
    int *b = nullptr;
    int *c = nullptr;
    EXPECT_FALSE(areNotNullptr(a));
    EXPECT_FALSE(areNotNullptr(a, b));
    EXPECT_FALSE(areNotNullptr(a, b, c));
}

TEST(areNotNullptr, WhenGivenNullParameterAmongNonNullParamsTheReturnsFalse) {
    int *a = nullptr;
    int b = 0;
    int c = 0;
    EXPECT_FALSE(areNotNullptr(a));
    EXPECT_FALSE(areNotNullptr(a, &b));
    EXPECT_FALSE(areNotNullptr(&b, a));
    EXPECT_FALSE(areNotNullptr(a, &b, &c));
    EXPECT_FALSE(areNotNullptr(&b, a, &c));
}
