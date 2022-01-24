/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/mem_obj/image_compression_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using XeHpSdvImageTests = ::testing::Test;

XEHPTEST_F(XeHpSdvImageTests, givenContextTypeDefaultWhenImageIsWritableAndOnlyOneTileIsAvailableThenRemainFlagsToTrue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(1);
    initPlatform();
    EXPECT_EQ(0u, platform()->getClDevice(0)->getNumGenericSubDevices());
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context(platform()->getClDevice(0));
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    surfaceState.setDisableSupportForMultiGpuPartialWrites(false);
    imageHw->appendSurfaceStateParams(&surfaceState, context.getDevice(0)->getRootDeviceIndex(), true);

    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvImageTests, givenContextTypeDefaultWhenImageIsWritableThenFlipPartialFlagsToFalse) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    initPlatform();

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context(platform()->getClDevice(0));
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    imageHw->appendSurfaceStateParams(&surfaceState, context.getDevice(0)->getRootDeviceIndex(), true);

    EXPECT_FALSE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_FALSE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvImageTests, givenDebugFlagForMultiTileSupportWhenSurfaceStateIsProgrammedThenItHasDesiredValues) {
    DebugManagerStateRestore restorer;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        CL_MEM_READ_ONLY, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;

    DebugManager.flags.ForceMultiGpuAtomics.set(0);
    DebugManager.flags.ForceMultiGpuPartialWrites.set(0);

    imageHw->appendSurfaceStateParams(&surfaceState, context.getDevice(0)->getRootDeviceIndex(), true);

    EXPECT_EQ(0u, surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_EQ(0u, surfaceState.getDisableSupportForMultiGpuPartialWrites());

    DebugManager.flags.ForceMultiGpuAtomics.set(1);
    DebugManager.flags.ForceMultiGpuPartialWrites.set(1);

    imageHw->appendSurfaceStateParams(&surfaceState, context.getDevice(0)->getRootDeviceIndex(), true);

    EXPECT_EQ(1u, surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_EQ(1u, surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvImageTests, givenContextTypeUnrestrictiveWhenImageIsWritableThenFlipPartialFlagsToFalse) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    initPlatform();

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context(platform()->getClDevice(0));
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;

    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    imageHw->appendSurfaceStateParams(&surfaceState, context.getDevice(0)->getRootDeviceIndex(), true);

    EXPECT_FALSE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_FALSE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvImageTests, givenContextTypeDefaultWhenImageIsNotWritableThenRemainPartialFlagsToTrue) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        CL_MEM_READ_ONLY, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    surfaceState.setDisableSupportForMultiGpuPartialWrites(false);
    imageHw->appendSurfaceStateParams(&surfaceState, context.getDevice(0)->getRootDeviceIndex(), true);

    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvImageTests, givenContextTypeSpecializedWhenImageIsWritableThenRemainPartialFlagsToTrue) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;

    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    surfaceState.setDisableSupportForMultiGpuPartialWrites(false);
    imageHw->appendSurfaceStateParams(&surfaceState, context.getDevice(0)->getRootDeviceIndex(), true);

    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

struct MultiGpuGlobalAtomicsImageTest : public XeHpSdvImageTests,
                                        public ::testing::WithParamInterface<std::tuple<unsigned int, unsigned int, bool, bool>> {
};

XEHPTEST_P(MultiGpuGlobalAtomicsImageTest, givenAppendSurfaceStateParamCalledThenDisableSupportForMultiGpuAtomicsIsSetCorrectly) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    unsigned int numAvailableSubDevices, memFlags;
    bool useGlobalAtomics, enableMultiGpuAtomicsOptimization;
    std::tie(numAvailableSubDevices, memFlags, useGlobalAtomics, enableMultiGpuAtomicsOptimization) = GetParam();

    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableMultiGpuAtomicsOptimization.set(enableMultiGpuAtomicsOptimization);

    UltClDeviceFactory deviceFactory{1, 2};

    ClDeviceVector deviceVector;

    for (auto i = 0u; i < numAvailableSubDevices; i++) {
        deviceVector.push_back(deviceFactory.subDevices[i]);
    }
    if (deviceVector.empty()) {
        deviceVector.push_back(deviceFactory.rootDevices[0]);
    }

    MockContext context(deviceVector);

    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        memFlags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(memFlags, 0, 0, &context.getDevice(0)->getDevice()),
        memFlags, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    surfaceState.setDisableSupportForMultiGpuPartialWrites(false);
    imageHw->appendSurfaceStateParams(&surfaceState, context.getDevice(0)->getRootDeviceIndex(), useGlobalAtomics);

    bool enableGlobalAtomics = numAvailableSubDevices != 1u;
    if (enableMultiGpuAtomicsOptimization) {
        enableGlobalAtomics &= useGlobalAtomics;
    }
    EXPECT_EQ(!enableGlobalAtomics, surfaceState.getDisableSupportForMultiGpuAtomics());
}

static unsigned int numAvailableSubDevicesForMultiGpuGlobalAtomicsImageTest[] = {0, 1, 2};
static unsigned int memFlags[] = {CL_MEM_READ_ONLY, CL_MEM_READ_WRITE};

INSTANTIATE_TEST_CASE_P(MultiGpuGlobalAtomicsImageTest,
                        MultiGpuGlobalAtomicsImageTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(numAvailableSubDevicesForMultiGpuGlobalAtomicsImageTest),
                            ::testing::ValuesIn(memFlags),
                            ::testing::Bool(),
                            ::testing::Bool()));

XEHPTEST_F(ImageCompressionTests, givenXeHpCoreAndRedescribableFormatWhenCreatingAllocationThenDoNotPreferCompression) {
    MockContext context{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_EQ(UnitTestHelper<FamilyType>::tiledImagesSupported, myMemoryManager->capturedPreferCompressed);

    imageFormat.image_channel_order = CL_RG;
    surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(myMemoryManager->capturedPreferCompressed);
}
