/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

template <typename T>
struct ClCreateImageTests : public ApiFixture<>,
                            public T {

    void SetUp() override {
        ApiFixture::setUp();

        // clang-format off
        imageFormat.image_channel_order     = CL_RGBA;
        imageFormat.image_channel_data_type = CL_UNORM_INT8;

        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = 32;
        imageDesc.image_height      = 32;
        imageDesc.image_depth       = 1;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object        = nullptr;
        // clang-format on
    }

    void TearDown() override {
        ApiFixture::tearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
};

typedef ClCreateImageTests<::testing::Test> clCreateImageTest;

TEST_F(clCreateImageTest, GivenNullHostPtrWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(clCreateImageTest, GivenDeviceThatDoesntSupportImagesWhenCreatingTiledImageThenInvalidOperationErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    MockClDevice mockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), 0)};
    MockContext mockContext{&mockClDevice};

    mockClDevice.sharedDeviceInfo.imageSupport = CL_FALSE;
    cl_bool imageSupportInfo = CL_TRUE;
    auto status = clGetDeviceInfo(&mockClDevice, CL_DEVICE_IMAGE_SUPPORT, sizeof(imageSupportInfo), &imageSupportInfo, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    cl_bool expectedValue = CL_FALSE;
    EXPECT_EQ(expectedValue, imageSupportInfo);
    auto image = clCreateImage(
        &mockContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    if (defaultHwInfo->capabilityTable.supportsImages) {
        EXPECT_EQ(CL_INVALID_OPERATION, retVal);
        EXPECT_EQ(nullptr, image);
    } else {
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, image);

        retVal = clReleaseMemObject(image);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

HWTEST_F(clCreateImageTest, GivenDeviceThatDoesntSupportImagesWhenCreatingNonTiledImageThenCreate) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    MockClDevice mockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), 0)};
    MockContext mockContext{&mockClDevice};

    mockClDevice.sharedDeviceInfo.imageSupport = CL_FALSE;
    cl_bool imageSupportInfo = CL_TRUE;
    auto status = clGetDeviceInfo(&mockClDevice, CL_DEVICE_IMAGE_SUPPORT, sizeof(imageSupportInfo), &imageSupportInfo, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    cl_bool expectedValue = CL_FALSE;
    EXPECT_EQ(expectedValue, imageSupportInfo);

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_height = 1;

    auto image = clCreateImage(
        &mockContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, GivenDeviceThatDoesntSupportImagesWhenCreatingImageWithPropertiesINTELThenImageCreatedAndSuccessIsReturned) {
    DebugManagerStateRestore stateRestore;
    MockClDevice mockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), 0)};
    MockContext mockContext{&mockClDevice};

    mockClDevice.sharedDeviceInfo.imageSupport = CL_FALSE;
    cl_bool imageSupportInfo = CL_TRUE;
    auto status = clGetDeviceInfo(&mockClDevice, CL_DEVICE_IMAGE_SUPPORT, sizeof(imageSupportInfo), &imageSupportInfo, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    cl_bool expectedValue = CL_FALSE;
    EXPECT_EQ(expectedValue, imageSupportInfo);

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_height = 1;
    debugManager.flags.ForceLinearImages.set(true);

    auto image = clCreateImageWithPropertiesINTEL(
        &mockContext,
        nullptr,
        0,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, GivenDeviceThatDoesntSupportImagesWhenCreatingImagesWithPropertiesAndWithoutThenInvalidOperationErrorIsReturned) {
    auto hardwareInfo = *defaultHwInfo;
    hardwareInfo.capabilityTable.supportsImages = false;
    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0));
    cl_device_id deviceId = pClDevice.get();
    auto pContext = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));

    auto image = clCreateImage(pContext.get(), CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(nullptr, image);

    auto imageWithProperties = clCreateImageWithProperties(pContext.get(), nullptr, CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(nullptr, imageWithProperties);
}

TEST_F(clCreateImageTest, GivenNullContextWhenCreatingImageWithPropertiesThenInvalidContextErrorIsReturned) {
    auto image = clCreateImageWithProperties(
        nullptr,
        nullptr,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenNonNullHostPtrAndAlignedRowPitchWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    char hostPtr[4096];
    imageDesc.image_row_pitch = 128;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, GivenNonNullHostPtrAndUnalignedRowPitchWhenCreatingImageThenInvalidImageDescriptotErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    char hostPtr[4096];
    imageDesc.image_row_pitch = 129;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenNonNullHostPtrAndSmallRowPitchWhenCreatingImageThenInvalidImageDescriptorErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    char hostPtr[4096];
    imageDesc.image_row_pitch = 4;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenUnrestrictedIntelFlagWhenCreatingImageWithInvalidFlagCombinationThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL;
    auto image = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, GivenNotNullHostPtrAndNoHostPtrFlagWhenCreatingImageThenInvalidHostPtrErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    char hostPtr[4096];
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenInvalidFlagBitsWhenCreatingImageThenInvalidValueErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    cl_mem_flags flags = (1 << 12);
    auto image = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenInvalidFlagBitsWhenCreatingImageFromAnotherImageThenInvalidValueErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    imageFormat.image_channel_order = CL_NV12_INTEL;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    imageFormat.image_channel_order = CL_RG;
    imageDesc.mem_object = image;
    cl_mem_flags flags = (1 << 30);

    auto imageFromImageObject = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(imageFromImageObject);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenInvalidRowPitchWhenCreatingImageThenInvalidImageDescriptorErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    imageDesc.image_row_pitch = 655;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenNullHostPtrAndCopyHostPtrFlagWhenCreatingImageThenInvalidHostPtrErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenNullHostPtrAndMemUseHostPtrFlagWhenCreatingImageThenInvalidHostPtrErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenNullHostPtrAndNonZeroRowPitchWhenCreatingImageThenInvalidImageDescriptorErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    imageDesc.image_row_pitch = 4;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenDeviceNotSupportingImagesWhenCreatingImageFromBufferThenInvalidOperationErrorIsReturned) {

    auto hardwareInfo = *defaultHwInfo;
    hardwareInfo.capabilityTable.supportsImages = false;
    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0));
    cl_device_id deviceId = pClDevice.get();
    auto pContext = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));

    auto buffer = clCreateBuffer(pContext.get(), CL_MEM_READ_WRITE, 4096 * 9, nullptr, nullptr);
    imageDesc.mem_object = buffer;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    auto image = clCreateImageWithPropertiesINTEL(
        pContext.get(),
        nullptr,
        0,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    clReleaseMemObject(buffer);
}

TEST_F(clCreateImageTest, GivenNonZeroPitchWhenCreatingImageFromBufferThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGES_OR_SKIP(pContext);

    auto buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, 4096 * 9, nullptr, nullptr);
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();

    imageDesc.mem_object = buffer;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 17;
    imageDesc.image_height = 17;
    imageDesc.image_row_pitch = gfxCoreHelper.getPitchAlignmentForImage(pDevice->getRootDeviceEnvironment()) * 17;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_NE(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_NE(nullptr, image);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, GivenNotNullHostPtrAndRowPitchIsNotGreaterThanWidthTimesElementSizeWhenCreatingImageThenInvalidImageDescriptorErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    imageDesc.image_row_pitch = 64;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenNullContextWhenCreatingImageThenInvalidContextErrorIsReturned) {
    auto image = clCreateImage(
        nullptr,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, WhenCreatingImageWithPropertiesThenParametersAreCorrectlyPassed) {
    VariableBackup<ImageFunctions::ValidateAndCreateImageFunc> imageCreateBackup{&ImageFunctions::validateAndCreateImage};

    cl_context context = pContext;
    cl_mem_properties *propertiesValues[] = {nullptr, reinterpret_cast<cl_mem_properties *>(0x1234)};
    cl_mem_flags flagsValues[] = {0, 4321};
    cl_image_format imageFormat = this->imageFormat;
    cl_image_desc imageDesc = this->imageDesc;
    void *pHostMem = reinterpret_cast<void *>(0x8000);

    for (auto properties : propertiesValues) {
        for (auto flags : flagsValues) {

            auto mockFunction = [context, properties, flags, &imageFormat, &imageDesc, pHostMem](cl_context contextArg,
                                                                                                 const cl_mem_properties *propertiesArg,
                                                                                                 cl_mem_flags flagsArg,
                                                                                                 cl_mem_flags_intel flagsIntelArg,
                                                                                                 const cl_image_format *imageFormatArg,
                                                                                                 const cl_image_desc *imageDescArg,
                                                                                                 const void *hostPtrArg,
                                                                                                 cl_int &errcodeRetArg) -> cl_mem {
                cl_mem_flags_intel expectedFlagsIntelArg = 0;

                EXPECT_EQ(context, contextArg);
                EXPECT_EQ(properties, propertiesArg);
                EXPECT_EQ(flags, flagsArg);
                EXPECT_EQ(expectedFlagsIntelArg, flagsIntelArg);
                EXPECT_EQ(&imageFormat, imageFormatArg);
                EXPECT_EQ(&imageDesc, imageDescArg);
                EXPECT_EQ(pHostMem, hostPtrArg);

                return nullptr;
            };
            imageCreateBackup = mockFunction;
            clCreateImageWithProperties(context, properties, flags, &imageFormat, &imageDesc, pHostMem, nullptr);
        }
    }
}

TEST_F(clCreateImageTest, WhenCreatingImageWithPropertiesThenErrorCodeIsCorrectlySet) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    VariableBackup<ImageFunctions::ValidateAndCreateImageFunc> imageCreateBackup{&ImageFunctions::validateAndCreateImage};

    cl_mem_properties *properties = nullptr;
    cl_mem_flags flags = 0;
    void *pHostMem = nullptr;
    cl_int errcodeRet;

    cl_int retValues[] = {CL_SUCCESS, CL_INVALID_PROPERTY};

    for (auto retValue : retValues) {
        auto mockFunction = [retValue](cl_context contextArg,
                                       const cl_mem_properties *propertiesArg,
                                       cl_mem_flags flagsArg,
                                       cl_mem_flags_intel flagsIntelArg,
                                       const cl_image_format *imageFormatArg,
                                       const cl_image_desc *imageDescArg,
                                       const void *hostPtrArg,
                                       cl_int &errcodeRetArg) -> cl_mem {
            errcodeRetArg = retValue;

            return nullptr;
        };
        imageCreateBackup = mockFunction;
        clCreateImageWithProperties(pContext, properties, flags, &imageFormat, &imageDesc, pHostMem, &errcodeRet);
        EXPECT_EQ(retValue, errcodeRet);
    }
}

TEST_F(clCreateImageTest, GivenImageCreatedWithNullPropertiesWhenQueryingPropertiesThenNothingIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    cl_int retVal = CL_SUCCESS;
    auto image = clCreateImageWithProperties(pContext, nullptr, 0, &imageFormat, &imageDesc, nullptr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(nullptr, image);

    size_t propertiesSize;
    retVal = clGetMemObjectInfo(image, CL_MEM_PROPERTIES, 0, nullptr, &propertiesSize);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(0u, propertiesSize);

    clReleaseMemObject(image);
}

TEST_F(clCreateImageTest, WhenCreatingImageWithPropertiesThenPropertiesAreCorrectlyStored) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties properties[5];
    size_t propertiesSize;

    std::vector<std::vector<uint64_t>> propertiesToTest{
        {0},
        {CL_MEM_FLAGS, CL_MEM_WRITE_ONLY, 0},
        {CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0},
        {CL_MEM_FLAGS, CL_MEM_WRITE_ONLY, CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0}};

    for (auto testProperties : propertiesToTest) {
        auto image = clCreateImageWithProperties(pContext, testProperties.data(), 0, &imageFormat, &imageDesc, nullptr, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, image);

        retVal = clGetMemObjectInfo(image, CL_MEM_PROPERTIES, sizeof(properties), properties, &propertiesSize);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(testProperties.size() * sizeof(cl_mem_properties), propertiesSize);
        for (size_t i = 0; i < testProperties.size(); i++) {
            EXPECT_EQ(testProperties[i], properties[i]);
        }

        clReleaseMemObject(image);
    }
}

typedef ClCreateImageTests<::testing::Test> clCreateImageTestYUV;
TEST_F(clCreateImageTestYUV, GivenInvalidGlagWhenCreatingYuvImageThenInvalidValueErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    imageFormat.image_channel_order = CL_YUYV_INTEL;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTestYUV, Given1DImageTypeWhenCreatingYuvImageThenInvalidImageDescriptorErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    imageFormat.image_channel_order = CL_YUYV_INTEL;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

typedef ClCreateImageTests<::testing::TestWithParam<uint64_t>> clCreateImageValidFlags;

static cl_mem_flags validFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY,
    CL_MEM_USE_HOST_PTR,
    CL_MEM_ALLOC_HOST_PTR,
    CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_NO_ACCESS,
    CL_MEM_NO_ACCESS_INTEL,
    CL_MEM_FORCE_LINEAR_STORAGE_INTEL,
    CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL,
};

INSTANTIATE_TEST_SUITE_P(CreateImageWithFlags,
                         clCreateImageValidFlags,
                         ::testing::ValuesIn(validFlags));

TEST_P(clCreateImageValidFlags, GivenValidFlagsWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    cl_mem_flags flags = GetParam();
    std::unique_ptr<char[]> ptr;
    char *hostPtr = nullptr;

    if (flags & CL_MEM_USE_HOST_PTR ||
        flags & CL_MEM_COPY_HOST_PTR) {
        ptr = std::make_unique<char[]>(alignUp(imageDesc.image_width * imageDesc.image_height * 4, MemoryConstants::pageSize));
        hostPtr = ptr.get();
    }

    auto image = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

typedef ClCreateImageTests<::testing::TestWithParam<uint64_t>> clCreateImageInvalidFlags;

static cl_mem_flags invalidFlagsCombinations[] = {
    CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY,
    CL_MEM_READ_WRITE | CL_MEM_READ_ONLY,
    CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY,
    CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR,
    CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS,
    CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS,
    CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_WRITE,
    CL_MEM_NO_ACCESS_INTEL | CL_MEM_WRITE_ONLY,
    CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_ONLY};

INSTANTIATE_TEST_SUITE_P(CreateImageWithFlags,
                         clCreateImageInvalidFlags,
                         ::testing::ValuesIn(invalidFlagsCombinations));

TEST_P(clCreateImageInvalidFlags, GivenInvalidFlagsCombinationsWhenCreatingImageThenInvalidValueErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);

    char ptr[10];
    imageDesc.image_row_pitch = 128;
    cl_mem_flags flags = GetParam();

    auto image = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        ptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, image);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, flags, 0};
    image = clCreateImageWithPropertiesINTEL(
        pContext,
        properties,
        0,
        &imageFormat,
        &imageDesc,
        ptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_PROPERTY, retVal);
    EXPECT_EQ(nullptr, image);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    image = clCreateImageWithPropertiesINTEL(
        pContext,
        nullptr,
        flags,
        &imageFormat,
        &imageDesc,
        ptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, image);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

struct ImageFlags {
    cl_mem_flags parentFlags;
    cl_mem_flags flags;
};

static ImageFlags flagsWithUnrestrictedIntel[] = {
    {CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL, CL_MEM_READ_WRITE},
    {CL_MEM_READ_WRITE, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL}};

typedef ClCreateImageTests<::testing::TestWithParam<ImageFlags>> clCreateImageFlagsUnrestrictedIntel;

INSTANTIATE_TEST_SUITE_P(CreateImageWithFlags,
                         clCreateImageFlagsUnrestrictedIntel,
                         ::testing::ValuesIn(flagsWithUnrestrictedIntel));

TEST_P(clCreateImageFlagsUnrestrictedIntel, GivenFlagsIncludingUnrestrictedIntelWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGES_OR_SKIP(pContext);

    imageFormat.image_channel_order = CL_NV12_INTEL;
    ImageFlags imageFlags = GetParam();
    cl_mem_flags parentFlags = imageFlags.parentFlags;
    cl_mem_flags flags = imageFlags.flags;

    auto image = clCreateImage(
        pContext,
        parentFlags | CL_MEM_HOST_NO_ACCESS,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    imageDesc.mem_object = image;
    imageFormat.image_channel_order = CL_RG;

    auto imageFromImageObject = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(imageFromImageObject);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

static ImageFlags validFlagsAndParentFlags[] = {
    {CL_MEM_WRITE_ONLY, CL_MEM_HOST_NO_ACCESS},
    {CL_MEM_READ_ONLY, CL_MEM_HOST_NO_ACCESS},
    {CL_MEM_NO_ACCESS_INTEL, CL_MEM_HOST_NO_ACCESS},
    {CL_MEM_HOST_NO_ACCESS, CL_MEM_READ_WRITE}};

typedef ClCreateImageTests<::testing::TestWithParam<ImageFlags>> clCreateImageValidFlagsAndParentFlagsCombinations;

INSTANTIATE_TEST_SUITE_P(CreateImageWithFlags,
                         clCreateImageValidFlagsAndParentFlagsCombinations,
                         ::testing::ValuesIn(validFlagsAndParentFlags));

TEST_P(clCreateImageValidFlagsAndParentFlagsCombinations, GivenValidFlagsAndParentFlagsWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGES_OR_SKIP(pContext);

    imageFormat.image_channel_order = CL_NV12_INTEL;
    ImageFlags imageFlags = GetParam();
    cl_mem_flags parentFlags = imageFlags.parentFlags;
    cl_mem_flags flags = imageFlags.flags;

    auto image = clCreateImage(
        pContext,
        parentFlags | CL_MEM_HOST_NO_ACCESS,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    imageDesc.mem_object = image;
    imageFormat.image_channel_order = CL_RG;

    auto imageFromImageObject = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(imageFromImageObject);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

static ImageFlags invalidFlagsAndParentFlags[] = {
    {CL_MEM_WRITE_ONLY, CL_MEM_READ_WRITE},
    {CL_MEM_WRITE_ONLY, CL_MEM_READ_ONLY},
    {CL_MEM_READ_ONLY, CL_MEM_READ_WRITE},
    {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY},
    {CL_MEM_NO_ACCESS_INTEL, CL_MEM_READ_WRITE},
    {CL_MEM_NO_ACCESS_INTEL, CL_MEM_WRITE_ONLY},
    {CL_MEM_NO_ACCESS_INTEL, CL_MEM_READ_ONLY},
    {CL_MEM_HOST_NO_ACCESS, CL_MEM_HOST_WRITE_ONLY},
    {CL_MEM_HOST_NO_ACCESS, CL_MEM_HOST_READ_ONLY}};

typedef ClCreateImageTests<::testing::TestWithParam<ImageFlags>> clCreateImageInvalidFlagsAndParentFlagsCombinations;

INSTANTIATE_TEST_SUITE_P(CreateImageWithFlags,
                         clCreateImageInvalidFlagsAndParentFlagsCombinations,
                         ::testing::ValuesIn(invalidFlagsAndParentFlags));

TEST_P(clCreateImageInvalidFlagsAndParentFlagsCombinations, GivenInvalidFlagsAndParentFlagsWhenCreatingImageThenInvalidMemObjectErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    imageFormat.image_channel_order = CL_NV12_INTEL;
    ImageFlags imageFlags = GetParam();
    cl_mem_flags parentFlags = imageFlags.parentFlags;
    cl_mem_flags flags = imageFlags.flags;

    auto image = clCreateImage(
        pContext,
        parentFlags | CL_MEM_HOST_NO_ACCESS,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    imageFormat.image_channel_order = CL_RG;
    imageDesc.mem_object = image;

    auto imageFromImageObject = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(imageFromImageObject);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

struct ImageSizes {
    size_t width;
    size_t height;
    size_t depth;
};

ImageSizes validImage2DSizes[] = {{64, 64, 1}, {3, 3, 1}, {8192, 1, 1}, {117, 39, 1}, {16384, 4, 1}, {4, 16384, 1}};

typedef ClCreateImageTests<::testing::TestWithParam<ImageSizes>> clCreateImageValidSizesTest;

INSTANTIATE_TEST_SUITE_P(validImage2DSizes,
                         clCreateImageValidSizesTest,
                         ::testing::ValuesIn(validImage2DSizes));

TEST_P(clCreateImageValidSizesTest, GivenValidSizesWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    ImageSizes sizes = GetParam();
    imageDesc.image_width = sizes.width;
    imageDesc.image_height = sizes.height;
    imageDesc.image_depth = sizes.depth;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_NE(nullptr, image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(image);
}

typedef ClCreateImageTests<::testing::Test> clCreateImage2DTest;

TEST_F(clCreateImage2DTest, GivenValidParametersWhenCreating2DImageThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    auto image = clCreateImage2D(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        0,
        0,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImage2DTest, GivenNoPtrToReturnValueWhenCreating2DImageThenImageIsCreated) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    auto image = clCreateImage2D(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        0,
        0,
        nullptr);

    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImage2DTest, GivenInvalidContextsWhenCreating2DImageThenInvalidContextErrorIsReturned) {
    auto image = clCreateImage2D(
        nullptr,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        0,
        0,
        &retVal);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImage2DTest, GivenDeviceThatDoesntSupportImagesWhenCreatingImagesWithclCreateImage2DThenInvalidOperationErrorIsReturned) {
    auto hardwareInfo = *defaultHwInfo;
    hardwareInfo.capabilityTable.supportsImages = false;
    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0));
    cl_device_id deviceId = pClDevice.get();
    auto pContext = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));

    auto image = clCreateImage2D(
        pContext.get(),
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        0,
        0,
        &retVal);
    ASSERT_EQ(nullptr, image);
    ASSERT_EQ(CL_INVALID_OPERATION, retVal);
}

typedef ClCreateImageTests<::testing::Test> clCreateImage3DTest;

TEST_F(clCreateImage3DTest, GivenValidParametersWhenCreating3DImageThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    auto image = clCreateImage3D(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        1,
        0,
        0,
        0,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImage3DTest, GivenNoPtrToReturnValueWhenCreating3DImageThenImageIsCreated) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    auto image = clCreateImage3D(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        1,
        0,
        0,
        0,
        nullptr);

    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImage3DTest, GivenInvalidContextsWhenCreating3DImageThenInvalidContextErrorIsReturned) {
    auto image = clCreateImage3D(
        nullptr,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        1,
        0,
        0,
        0,
        &retVal);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImage3DTest, GivenDeviceThatDoesntSupportImagesWhenCreatingImagesWithclCreateImage3DThenInvalidOperationErrorIsReturned) {
    auto hardwareInfo = *defaultHwInfo;
    hardwareInfo.capabilityTable.supportsImages = false;
    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0));
    cl_device_id deviceId = pClDevice.get();
    auto pContext = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));

    auto image = clCreateImage3D(
        pContext.get(),
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        1,
        0,
        0,
        0,
        &retVal);
    ASSERT_EQ(nullptr, image);
    ASSERT_EQ(CL_INVALID_OPERATION, retVal);
}

using clCreateImageWithPropertiesINTELTest = clCreateImageTest;

TEST_F(clCreateImageWithPropertiesINTELTest, GivenInvalidContextWhenCreatingImageWithPropertiesThenInvalidContextErrorIsReturned) {

    auto image = clCreateImageWithPropertiesINTEL(
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageWithPropertiesINTELTest, GivenValidParametersWhenCreatingImageWithPropertiesThenImageIsCreatedAndSuccessReturned) {

    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_READ_WRITE, 0};
    auto image = clCreateImageWithPropertiesINTEL(
        pContext,
        properties,
        0,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);

    image = clCreateImageWithPropertiesINTEL(
        pContext,
        nullptr,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageWithPropertiesINTELTest, GivenInvalidPropertyKeyWhenCreatingImageWithPropertiesThenInvalidValueErrorIsReturned) {

    cl_mem_properties_intel properties[] = {(cl_mem_properties_intel(1) << 31), 0, 0};

    auto image = clCreateImageWithPropertiesINTEL(
        pContext,
        properties,
        0,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_INVALID_PROPERTY, retVal);
}

typedef ClCreateImageTests<::testing::Test> ClCreateImageFromImageTest;

TEST_F(ClCreateImageFromImageTest, GivenImage2dWhenCreatingImage2dFromImageWithTheSameDescriptorAndValidFormatThenImageIsCreatedAndSuccessReturned) {
    REQUIRE_IMAGES_OR_SKIP(pContext);

    imageFormat.image_channel_order = CL_BGRA;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_width), &imageDesc.image_width, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_height), &imageDesc.image_height, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_DEPTH, sizeof(imageDesc.image_depth), &imageDesc.image_depth, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ROW_PITCH, sizeof(imageDesc.image_row_pitch), &imageDesc.image_row_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_SLICE_PITCH, sizeof(imageDesc.image_slice_pitch), &imageDesc.image_slice_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_MIP_LEVELS, sizeof(imageDesc.num_mip_levels), &imageDesc.num_mip_levels, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_SAMPLES, sizeof(imageDesc.num_samples), &imageDesc.num_samples, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ARRAY_SIZE, sizeof(imageDesc.image_array_size), &imageDesc.image_array_size, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    imageDesc.mem_object = image;
    imageFormat.image_channel_order = CL_sBGRA;

    auto imageFromImageObject = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    if (pContext->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features == false) {
        EXPECT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, retVal);
        EXPECT_EQ(nullptr, imageFromImageObject);
    } else {
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, imageFromImageObject);
        retVal = clReleaseMemObject(imageFromImageObject);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateImageFromImageTest, GivenImage2dWhenCreatingImage2dFromImageWithDifferentDescriptorAndValidFormatThenInvalidImageFormatDescriptorErrorIsReturned) {
    REQUIRE_IMAGES_OR_SKIP(pContext);

    imageFormat.image_channel_order = CL_BGRA;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_width), &imageDesc.image_width, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_height), &imageDesc.image_height, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_DEPTH, sizeof(imageDesc.image_depth), &imageDesc.image_depth, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ROW_PITCH, sizeof(imageDesc.image_row_pitch), &imageDesc.image_row_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_SLICE_PITCH, sizeof(imageDesc.image_slice_pitch), &imageDesc.image_slice_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_MIP_LEVELS, sizeof(imageDesc.num_mip_levels), &imageDesc.num_mip_levels, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_SAMPLES, sizeof(imageDesc.num_samples), &imageDesc.num_samples, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ARRAY_SIZE, sizeof(imageDesc.image_array_size), &imageDesc.image_array_size, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    imageDesc.mem_object = image;
    imageDesc.image_width++;
    imageFormat.image_channel_order = CL_sBGRA;

    auto imageFromImageObject = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    if (pContext->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features) {
        EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
    } else {
        EXPECT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, retVal);
    }
    EXPECT_EQ(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateImageFromImageTest, GivenImage2dWhenCreatingImage2dFromImageWithTheSameDescriptorAndNotValidFormatThenInvalidImageFormatDescriptorErrorIsReturned) {
    REQUIRE_IMAGES_OR_SKIP(pContext);

    imageFormat.image_channel_order = CL_BGRA;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_width), &imageDesc.image_width, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_height), &imageDesc.image_height, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_DEPTH, sizeof(imageDesc.image_depth), &imageDesc.image_depth, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ROW_PITCH, sizeof(imageDesc.image_row_pitch), &imageDesc.image_row_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_SLICE_PITCH, sizeof(imageDesc.image_slice_pitch), &imageDesc.image_slice_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_MIP_LEVELS, sizeof(imageDesc.num_mip_levels), &imageDesc.num_mip_levels, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_SAMPLES, sizeof(imageDesc.num_samples), &imageDesc.num_samples, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ARRAY_SIZE, sizeof(imageDesc.image_array_size), &imageDesc.image_array_size, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    imageDesc.mem_object = image;
    imageFormat.image_channel_order = CL_BGRA;

    auto imageFromImageObject = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

uint32_t non2dImageTypes[] = {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE1D_BUFFER, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D};

struct ClCreateNon2dImageFromImageTest : public ClCreateImageFromImageTest,
                                         public ::testing::WithParamInterface<uint32_t /*image type*/> {
    void SetUp() override {
        ClCreateImageFromImageTest::SetUp();
        image = ImageFunctions::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, image);
        imageDesc.mem_object = image;
    }
    void TearDown() override {
        retVal = clReleaseMemObject(image);
        ClCreateImageFromImageTest::TearDown();
    }
    cl_mem image;
};

TEST_P(ClCreateNon2dImageFromImageTest, GivenImage2dWhenCreatingImageFromNon2dImageThenInvalidImageDescriptorErrorIsReturned) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(pContext);
    imageDesc.image_type = GetParam();
    auto imageFromImageObject = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, imageFromImageObject);
}

INSTANTIATE_TEST_SUITE_P(clCreateNon2dImageFromImageTests,
                         ClCreateNon2dImageFromImageTest,
                         ::testing::ValuesIn(non2dImageTypes));

using clCreateImageWithMultiDeviceContextTests = MultiRootDeviceFixture;

TEST_F(clCreateImageWithMultiDeviceContextTests, GivenImageCreatedWithoutHostPtrAndWithContextdWithMultiDeviceThenGraphicsAllocationsAreProperlyCreatedAndMapPtrIsNotSet) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    std::unique_ptr<Image> image(ImageHelperUlt<ImageWithoutHostPtr>::create(context.get()));

    EXPECT_EQ(image->getMultiGraphicsAllocation().getGraphicsAllocations().size(), 3u);
    EXPECT_NE(image->getMultiGraphicsAllocation().getGraphicsAllocation(1u), nullptr);
    EXPECT_NE(image->getMultiGraphicsAllocation().getGraphicsAllocation(2u), nullptr);
    EXPECT_NE(image->getMultiGraphicsAllocation().getGraphicsAllocation(1u), image->getMultiGraphicsAllocation().getGraphicsAllocation(2u));

    EXPECT_EQ(image->getAllocatedMapPtr(), nullptr);
    EXPECT_TRUE(image->getMultiGraphicsAllocation().requiresMigrations());
}

TEST_F(clCreateImageWithMultiDeviceContextTests, GivenImageCreatedWithHostPtrAndWithContextdWithMultiDeviceThenGraphicsAllocationsAreProperlyCreatedAndMapPtrIsNotSet) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    DebugManagerStateRestore dbgRestore;

    std::unique_ptr<Image> image(ImageHelperUlt<ImageUseHostPtr<Image1dDefaults>>::create(context.get()));

    EXPECT_EQ(image->getMultiGraphicsAllocation().getGraphicsAllocations().size(), 3u);
    EXPECT_NE(image->getMultiGraphicsAllocation().getGraphicsAllocation(1u), nullptr);
    EXPECT_NE(image->getMultiGraphicsAllocation().getGraphicsAllocation(2u), nullptr);
    EXPECT_NE(image->getMultiGraphicsAllocation().getGraphicsAllocation(1u), image->getMultiGraphicsAllocation().getGraphicsAllocation(2u));

    EXPECT_TRUE(image->getMultiGraphicsAllocation().requiresMigrations());

    EXPECT_EQ(image->getAllocatedMapPtr(), nullptr);
}

TEST_F(clCreateImageWithMultiDeviceContextTests, GivenContextdWithMultiDeviceFailingAllocationThenImageAllocateFails) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    {
        static_cast<MockMemoryManager *>(context->getMemoryManager())->successAllocatedGraphicsMemoryIndex = 0u;
        static_cast<MockMemoryManager *>(context->getMemoryManager())->maxSuccessAllocatedGraphicsMemoryIndex = 0u;

        std::unique_ptr<Image> image(ImageHelperUlt<ImageWithoutHostPtr>::create(context.get()));

        EXPECT_EQ(nullptr, image);
    }

    {
        static_cast<MockMemoryManager *>(context->getMemoryManager())->successAllocatedGraphicsMemoryIndex = 0u;
        static_cast<MockMemoryManager *>(context->getMemoryManager())->maxSuccessAllocatedGraphicsMemoryIndex = 1u;

        std::unique_ptr<Image> image(ImageHelperUlt<ImageWithoutHostPtr>::create(context.get()));

        EXPECT_EQ(nullptr, image);
    }
}

} // namespace ULT
