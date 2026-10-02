/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_command_list.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

constexpr auto interopExtensionName = "cl_intel_level_zero_interop";

ze_image_handle_t fakeImageHandle(uintptr_t value) {
    return reinterpret_cast<ze_image_handle_t>(value);
}

/*****************************************************************************
 * Property list encoding - pure static helpers, no device needed
 *****************************************************************************/

TEST(LeoInteropPropertyListTests, givenPropertyNamesWhenClassifyingThenOnlyImageAndKhrDeviceListsAreLists) {
    EXPECT_TRUE(MemObj::isHandleListProperty(CL_IMAGE_L0_HANDLE_INTEL));
    EXPECT_TRUE(MemObj::isHandleListProperty(CL_MEM_DEVICE_HANDLE_LIST_KHR));

    EXPECT_FALSE(MemObj::isHandleListProperty(CL_MEM_L0_HANDLE_INTEL));
    EXPECT_FALSE(MemObj::isHandleListProperty(CL_MEM_FLAGS));
    EXPECT_FALSE(MemObj::isHandleListProperty(CL_MEM_FLAGS_INTEL));
}

TEST(LeoInteropPropertyListTests, givenHandleListWhenLocatingEndThenIndexOfTerminatorIsReturned) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, 0x1000, 0x2000, 0x3000, CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    EXPECT_EQ(4u, MemObj::getHandleListEnd(props, 0));
}

TEST(LeoInteropPropertyListTests, givenEmptyHandleListWhenLocatingEndThenTerminatorFollowsNameDirectly) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    EXPECT_EQ(1u, MemObj::getHandleListEnd(props, 0));
}

TEST(LeoInteropPropertyListTests, givenSingleImageHandleWhenGettingHandleListThenOneHandleIsReturned) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, 0xabc, CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    bool found = false;
    auto handles = MemObj::getMemObjHandleList(props, CL_IMAGE_L0_HANDLE_INTEL, &found);

    EXPECT_TRUE(found);
    ASSERT_EQ(1u, handles.size());
    EXPECT_EQ(0xabcu, handles[0]);
}

TEST(LeoInteropPropertyListTests, givenMultipleImageHandlesWhenGettingHandleListThenOrderIsPreserved) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, 0x111, 0x222, 0x333, CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    bool found = false;
    auto handles = MemObj::getMemObjHandleList(props, CL_IMAGE_L0_HANDLE_INTEL, &found);

    EXPECT_TRUE(found);
    ASSERT_EQ(3u, handles.size());
    EXPECT_EQ(0x111u, handles[0]);
    EXPECT_EQ(0x222u, handles[1]);
    EXPECT_EQ(0x333u, handles[2]);
}

TEST(LeoInteropPropertyListTests, givenEmptyImageHandleListWhenGettingHandleListThenFoundWithNoHandles) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    bool found = false;
    auto handles = MemObj::getMemObjHandleList(props, CL_IMAGE_L0_HANDLE_INTEL, &found);

    EXPECT_TRUE(found);
    EXPECT_TRUE(handles.empty());
}

TEST(LeoInteropPropertyListTests, givenAbsentPropertyWhenGettingHandleListThenNotFoundAndNoHandles) {
    cl_mem_properties props[] = {CL_MEM_FLAGS, CL_MEM_READ_WRITE, 0};
    bool found = true;
    auto handles = MemObj::getMemObjHandleList(props, CL_IMAGE_L0_HANDLE_INTEL, &found);

    EXPECT_FALSE(found);
    EXPECT_TRUE(handles.empty());
}

TEST(LeoInteropPropertyListTests, givenNullPropertiesWhenGettingHandleListThenNotFoundAndNoHandles) {
    bool found = true;
    auto handles = MemObj::getMemObjHandleList(nullptr, CL_IMAGE_L0_HANDLE_INTEL, &found);

    EXPECT_FALSE(found);
    EXPECT_TRUE(handles.empty());
}

TEST(LeoInteropPropertyListTests, givenPropertiesAfterImageHandleListWhenGettingScalarPropertyThenListIsSteppedOver) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, 0x111, 0x222, CL_IMAGE_L0_HANDLE_LIST_END_INTEL,
                                 CL_MEM_FLAGS, CL_MEM_READ_ONLY,
                                 0};
    bool found = false;
    auto flags = MemObj::getMemObjProperties<cl_mem_flags>(props, CL_MEM_FLAGS, &found);

    EXPECT_TRUE(found);
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), flags);
}

TEST(LeoInteropPropertyListTests, givenPropertiesAfterKhrDeviceListWhenGettingScalarPropertyThenListIsSteppedOver) {
    cl_mem_properties props[] = {CL_MEM_DEVICE_HANDLE_LIST_KHR, 0x1, 0x2, CL_MEM_DEVICE_HANDLE_LIST_END_KHR,
                                 CL_MEM_L0_HANDLE_INTEL, 0xfeed,
                                 0};
    bool found = false;
    auto handle = MemObj::getMemObjProperties<uintptr_t>(props, CL_MEM_L0_HANDLE_INTEL, &found);

    EXPECT_TRUE(found);
    EXPECT_EQ(0xfeedu, handle);
}

TEST(LeoInteropPropertyListTests, givenImageHandleListAfterScalarPropertyWhenGettingHandleListThenHandlesAreReturned) {
    cl_mem_properties props[] = {CL_MEM_FLAGS, CL_MEM_READ_WRITE,
                                 CL_IMAGE_L0_HANDLE_INTEL, 0x777, 0x888, CL_IMAGE_L0_HANDLE_LIST_END_INTEL,
                                 0};
    bool found = false;
    auto handles = MemObj::getMemObjHandleList(props, CL_IMAGE_L0_HANDLE_INTEL, &found);

    EXPECT_TRUE(found);
    ASSERT_EQ(2u, handles.size());
    EXPECT_EQ(0x777u, handles[0]);
    EXPECT_EQ(0x888u, handles[1]);
}

TEST(LeoInteropPropertyListTests, givenBufferHandleAsScalarWhenGettingHandleListThenSingleValueIsReturned) {
    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, 0xdead, 0};
    bool found = false;
    auto handles = MemObj::getMemObjHandleList(props, CL_MEM_L0_HANDLE_INTEL, &found);

    EXPECT_TRUE(found);
    ASSERT_EQ(1u, handles.size());
    EXPECT_EQ(0xdeadu, handles[0]);
}

/*****************************************************************************
 * parseMemoryProperties - buffer/image key split and list skipping
 *****************************************************************************/

struct LeoInteropParseFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        leoContext = std::make_unique<Context>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        Test<OclFixture>::TearDown();
    }

    bool parse(const cl_mem_properties *properties, ClMemoryPropertiesHelper::ObjType objType) {
        flags = 0;
        flagsIntel = 0;
        allocFlags = 0;
        memoryProperties = {};
        return ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel,
                                                               allocFlags, objType, *leoContext);
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<Context> leoContext;
    MemoryProperties memoryProperties{};
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocFlags = 0;
};

TEST_F(LeoInteropParseFixture, givenBufferHandlePropertyWhenParsingForBufferThenItIsAccepted) {
    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, 0x1000, 0};
    EXPECT_TRUE(parse(props, ClMemoryPropertiesHelper::ObjType::buffer));
}

TEST_F(LeoInteropParseFixture, givenBufferHandlePropertyWhenParsingForImageThenItIsRejected) {
    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, 0x1000, 0};
    EXPECT_FALSE(parse(props, ClMemoryPropertiesHelper::ObjType::image));
}

TEST_F(LeoInteropParseFixture, givenBufferHandlePropertyWhenParsingForUnknownObjectThenItIsRejected) {
    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, 0x1000, 0};
    EXPECT_FALSE(parse(props, ClMemoryPropertiesHelper::ObjType::unknown));
}

TEST_F(LeoInteropParseFixture, givenImageHandleListWhenParsingForImageThenItIsAccepted) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, 0x1000, CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    EXPECT_TRUE(parse(props, ClMemoryPropertiesHelper::ObjType::image));
}

TEST_F(LeoInteropParseFixture, givenImageHandleListWhenParsingForBufferThenItIsRejected) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, 0x1000, CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    EXPECT_FALSE(parse(props, ClMemoryPropertiesHelper::ObjType::buffer));
}

TEST_F(LeoInteropParseFixture, givenImageHandleListWhenParsingForUnknownObjectThenItIsRejected) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, 0x1000, CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    EXPECT_FALSE(parse(props, ClMemoryPropertiesHelper::ObjType::unknown));
}

TEST_F(LeoInteropParseFixture, givenMultipleImageHandlesFollowedByFlagsWhenParsingThenListIsSkippedAndFlagsAreParsed) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, 0x1000, 0x2000, 0x3000, CL_IMAGE_L0_HANDLE_LIST_END_INTEL,
                                 CL_MEM_FLAGS, CL_MEM_READ_ONLY,
                                 0};
    ASSERT_TRUE(parse(props, ClMemoryPropertiesHelper::ObjType::image));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), flags);
}

TEST_F(LeoInteropParseFixture, givenMultiEntryKhrDeviceListFollowedByFlagsWhenParsingThenListIsSkippedAndFlagsAreParsed) {
    cl_mem_properties props[] = {CL_MEM_DEVICE_HANDLE_LIST_KHR, 0x1, 0x2, CL_MEM_DEVICE_HANDLE_LIST_END_KHR,
                                 CL_MEM_FLAGS, CL_MEM_WRITE_ONLY,
                                 0};
    ASSERT_TRUE(parse(props, ClMemoryPropertiesHelper::ObjType::buffer));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_WRITE_ONLY), flags);
}

TEST_F(LeoInteropParseFixture, givenBufferHandleFollowedByFlagsWhenParsingThenBothAreHandled) {
    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, 0x1000,
                                 CL_MEM_FLAGS, CL_MEM_READ_WRITE,
                                 0};
    ASSERT_TRUE(parse(props, ClMemoryPropertiesHelper::ObjType::buffer));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), flags);
}

TEST_F(LeoInteropParseFixture, givenUnknownPropertyWhenParsingThenItIsRejected) {
    cl_mem_properties props[] = {0xdead, 0x1000, 0};
    EXPECT_FALSE(parse(props, ClMemoryPropertiesHelper::ObjType::buffer));
}

/*****************************************************************************
 * Buffer import and query - CL_MEM_L0_HANDLE_INTEL stays scalar
 *****************************************************************************/

struct LeoInteropBufferFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
    uint64_t importedStorage = 0u;
};

TEST_F(LeoInteropBufferFixture, givenImportedUsmPointerWhenCreatingBufferThenHandleQueryReturnsThatPointer) {
    void *usmPtr = &importedStorage;
    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(usmPtr), 0};

    cl_int errcode = CL_INVALID_VALUE;
    auto buffer = clCreateBufferWithProperties(leoContext.get(), props, 0, sizeof(importedStorage), nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, buffer);

    void *queried = nullptr;
    size_t queriedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetMemObjectInfo(buffer, CL_MEM_L0_HANDLE_INTEL, sizeof(queried), &queried, &queriedSize));
    EXPECT_EQ(usmPtr, queried);
    EXPECT_EQ(sizeof(void *), queriedSize);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(LeoInteropBufferFixture, givenImportedUsmPointerWhenReleasingBufferThenPointerIsNotFreed) {
    void *usmPtr = &importedStorage;
    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(usmPtr), 0};

    auto buffer = clCreateBufferWithProperties(leoContext.get(), props, 0, sizeof(importedStorage), nullptr, nullptr);
    ASSERT_NE(nullptr, buffer);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());
}

TEST_F(LeoInteropBufferFixture, givenImportedBufferWhenQueryingMemPropertiesThenScalarPropertyRoundTrips) {
    void *usmPtr = &importedStorage;
    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(usmPtr), 0};

    auto buffer = clCreateBufferWithProperties(leoContext.get(), props, 0, sizeof(importedStorage), nullptr, nullptr);
    ASSERT_NE(nullptr, buffer);

    cl_mem_properties stored[8] = {};
    size_t storedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetMemObjectInfo(buffer, CL_MEM_PROPERTIES, sizeof(stored), stored, &storedSize));

    ASSERT_EQ(3u * sizeof(cl_mem_properties), storedSize);
    EXPECT_EQ(static_cast<cl_mem_properties>(CL_MEM_L0_HANDLE_INTEL), stored[0]);
    EXPECT_EQ(reinterpret_cast<cl_mem_properties>(usmPtr), stored[1]);
    EXPECT_EQ(0u, stored[2]);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(LeoInteropBufferFixture, givenImageHandleKeyWhenCreatingBufferThenInvalidValueIsReturned) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(&importedStorage),
                                 CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};

    cl_int errcode = CL_SUCCESS;
    auto buffer = clCreateBufferWithProperties(leoContext.get(), props, 0, sizeof(importedStorage), nullptr, &errcode);
    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST_F(LeoInteropBufferFixture, givenKhrDeviceListBeforeBufferHandleWhenCreatingBufferThenHandleIsStillImported) {
    void *usmPtr = &importedStorage;
    cl_mem_properties props[] = {CL_MEM_DEVICE_HANDLE_LIST_KHR, reinterpret_cast<cl_mem_properties>(clDevice),
                                 CL_MEM_DEVICE_HANDLE_LIST_END_KHR,
                                 CL_MEM_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(usmPtr),
                                 0};

    cl_int errcode = CL_INVALID_VALUE;
    auto buffer = clCreateBufferWithProperties(leoContext.get(), props, 0, sizeof(importedStorage), nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, buffer);

    void *queried = nullptr;
    EXPECT_EQ(CL_SUCCESS, clGetMemObjectInfo(buffer, CL_MEM_L0_HANDLE_INTEL, sizeof(queried), &queried, nullptr));
    EXPECT_EQ(usmPtr, queried);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());
}

/*****************************************************************************
 * Image import and query - CL_IMAGE_L0_HANDLE_INTEL is a per-device list
 *****************************************************************************/

struct LeoInteropImageFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        if (!clDevice->getHardwareInfo().capabilityTable.supportsImages) {
            GTEST_SKIP() << "Product does not support images";
        }
        cl_device_id clDeviceId = clDevice;
        leoContext = std::make_unique<Context>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDeviceId, true);
        ASSERT_EQ(CL_SUCCESS, leoContext->initialize());

        imageFormat.image_channel_order = CL_RGBA;
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = 16;
        imageDesc.image_height = 16;
    }

    void TearDown() override {
        leoContext.reset();
        Test<OclFixture>::TearDown();
    }

    cl_mem createPlainImage() {
        cl_int errcode = CL_INVALID_VALUE;
        auto image = clCreateImage(leoContext.get(), CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &errcode);
        EXPECT_EQ(CL_SUCCESS, errcode);
        return image;
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<Context> leoContext;
    cl_image_format imageFormat{};
    cl_image_desc imageDesc{};
};

TEST_F(LeoInteropImageFixture, givenImageWhenQueryingImageInfoThenSingleL0HandleIsReturned) {
    auto image = createPlainImage();
    ASSERT_NE(nullptr, image);

    ze_image_handle_t queried = nullptr;
    size_t queriedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetImageInfo(image, CL_IMAGE_L0_HANDLE_INTEL, sizeof(queried), &queried, &queriedSize));
    EXPECT_EQ(sizeof(ze_image_handle_t), queriedSize);
    EXPECT_EQ(castToObject<Image>(image)->getL0Handle(), queried);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(image));
}

TEST_F(LeoInteropImageFixture, givenImageWhenQueryingSizeOnlyThenOneHandlePerRootDeviceIsReported) {
    auto image = createPlainImage();
    ASSERT_NE(nullptr, image);

    size_t queriedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetImageInfo(image, CL_IMAGE_L0_HANDLE_INTEL, 0, nullptr, &queriedSize));
    EXPECT_EQ(leoContext->getRootDeviceIndices().size() * sizeof(ze_image_handle_t), queriedSize);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(image));
}

TEST_F(LeoInteropImageFixture, givenImageWhenQueryingBufferHandleThroughMemObjectInfoThenInvalidValueIsReturned) {
    auto image = createPlainImage();
    ASSERT_NE(nullptr, image);

    void *queried = nullptr;
    EXPECT_EQ(CL_INVALID_VALUE, clGetMemObjectInfo(image, CL_MEM_L0_HANDLE_INTEL, sizeof(queried), &queried, nullptr));

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(image));
}

TEST_F(LeoInteropImageFixture, givenBufferWhenQueryingImageHandleThroughImageInfoThenInvalidMemObjectIsReturned) {
    uint64_t storage = 0;
    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(&storage), 0};
    auto buffer = clCreateBufferWithProperties(leoContext.get(), props, 0, sizeof(storage), nullptr, nullptr);
    ASSERT_NE(nullptr, buffer);

    ze_image_handle_t queried = nullptr;
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clGetImageInfo(buffer, CL_IMAGE_L0_HANDLE_INTEL, sizeof(queried), &queried, nullptr));

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(LeoInteropImageFixture, givenImportedL0ImageWhenCreatingImageThenHandleIsAdoptedAndQueryable) {
    auto sourceImage = createPlainImage();
    ASSERT_NE(nullptr, sourceImage);
    auto sourceHandle = castToObject<Image>(sourceImage)->getL0Handle();

    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(sourceHandle),
                                 CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    cl_int errcode = CL_INVALID_VALUE;
    auto importedImage = clCreateImageWithProperties(leoContext.get(), props, CL_MEM_READ_WRITE,
                                                     &imageFormat, &imageDesc, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, importedImage);

    ze_image_handle_t queried = nullptr;
    EXPECT_EQ(CL_SUCCESS, clGetImageInfo(importedImage, CL_IMAGE_L0_HANDLE_INTEL, sizeof(queried), &queried, nullptr));
    EXPECT_EQ(sourceHandle, queried);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(importedImage));

    size_t width = 0;
    EXPECT_EQ(CL_SUCCESS, clGetImageInfo(sourceImage, CL_IMAGE_WIDTH, sizeof(width), &width, nullptr));
    EXPECT_EQ(imageDesc.image_width, width);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(sourceImage));
}

TEST_F(LeoInteropImageFixture, givenImportedL0ImageWhenQueryingMemPropertiesThenHandleListRoundTripsWithTerminator) {
    auto sourceImage = createPlainImage();
    ASSERT_NE(nullptr, sourceImage);
    auto sourceHandle = castToObject<Image>(sourceImage)->getL0Handle();

    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(sourceHandle),
                                 CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    auto importedImage = clCreateImageWithProperties(leoContext.get(), props, CL_MEM_READ_WRITE,
                                                     &imageFormat, &imageDesc, nullptr, nullptr);
    ASSERT_NE(nullptr, importedImage);

    cl_mem_properties stored[8] = {};
    size_t storedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetMemObjectInfo(importedImage, CL_MEM_PROPERTIES, sizeof(stored), stored, &storedSize));

    ASSERT_EQ(4u * sizeof(cl_mem_properties), storedSize);
    EXPECT_EQ(static_cast<cl_mem_properties>(CL_IMAGE_L0_HANDLE_INTEL), stored[0]);
    EXPECT_EQ(reinterpret_cast<cl_mem_properties>(sourceHandle), stored[1]);
    EXPECT_EQ(static_cast<cl_mem_properties>(CL_IMAGE_L0_HANDLE_LIST_END_INTEL), stored[2]);
    EXPECT_EQ(0u, stored[3]);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(importedImage));
    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(sourceImage));
}

TEST_F(LeoInteropImageFixture, givenBufferHandleKeyWhenCreatingImageThenInvalidPropertyIsReturned) {
    auto sourceImage = createPlainImage();
    ASSERT_NE(nullptr, sourceImage);
    auto sourceHandle = castToObject<Image>(sourceImage)->getL0Handle();

    cl_mem_properties props[] = {CL_MEM_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(sourceHandle), 0};
    cl_int errcode = CL_SUCCESS;
    auto importedImage = clCreateImageWithProperties(leoContext.get(), props, CL_MEM_READ_WRITE,
                                                     &imageFormat, &imageDesc, nullptr, &errcode);
    EXPECT_EQ(nullptr, importedImage);
    EXPECT_EQ(CL_INVALID_PROPERTY, errcode);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(sourceImage));
}

TEST_F(LeoInteropImageFixture, givenEmptyHandleListWhenCreatingImageThenInvalidPropertyIsReturned) {
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL, CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    cl_int errcode = CL_SUCCESS;
    auto importedImage = clCreateImageWithProperties(leoContext.get(), props, CL_MEM_READ_WRITE,
                                                     &imageFormat, &imageDesc, nullptr, &errcode);
    EXPECT_EQ(nullptr, importedImage);
    EXPECT_EQ(CL_INVALID_PROPERTY, errcode);
}

TEST_F(LeoInteropImageFixture, givenMoreHandlesThanRootDevicesWhenCreatingImageThenInvalidPropertyIsReturned) {
    auto sourceImage = createPlainImage();
    ASSERT_NE(nullptr, sourceImage);
    auto sourceHandle = castToObject<Image>(sourceImage)->getL0Handle();

    ASSERT_EQ(1u, leoContext->getRootDeviceIndices().size());
    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL,
                                 reinterpret_cast<cl_mem_properties>(sourceHandle),
                                 reinterpret_cast<cl_mem_properties>(sourceHandle),
                                 CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    cl_int errcode = CL_SUCCESS;
    auto importedImage = clCreateImageWithProperties(leoContext.get(), props, CL_MEM_READ_WRITE,
                                                     &imageFormat, &imageDesc, nullptr, &errcode);
    EXPECT_EQ(nullptr, importedImage);
    EXPECT_EQ(CL_INVALID_PROPERTY, errcode);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(sourceImage));
}

/*****************************************************************************
 * Multi device image handle lists
 *****************************************************************************/

struct LeoInteropMultiDeviceFixture : public L0::ult::MultiDeviceFixture {
    void setUp() {
        L0::ult::MultiDeviceFixture::setUp();
        platform = std::make_unique<Platform>(driverHandle->toHandle());

        for (auto &clDevice : platform->getDevices()) {
            clDeviceIds.push_back(clDevice.get());
        }
        leoContext = std::make_unique<Context>(nullptr, this->L0::ult::MultiDeviceFixture::context->toHandle(),
                                               static_cast<cl_uint>(clDeviceIds.size()), clDeviceIds.data(), true);
    }

    void tearDown() {
        leoContext.reset();
        clDeviceIds.clear();
        platform.reset();
        L0::ult::MultiDeviceFixture::tearDown();
    }

    std::unique_ptr<Image> createImportedImage(const std::vector<ze_image_handle_t> &handles) {
        MemoryProperties properties{};
        cl_image_format format{CL_R, CL_UNORM_INT8};
        auto image = std::make_unique<Image>(leoContext.get(), properties, CL_MEM_READ_WRITE, handles[0],
                                             nullptr, nullptr, true, format, nullptr);

        const auto &rootDeviceIndices = leoContext->getRootDeviceIndices();
        image->setOwnerRootDeviceIndex(rootDeviceIndices[0]);
        for (size_t i = 1; i < handles.size(); ++i) {
            image->addPerDeviceHandle(rootDeviceIndices[i], handles[i]);
        }
        return image;
    }

    std::unique_ptr<Platform> platform;
    std::vector<cl_device_id> clDeviceIds;
    std::unique_ptr<Context> leoContext;
};

using LeoInteropMultiDeviceTests = Test<LeoInteropMultiDeviceFixture>;

TEST_F(LeoInteropMultiDeviceTests, givenHandlePerRootDeviceWhenGettingHandlesThenTheyFollowRootDeviceIndexOrder) {
    const auto &rootDeviceIndices = leoContext->getRootDeviceIndices();
    ASSERT_LT(1u, rootDeviceIndices.size());

    std::vector<ze_image_handle_t> handles;
    for (size_t i = 0; i < rootDeviceIndices.size(); ++i) {
        handles.push_back(fakeImageHandle(0x1000 + i));
    }

    auto image = createImportedImage(handles);
    auto queried = image->getL0Handles();

    ASSERT_EQ(rootDeviceIndices.size(), queried.size());
    for (size_t i = 0; i < queried.size(); ++i) {
        EXPECT_EQ(handles[i], queried[i]) << "entry " << i;
        EXPECT_EQ(handles[i], image->getL0Handle(rootDeviceIndices[i])) << "root device index " << rootDeviceIndices[i];
    }
}

TEST_F(LeoInteropMultiDeviceTests, givenOnlyOwnerHandleWhenGettingHandlesThenEveryEntryFallsBackToIt) {
    const auto &rootDeviceIndices = leoContext->getRootDeviceIndices();
    ASSERT_LT(1u, rootDeviceIndices.size());

    auto ownerHandle = fakeImageHandle(0x2000);
    auto image = createImportedImage({ownerHandle});

    EXPECT_FALSE(image->isMultiDevice());

    auto queried = image->getL0Handles();
    ASSERT_EQ(rootDeviceIndices.size(), queried.size());
    for (auto handle : queried) {
        EXPECT_EQ(ownerHandle, handle);
    }
}

TEST_F(LeoInteropMultiDeviceTests, givenPartialPerDeviceHandlesWhenGettingHandlesThenMissingEntriesFallBackToOwner) {
    const auto &rootDeviceIndices = leoContext->getRootDeviceIndices();
    ASSERT_LT(2u, rootDeviceIndices.size());

    auto ownerHandle = fakeImageHandle(0x3000);
    auto secondHandle = fakeImageHandle(0x3001);
    auto image = createImportedImage({ownerHandle, secondHandle});

    EXPECT_TRUE(image->isMultiDevice());

    auto queried = image->getL0Handles();
    ASSERT_EQ(rootDeviceIndices.size(), queried.size());
    EXPECT_EQ(ownerHandle, queried[0]);
    EXPECT_EQ(secondHandle, queried[1]);
    for (size_t i = 2; i < queried.size(); ++i) {
        EXPECT_EQ(ownerHandle, queried[i]) << "entry " << i;
    }
}

TEST_F(LeoInteropMultiDeviceTests, givenFewerHandlesThanRootDevicesWhenCreatingImageThenInvalidPropertyIsReturned) {
    if (!platform->getDevices()[0]->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP() << "Product does not support images";
    }

    const auto &rootDeviceIndices = leoContext->getRootDeviceIndices();
    ASSERT_LT(1u, rootDeviceIndices.size());

    // The handle count is checked before any handle is dereferenced, so fake handles are sufficient here.
    cl_image_format format{CL_R, CL_UNORM_INT8};
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = 4;
    desc.image_height = 4;

    cl_mem_properties props[] = {CL_IMAGE_L0_HANDLE_INTEL,
                                 reinterpret_cast<cl_mem_properties>(fakeImageHandle(0x4000)),
                                 CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    cl_int errcode = CL_SUCCESS;
    auto importedImage = clCreateImageWithProperties(leoContext.get(), props, CL_MEM_READ_WRITE,
                                                     &format, &desc, nullptr, &errcode);
    EXPECT_EQ(nullptr, importedImage);
    EXPECT_EQ(CL_INVALID_PROPERTY, errcode);
}

TEST_F(LeoInteropMultiDeviceTests, givenMultiDeviceContextWhenGettingHandleListFromPropertiesThenPositionalOrderMatchesRootDeviceIndices) {
    const auto &rootDeviceIndices = leoContext->getRootDeviceIndices();
    ASSERT_LT(1u, rootDeviceIndices.size());

    std::vector<cl_mem_properties> props{CL_IMAGE_L0_HANDLE_INTEL};
    for (size_t i = 0; i < rootDeviceIndices.size(); ++i) {
        props.push_back(static_cast<cl_mem_properties>(0x1000 + i));
    }
    props.push_back(CL_IMAGE_L0_HANDLE_LIST_END_INTEL);
    props.push_back(0);

    bool found = false;
    auto handles = MemObj::getMemObjHandleList(props.data(), CL_IMAGE_L0_HANDLE_INTEL, &found);

    ASSERT_TRUE(found);
    ASSERT_EQ(rootDeviceIndices.size(), handles.size());
    for (size_t i = 0; i < handles.size(); ++i) {
        EXPECT_EQ(0x1000u + i, handles[i]) << "entry " << i;
    }
}

/*****************************************************************************
 * Command queue import - every input property except profiling is ignored
 *****************************************************************************/

struct LeoInteropCommandQueueFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        clDeviceId = clDevice;
        leoContext = std::make_unique<Context>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        Test<OclFixture>::TearDown();
    }

    cl_command_queue importQueue(const cl_queue_properties *properties, cl_int &errcode) {
        errcode = CL_INVALID_VALUE;
        return clCreateCommandQueueWithProperties(leoContext.get(), clDeviceId, properties, &errcode);
    }

    cl_command_queue_properties queryProperties(cl_command_queue queue) {
        cl_command_queue_properties reported = std::numeric_limits<cl_command_queue_properties>::max();
        EXPECT_EQ(CL_SUCCESS, clGetCommandQueueInfo(queue, CL_QUEUE_PROPERTIES, sizeof(reported), &reported, nullptr));
        return reported;
    }

    ClDevice *clDevice = nullptr;
    cl_device_id clDeviceId = nullptr;
    CapturingCommandList capturingCmdList{};
    std::unique_ptr<Context> leoContext;
};

TEST_F(LeoInteropCommandQueueFixture, givenImportedCmdListWhenCreatingQueueThenHandleIsAdopted) {
    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   0};
    cl_int errcode = CL_INVALID_VALUE;
    auto queue = importQueue(props, errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    ze_command_list_handle_t queried = nullptr;
    size_t queriedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetCommandQueueInfo(queue, CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                                sizeof(queried), &queried, &queriedSize));
    EXPECT_EQ(capturingCmdList.toHandle(), queried);
    EXPECT_EQ(sizeof(ze_command_list_handle_t), queriedSize);

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(queue));
}

TEST_F(LeoInteropCommandQueueFixture, givenImportedCmdListWithProfilingWhenCreatingQueueThenProfilingIsKept) {
    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE,
                                   0};
    cl_int errcode = CL_INVALID_VALUE;
    auto queue = importQueue(props, errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    // The imported command list is out-of-order, so that bit is reported alongside profiling.
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE),
              queryProperties(queue));
    EXPECT_TRUE(castToObject<CommandQueue>(queue)->isProfilingEnabled());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(queue));
}

TEST_F(LeoInteropCommandQueueFixture, givenImportedCmdListWithoutProfilingWhenCreatingQueueThenProfilingIsDisabled) {
    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                   0};
    cl_int errcode = CL_INVALID_VALUE;
    auto queue = importQueue(props, errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    // Profiling was not requested; the reported out-of-order bit comes from the imported command list.
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE), queryProperties(queue));
    EXPECT_FALSE(castToObject<CommandQueue>(queue)->isProfilingEnabled());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(queue));
}

TEST_F(LeoInteropCommandQueueFixture, givenOutOfOrderCmdListAndNoOutOfOrderPropertyWhenCreatingQueueThenOutOfOrderIsStillReported) {
    ASSERT_FALSE(capturingCmdList.isInOrderExecutionEnabled());

    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   0};
    cl_int errcode = CL_INVALID_VALUE;
    auto queue = importQueue(props, errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    // CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE was not requested, but the queue behaves out-of-order
    // because the imported command list does, so the query has to report it.
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE), queryProperties(queue));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(queue));
}

TEST_F(LeoInteropCommandQueueFixture, givenInOrderRequestedCmdListWithDeferredInitializationWhenCreatingQueueWithOutOfOrderPropertyThenOutOfOrderIsNotReported) {
    capturingCmdList.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    ASSERT_FALSE(capturingCmdList.isInOrderExecutionEnabled());

    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                   0};
    cl_int errcode = CL_INVALID_VALUE;
    auto queue = importQueue(props, errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE), queryProperties(queue));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(queue));
}

TEST_F(LeoInteropCommandQueueFixture, givenImportedCmdListWithManyPropertiesWhenCreatingQueueThenOnlyProfilingAndListOrderingAreReported) {
    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                   CL_QUEUE_FAMILY_INTEL, 7,
                                   CL_QUEUE_INDEX_INTEL, 9,
                                   CL_QUEUE_THROTTLE_KHR, CL_QUEUE_THROTTLE_LOW_KHR,
                                   CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_HIGH_KHR,
                                   CL_QUEUE_SLICE_COUNT_INTEL, 3,
                                   0};
    cl_int errcode = CL_INVALID_VALUE;
    auto queue = importQueue(props, errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE),
              queryProperties(queue));

    cl_uint reportedFamily = 0xdead;
    cl_uint reportedIndex = 0xdead;
    EXPECT_EQ(CL_SUCCESS, clGetCommandQueueInfo(queue, CL_QUEUE_FAMILY_INTEL, sizeof(reportedFamily), &reportedFamily, nullptr));
    EXPECT_EQ(CL_SUCCESS, clGetCommandQueueInfo(queue, CL_QUEUE_INDEX_INTEL, sizeof(reportedIndex), &reportedIndex, nullptr));
    EXPECT_EQ(0u, reportedFamily);
    EXPECT_EQ(0u, reportedIndex);

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(queue));
}

TEST_F(LeoInteropCommandQueueFixture, givenImportedCmdListWithUnknownPropertyWhenCreatingQueueThenInvalidValueIsReturned) {
    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   0xdead, 0x1234,
                                   0};
    cl_int errcode = CL_SUCCESS;
    auto queue = importQueue(props, errcode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST_F(LeoInteropCommandQueueFixture, givenImportedCmdListWithOnDeviceQueueWhenCreatingQueueThenInvalidValueIsReturned) {
    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE,
                                   0};
    cl_int errcode = CL_SUCCESS;
    auto queue = importQueue(props, errcode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST_F(LeoInteropCommandQueueFixture, givenImportedCmdListWithOnDeviceOutOfOrderQueueWhenCreatingQueueThenInvalidQueuePropertiesIsReturned) {
    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                   0};
    cl_int errcode = CL_SUCCESS;
    auto queue = importQueue(props, errcode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, errcode);
}

TEST_F(LeoInteropCommandQueueFixture, givenImportedCmdListWhenQueryingPropertiesArrayThenSanitizedArrayIsReported) {
    cl_queue_properties props[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL,
                                   reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()),
                                   CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE,
                                   CL_QUEUE_FAMILY_INTEL, 7,
                                   0};
    cl_int errcode = CL_INVALID_VALUE;
    auto queue = importQueue(props, errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    cl_queue_properties stored[8] = {};
    size_t storedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetCommandQueueInfo(queue, CL_QUEUE_PROPERTIES_ARRAY, sizeof(stored), stored, &storedSize));

    ASSERT_EQ(5u * sizeof(cl_queue_properties), storedSize);
    EXPECT_EQ(static_cast<cl_queue_properties>(CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL), stored[0]);
    EXPECT_EQ(reinterpret_cast<cl_queue_properties>(capturingCmdList.toHandle()), stored[1]);
    EXPECT_EQ(static_cast<cl_queue_properties>(CL_QUEUE_PROPERTIES), stored[2]);
    EXPECT_EQ(static_cast<cl_queue_properties>(CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE), stored[3]);
    EXPECT_EQ(0u, stored[4]);

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(queue));
}

TEST_F(LeoInteropCommandQueueFixture, givenNoImportedCmdListAndUnknownPropertyWhenCreatingQueueThenInvalidValueIsReturned) {
    cl_queue_properties props[] = {0xdead, 0x1234, 0};
    cl_int errcode = CL_SUCCESS;
    auto queue = importQueue(props, errcode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST_F(LeoInteropCommandQueueFixture, givenNoImportedCmdListAndOnDeviceQueueWhenCreatingQueueThenItIsStillRejected) {
    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES,
                                   CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                   0};
    cl_int errcode = CL_SUCCESS;
    auto queue = importQueue(props, errcode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, errcode);
}

/*****************************************************************************
 * Extension advertisement and the remaining handle queries
 *****************************************************************************/

struct LeoInteropAdvertisementFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        clDeviceId = clDevice;
    }

    std::string getDeviceExtensions() {
        size_t size = 0;
        EXPECT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, CL_DEVICE_EXTENSIONS, 0, nullptr, &size));
        std::vector<char> extensions(size);
        EXPECT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, CL_DEVICE_EXTENSIONS, size, extensions.data(), nullptr));
        return std::string(extensions.data());
    }

    std::string getPlatformExtensions() {
        size_t size = 0;
        EXPECT_EQ(CL_SUCCESS, clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS, 0, nullptr, &size));
        std::vector<char> extensions(size);
        EXPECT_EQ(CL_SUCCESS, clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS, size, extensions.data(), nullptr));
        return std::string(extensions.data());
    }

    ClDevice *clDevice = nullptr;
    cl_device_id clDeviceId = nullptr;
};

using LeoInteropAdvertisementTests = LeoInteropAdvertisementFixture;

TEST_F(LeoInteropAdvertisementTests, whenQueryingDeviceExtensionsThenInteropExtensionIsReported) {
    const auto extensions = getDeviceExtensions();
    EXPECT_NE(std::string::npos, extensions.find(interopExtensionName)) << extensions;
}

TEST_F(LeoInteropAdvertisementTests, whenQueryingPlatformExtensionsThenInteropExtensionIsReported) {
    const auto extensions = getPlatformExtensions();
    EXPECT_NE(std::string::npos, extensions.find(interopExtensionName)) << extensions;
}

TEST_F(LeoInteropAdvertisementTests, whenQueryingExtensionsWithVersionThenInteropExtensionIsReportedAsOneZeroZero) {
    size_t size = 0;
    ASSERT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, CL_DEVICE_EXTENSIONS_WITH_VERSION, 0, nullptr, &size));
    std::vector<cl_name_version> extensions(size / sizeof(cl_name_version));
    ASSERT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, CL_DEVICE_EXTENSIONS_WITH_VERSION, size, extensions.data(), nullptr));

    bool found = false;
    for (const auto &extension : extensions) {
        if (strcmp(extension.name, interopExtensionName) == 0) {
            found = true;
            EXPECT_EQ(static_cast<cl_version>(CL_MAKE_VERSION(1, 0, 0)), extension.version);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LeoInteropAdvertisementTests, whenQueryingDeviceHandleThenL0DeviceHandleIsReturned) {
    ze_device_handle_t queried = nullptr;
    size_t queriedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, CL_DEVICE_L0_HANDLE_INTEL, sizeof(queried), &queried, &queriedSize));
    EXPECT_EQ(clDevice->getL0Handle(), queried);
    EXPECT_EQ(sizeof(ze_device_handle_t), queriedSize);
}

TEST_F(LeoInteropAdvertisementTests, whenQueryingPlatformDriverHandleThenL0DriverHandleIsReturned) {
    ze_driver_handle_t queried = nullptr;
    size_t queriedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetPlatformInfo(platform, CL_PLATFORM_L0_DRIVER_HANDLE_INTEL,
                                            sizeof(queried), &queried, &queriedSize));
    EXPECT_EQ(driverHandle->toHandle(), queried);
    EXPECT_EQ(sizeof(ze_driver_handle_t), queriedSize);
}

TEST_F(LeoInteropAdvertisementTests, givenImportedL0ContextWhenQueryingContextHandleThenSameHandleIsReturned) {
    auto l0ContextHandle = this->L0::ult::DeviceFixture::context->toHandle();
    Context leoContext(nullptr, l0ContextHandle, 1, &clDeviceId, true);

    ze_context_handle_t queried = nullptr;
    size_t queriedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetContextInfo(&leoContext, CL_CONTEXT_L0_HANDLE_INTEL,
                                           sizeof(queried), &queried, &queriedSize));
    EXPECT_EQ(l0ContextHandle, queried);
    EXPECT_EQ(sizeof(ze_context_handle_t), queriedSize);
}

TEST_F(LeoInteropAdvertisementTests, givenImportedL0ContextWhenCreatingContextThroughApiThenHandleIsAdoptedAndNotDestroyed) {
    auto l0ContextHandle = this->L0::ult::DeviceFixture::context->toHandle();
    cl_context_properties props[] = {CL_CONTEXT_L0_HANDLE_INTEL,
                                     reinterpret_cast<cl_context_properties>(l0ContextHandle),
                                     0};

    cl_int errcode = CL_INVALID_VALUE;
    auto clContext = clCreateContext(props, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, clContext);

    ze_context_handle_t queried = nullptr;
    EXPECT_EQ(CL_SUCCESS, clGetContextInfo(clContext, CL_CONTEXT_L0_HANDLE_INTEL, sizeof(queried), &queried, nullptr));
    EXPECT_EQ(l0ContextHandle, queried);

    EXPECT_EQ(CL_SUCCESS, clReleaseContext(clContext));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
