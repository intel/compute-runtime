/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_formats.h"
#include "level_zero/core/test/common/ult_helpers_l0.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"

#include "third_party/opencl_headers/CL/cl_ext.h"

namespace L0 {
namespace ult {

using ImageCreate = Test<DeviceFixture>;
using ImageView = Test<DeviceFixture>;

struct ImageUsmPoolTest : ::testing::TestWithParam<int>, public DeviceFixture {
    void SetUp() override {
        NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(GetParam());
        DeviceFixture::setUp();
    }
    void TearDown() override {
        DeviceFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

using ImageCreateUsmPool = ImageUsmPoolTest;

HWTEST_F(ImageCreate, givenValidImageDescriptionWhenImageCreateThenImageIsCreatedCorrectly) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32,
                     ZE_IMAGE_FORMAT_TYPE_UINT,
                     ZE_IMAGE_FORMAT_SWIZZLE_R,
                     ZE_IMAGE_FORMAT_SWIZZLE_G,
                     ZE_IMAGE_FORMAT_SWIZZLE_B,
                     ZE_IMAGE_FORMAT_SWIZZLE_A};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    std::unique_ptr<L0::Image> image(imagePtr);

    ASSERT_NE(image, nullptr);

    auto imageInfo = image->getImageInfo();

    EXPECT_EQ(imageInfo.imgDesc.fromParent, false);
    EXPECT_EQ(imageInfo.imgDesc.imageArraySize, zeDesc.arraylevels);
    EXPECT_EQ(imageInfo.imgDesc.imageDepth, zeDesc.depth);
    EXPECT_EQ(imageInfo.imgDesc.imageHeight, zeDesc.height);
    EXPECT_EQ(imageInfo.imgDesc.imageType, NEO::ImageType::image2DArray);
    EXPECT_EQ(imageInfo.imgDesc.imageWidth, zeDesc.width);
    EXPECT_EQ(imageInfo.imgDesc.numMipLevels, zeDesc.miplevels);
    EXPECT_EQ(imageInfo.imgDesc.numSamples, 0u);
    EXPECT_EQ(imageInfo.baseMipLevel, 0u);
    EXPECT_EQ(imageInfo.linearStorage, false);
    EXPECT_EQ(imageInfo.mipCount, 0u);
    EXPECT_EQ(imageInfo.plane, GMM_NO_PLANE);
    EXPECT_EQ(imageInfo.useLocalMemory, false);
}

HWTEST_F(ImageCreate, givenValidImageDescriptionWhenImageCreateWithUnsupportedImageThenNullPtrImageIsReturned) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_P216};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);

    ASSERT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT);
    ASSERT_EQ(imagePtr, nullptr);
}

class TestImageFormats : public DeviceFixture, public testing::TestWithParam<std::pair<ze_image_format_layout_t, ze_image_format_type_t>> {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
};

HWTEST_F(ImageCreate, givenDifferentSwizzleFormatWhenImageInitializeThenCorrectSwizzleInRSSIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto surfaceState = &imageHW->surfaceState;

    ASSERT_EQ(surfaceState->getShaderChannelSelectRed(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    ASSERT_EQ(surfaceState->getShaderChannelSelectGreen(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
    ASSERT_EQ(surfaceState->getShaderChannelSelectBlue(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    ASSERT_EQ(surfaceState->getShaderChannelSelectAlpha(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
}

HWTEST_F(ImageCreate, givenBindlessImageWhenImageInitializeThenImageImplicitArgsAreCorrectlyStoredInNewSeparateAllocation) {
    if (!device->getNEODevice()->getRootDeviceEnvironment().getReleaseHelper()) {
        GTEST_SKIP();
    }

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t desc = {};
    desc.pNext = &bindlessExtDesc;

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imgImplicitArgsBuffer = imageHW->getImplicitArgsAllocation()->getUnderlyingBuffer();

    {
        auto imgInfo = imageHW->getImageInfo();

        auto clChannelType = getClChannelDataType(desc.format);
        auto clChannelOrder = getClChannelOrder(desc.format);

        ImageImplicitArgs imageImplicitArgs{};
        imageImplicitArgs.structVersion = 0;
        imageImplicitArgs.imageWidth = desc.width;
        imageImplicitArgs.imageHeight = desc.height;
        imageImplicitArgs.imageDepth = desc.depth;

        imageImplicitArgs.imageArraySize = imgInfo.imgDesc.imageArraySize;
        imageImplicitArgs.numSamples = imgInfo.imgDesc.numSamples;
        imageImplicitArgs.channelType = clChannelType;
        imageImplicitArgs.channelOrder = clChannelOrder;
        imageImplicitArgs.numMipLevels = imgInfo.imgDesc.numMipLevels;
        imageImplicitArgs.flatBaseOffset = imageHW->getImplicitArgsAllocation()->getGpuAddress();

        auto pixelSize = imgInfo.surfaceFormat->imageElementSizeInBytes;
        imageImplicitArgs.flatWidth = (imgInfo.imgDesc.imageWidth * pixelSize) - 1u;
        imageImplicitArgs.flagHeight = (imgInfo.imgDesc.imageHeight * pixelSize) - 1u;
        imageImplicitArgs.flatPitch = imgInfo.imgDesc.imageRowPitch - 1u;

        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, structSize)), &imageImplicitArgs.structSize, ImageImplicitArgs::getSize()));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, structVersion)), &imageImplicitArgs.structVersion, sizeof(imageImplicitArgs.structVersion)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, imageWidth)), &imageImplicitArgs.imageWidth, sizeof(imageImplicitArgs.imageWidth)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, imageHeight)), &imageImplicitArgs.imageHeight, sizeof(imageImplicitArgs.imageHeight)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, imageDepth)), &imageImplicitArgs.imageDepth, sizeof(imageImplicitArgs.imageDepth)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, imageArraySize)), &imageImplicitArgs.imageArraySize, sizeof(imageImplicitArgs.imageArraySize)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, numSamples)), &imageImplicitArgs.numSamples, sizeof(imageImplicitArgs.numSamples)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, channelType)), &imageImplicitArgs.channelType, sizeof(imageImplicitArgs.channelType)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, channelOrder)), &imageImplicitArgs.channelOrder, sizeof(imageImplicitArgs.channelOrder)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, numMipLevels)), &imageImplicitArgs.numMipLevels, sizeof(imageImplicitArgs.numMipLevels)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, flatBaseOffset)), &imageImplicitArgs.flatBaseOffset, sizeof(imageImplicitArgs.flatBaseOffset)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, flatWidth)), &imageImplicitArgs.flatWidth, sizeof(imageImplicitArgs.flatWidth)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, flagHeight)), &imageImplicitArgs.flagHeight, sizeof(imageImplicitArgs.flagHeight)));
        EXPECT_EQ(0, memcmp(ptrOffset(imgImplicitArgsBuffer, offsetof(ImageImplicitArgs, flatPitch)), &imageImplicitArgs.flatPitch, sizeof(imageImplicitArgs.flatPitch)));
    }
    {
        auto implicitArgsSurfaceState = &imageHW->implicitArgsSurfaceState;

        auto implicitArgsSurfaceStateBaseAddress = reinterpret_cast<void *>(implicitArgsSurfaceState->getSurfaceBaseAddress());
        EXPECT_EQ(imgImplicitArgsBuffer, implicitArgsSurfaceStateBaseAddress);

        auto implicitArgsSurfaceType = implicitArgsSurfaceState->getSurfaceType();
        EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, implicitArgsSurfaceType);
    }
}

HWTEST_F(ImageCreate, givenBindlessModeDisabledWhenImageInitializeThenImageImplicitArgsAllocationAndSurfaceStateAreNotCreated) {
    if (!device->getNEODevice()->getRootDeviceEnvironment().getReleaseHelper()) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.UseBindlessMode.set(0);

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imgImplicitArgsAllocation = imageHW->getImplicitArgsAllocation();
    EXPECT_EQ(nullptr, imgImplicitArgsAllocation);

    auto implicitArgsSurfaceState = &imageHW->implicitArgsSurfaceState;
    EXPECT_EQ(nullptr, reinterpret_cast<void *>(implicitArgsSurfaceState->getSurfaceBaseAddress()));
}

HWTEST_F(ImageView, givenPlanarImageWhenCreateImageViewThenProperPlaneIsCreated) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ze_image_view_planar_exp_desc_t planeYdesc = {};
    planeYdesc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    planeYdesc.planeIndex = 0u; // Y plane

    ze_image_desc_t imageViewDescPlaneY = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                           &planeYdesc,
                                           (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                           ZE_IMAGE_TYPE_2D,
                                           {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                            ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                            ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                           width,
                                           height,
                                           depth,
                                           0,
                                           0};
    ze_image_handle_t planeY;

    ret = imageHW->createView(device, &imageViewDescPlaneY, &planeY);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    auto yPlaneDesc = static_cast<L0::Image *>(planeY)->getImageInfo().imgDesc;
    EXPECT_EQ(width, yPlaneDesc.imageWidth);
    EXPECT_EQ(height, yPlaneDesc.imageHeight);

    ze_image_view_planar_exp_desc_t planeUVdesc = {};
    planeUVdesc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    planeUVdesc.planeIndex = 1u; // UV plane

    ze_image_desc_t imageViewDescPlaneUV = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                            &planeUVdesc,
                                            (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                            ZE_IMAGE_TYPE_2D,
                                            {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                             ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                             ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                            width / 2,
                                            height / 2,
                                            depth,
                                            0,
                                            0};
    ze_image_handle_t planeUV;
    ret = imageHW->createView(device, &imageViewDescPlaneUV, &planeUV);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    auto uvPlaneDesc = static_cast<L0::Image *>(planeUV)->getImageInfo().imgDesc;
    EXPECT_EQ(width / 2, uvPlaneDesc.imageWidth);
    EXPECT_EQ(height / 2, uvPlaneDesc.imageHeight);

    auto nv12Allocation = imageHW->getAllocation();

    auto planeYAllocation = Image::fromHandle(planeY)->getAllocation();
    auto planeUVAllocation = Image::fromHandle(planeUV)->getAllocation();

    EXPECT_EQ(nv12Allocation->getGpuBaseAddress(), planeYAllocation->getGpuBaseAddress());
    EXPECT_EQ(nv12Allocation->getGpuBaseAddress(), planeUVAllocation->getGpuBaseAddress());

    zeImageDestroy(planeY);
    zeImageDestroy(planeUV);
}

HWTEST_F(ImageView, given3ChannelImageWhenCreateImageViewIsCalledThenProperViewIsCreated) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_1},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &srcImgDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_image_handle_t imgHandle = imagePtr->toHandle();

    ze_image_handle_t viewHandle;
    auto ret = L0::Image::fromHandle(imgHandle)->createView(device, &srcImgDesc, &viewHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto viewDesc = static_cast<L0::Image *>(viewHandle)->getImageInfo().imgDesc;
    EXPECT_EQ(width, viewDesc.imageWidth);
    EXPECT_EQ(height, viewDesc.imageHeight);

    zeImageDestroy(viewHandle);
    zeImageDestroy(imgHandle);
}

HWTEST_F(ImageView, given3Channel16BitImageWhenCreateImageViewIsCalledThenProperViewIsCreated) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_16_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_1},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &srcImgDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_image_handle_t imgHandle = imagePtr->toHandle();

    ze_image_handle_t viewHandle;
    auto ret = L0::Image::fromHandle(imgHandle)->createView(device, &srcImgDesc, &viewHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto viewDesc = static_cast<L0::Image *>(viewHandle)->getImageInfo().imgDesc;
    EXPECT_EQ(width, viewDesc.imageWidth);
    EXPECT_EQ(height, viewDesc.imageHeight);

    zeImageDestroy(viewHandle);
    zeImageDestroy(imgHandle);
}

HWTEST_F(ImageView, given3ChannelMickedImageWhenCreateImageViewIsCalledThenProperViewIsCreated) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_1},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &srcImgDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_image_handle_t imgHandle = imagePtr->toHandle();

    ze_image_desc_t viewImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                   nullptr,
                                   (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                   ZE_IMAGE_TYPE_2D,
                                   {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UNORM,
                                    ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_X,
                                    ZE_IMAGE_FORMAT_SWIZZLE_X, ZE_IMAGE_FORMAT_SWIZZLE_X},
                                   width * 3,
                                   height,
                                   depth,
                                   0,
                                   0};

    ze_image_handle_t viewHandle;
    auto ret = L0::Image::fromHandle(imgHandle)->createView(device, &viewImgDesc, &viewHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    zeImageDestroy(viewHandle);
    zeImageDestroy(imgHandle);
}

HWTEST_F(ImageView, given32bitImageWhenCreateImageViewIsCalledWith3ChannelThenNotSuppotedIsReturned) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &srcImgDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_image_handle_t imgHandle = imagePtr->toHandle();

    ze_image_desc_t viewImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                   nullptr,
                                   (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                   ZE_IMAGE_TYPE_2D,
                                   {ZE_IMAGE_FORMAT_LAYOUT_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT,
                                    ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                    ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_1},
                                   width,
                                   height,
                                   depth,
                                   0,
                                   0};

    ze_image_handle_t viewHandle;
    auto ret = L0::Image::fromHandle(imgHandle)->createView(device, &viewImgDesc, &viewHandle);
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);

    zeImageDestroy(imgHandle);
}

HWTEST_F(ImageView, givenPlanarImageWhenCreateImageWithInvalidStructViewThenProperErrorIsReturned) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ze_image_view_planar_exp_desc_t planeYdesc = {};
    planeYdesc.stype = ZE_STRUCTURE_TYPE_DEVICE_CACHE_PROPERTIES;
    planeYdesc.planeIndex = 0u; // Y plane

    ze_image_desc_t imageViewDescPlaneY = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                           &planeYdesc,
                                           (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                           ZE_IMAGE_TYPE_2D,
                                           {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                            ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                            ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                           width,
                                           height,
                                           depth,
                                           0,
                                           0};
    ze_image_handle_t planeY;

    ret = imageHW->createView(device, &imageViewDescPlaneY, &planeY);
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, ret);
}

HWTEST_F(ImageCreate, givenFDWhenCreatingImageThenSuccessIsReturned) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    ze_external_memory_import_fd_t importFd = {};
    importFd.fd = 1;
    importFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    importFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    desc.pNext = &importFd;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_EQ(static_cast<int>(imageHW->getAllocation()->peekSharedHandle()), importFd.fd);
}

HWTEST_F(ImageCreate, givenOpaqueFdWhenCreatingImageThenSuccessIsReturned) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    ze_external_memory_import_fd_t importFd = {};
    importFd.fd = 1;
    importFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    importFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    desc.pNext = &importFd;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(ImageCreate, givenExportStructWhenCreatingImageThenUnsupportedErrorIsReturned) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    ze_external_memory_export_fd_t exportFd = {};
    exportFd.fd = 1;
    exportFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    desc.pNext = &exportFd;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, ret);
}

class MemoryManagerNTHandleMock : public NEO::OsAgnosticMemoryManager {
  public:
    MemoryManagerNTHandleMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::OsAgnosticMemoryManager(executionEnvironment) {}

    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        auto graphicsAllocation = createMemoryAllocation(AllocationType::internalHostMemory, nullptr, reinterpret_cast<void *>(1), 1,
                                                         4096u, osHandleData.handle, MemoryPool::systemCpuInaccessible,
                                                         properties.rootDeviceIndex, false, false, false);
        graphicsAllocation->setSharedHandle(osHandleData.handle);
        graphicsAllocation->set32BitAllocation(false);
        GmmRequirements gmmRequirements{};
        gmmRequirements.allowLargePages = true;
        gmmRequirements.preferCompressed = false;
        graphicsAllocation->setDefaultGmm(new Gmm(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
        return graphicsAllocation;
    }
};

class ImageCreateExternalMemory : public DeviceFixtureWithCustomMemoryManager<MemoryManagerNTHandleMock> {
  public:
    void setUp() {
        DeviceFixtureWithCustomMemoryManager<MemoryManagerNTHandleMock>::setUp();

        desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
        desc.type = ZE_IMAGE_TYPE_3D;
        desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
        desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
        desc.width = 11;
        desc.height = 13;
        desc.depth = 17;

        desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
        desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
        desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
        desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;
    }

    void tearDown() {
        DeviceFixtureWithCustomMemoryManager<MemoryManagerNTHandleMock>::tearDown();
    }

    ze_image_desc_t desc = {};
    uint64_t imageHandle = 0x1;
};

using ImageCreateExternalMemoryTest = Test<ImageCreateExternalMemory>;

HWTEST_F(ImageCreateExternalMemoryTest, givenNTHandleWhenCreatingImageThenSuccessIsReturned) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    desc.pNext = &importNTHandle;

    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());
    L0UltHelper::initUsmAllocPools(driverHandle.get());

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_EQ(imageHW->getAllocation()->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateExternalMemoryTest, givenD3D12HeapHandleWhenCreatingImageThenSuccessIsReturned) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_HEAP;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    desc.pNext = &importNTHandle;

    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());
    L0UltHelper::initUsmAllocPools(driverHandle.get());

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_EQ(imageHW->getAllocation()->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateExternalMemoryTest, givenD3D12ResourceHandleWhenCreatingImageThenSuccessIsReturned) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    desc.pNext = &importNTHandle;

    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());
    L0UltHelper::initUsmAllocPools(driverHandle.get());

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_EQ(imageHW->getAllocation()->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateExternalMemoryTest, givenD3D11TextureHandleWhenCreatingImageThenSuccessIsReturned) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D11_TEXTURE;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    desc.pNext = &importNTHandle;

    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());
    L0UltHelper::initUsmAllocPools(driverHandle.get());

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_EQ(imageHW->getAllocation()->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));

    imageHW.reset(nullptr);
}

using ImageCreateWithMemoryManagerNTHandleMock = Test<DeviceFixtureWithCustomMemoryManager<MemoryManagerNTHandleMock>>;

HWTEST_F(ImageCreateWithMemoryManagerNTHandleMock, givenNTHandleWhenCreatingNV12ImageThenSuccessIsReturnedAndUVOffsetIsSet) {
    constexpr uint32_t yOffsetForUVPlane = 8u; // mock sets reqOffsetInfo.Lock.Offset to 16 and reqOffsetInfo.Lock.Pitch to 2

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_2D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_NV12;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_G;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_B;

    uint64_t imageHandle = 0x1;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    desc.pNext = &importNTHandle;

    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());
    L0UltHelper::initUsmAllocPools(driverHandle.get());

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_EQ(imageHW->getAllocation()->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));
    EXPECT_EQ(yOffsetForUVPlane, imageHW->surfaceState.getYOffsetForUOrUvPlane());
}

class FailMemoryManagerMock : public NEO::OsAgnosticMemoryManager {
  public:
    FailMemoryManagerMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::OsAgnosticMemoryManager(executionEnvironment) {}

    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        if (fail) {
            return nullptr;
        }
        return OsAgnosticMemoryManager::allocateGraphicsMemoryWithProperties(properties);
    }
    bool fail = false;
};

using ImageCreateWithFailMemoryManagerMock = Test<DeviceFixtureWithCustomMemoryManager<FailMemoryManagerMock>>;

HWTEST_F(ImageCreateWithFailMemoryManagerMock, givenImageDescWhenFailImageAllocationThenProperErrorIsReturned) {
    VariableBackup<bool> backupSipInitType{&MockSipData::useMockSip};

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    auto isHexadecimalArrayPreferred = gfxCoreHelper.isSipKernelAsHexadecimalArrayPreferred();
    if (isHexadecimalArrayPreferred) {
        backupSipInitType = true;
    }

    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());
    L0UltHelper::initUsmAllocPools(driverHandle.get());

    L0::Image *imageHandle = nullptr;
    static_cast<FailMemoryManagerMock *>(execEnv->memoryManager.get())->fail = true;
    auto ret = L0::Image::create(neoDevice->getHardwareInfo().platform.eProductFamily, device, &desc, &imageHandle);

    ASSERT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, ret);
    EXPECT_EQ(imageHandle, nullptr);
}

HWTEST_F(ImageCreate, givenMediaBlockOptionWhenCopySurfaceStateThenSurfaceStateIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto surfaceState = &imageHW->surfaceState;

    RENDER_SURFACE_STATE rss = {};

    imageHW->copySurfaceStateToSSH(&rss, 0u, NEO::BindlessImageSlot::image, true);

    EXPECT_EQ(surfaceState->getWidth(), (static_cast<uint32_t>(imageHW->getImageInfo().surfaceFormat->imageElementSizeInBytes) * static_cast<uint32_t>(imageHW->getImageInfo().imgDesc.imageWidth)) / sizeof(uint32_t));
}

HWTEST_P(TestImageFormats, givenValidLayoutAndTypeWhenCreateImageCoreFamilyThenValidImageIsCreated) {
    auto params = GetParam();

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 10u;
    zeDesc.height = 10u;
    zeDesc.width = 10u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.flags = ZE_IMAGE_FLAG_KERNEL_WRITE;

    zeDesc.format = {};
    zeDesc.format.layout = params.first;
    zeDesc.format.type = params.second;
    zeDesc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    zeDesc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    zeDesc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    zeDesc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();

    imageHW->initialize(device, &zeDesc);

    EXPECT_EQ(imageHW->getAllocation()->getAllocationType(), NEO::AllocationType::image);
    auto rss = imageHW->surfaceState;
    EXPECT_EQ(rss.getSurfaceType(), FamilyType::RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D);
    EXPECT_EQ(rss.getAuxiliarySurfaceMode(), FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(rss.getRenderTargetViewExtent(), 1u);

    auto hAlign = static_cast<typename FamilyType::RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT>(imageHW->getAllocation()->getDefaultGmm()->gmmResourceInfo->getHAlignSurfaceState());
    auto vAlign = static_cast<typename FamilyType::RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT>(imageHW->getAllocation()->getDefaultGmm()->gmmResourceInfo->getVAlignSurfaceState());

    EXPECT_EQ(rss.getSurfaceHorizontalAlignment(), hAlign);
    EXPECT_EQ(rss.getSurfaceVerticalAlignment(), vAlign);

    auto isMediaFormatLayout = imageHW->isMediaFormat(params.first);
    if (isMediaFormatLayout) {
        auto imgInfo = imageHW->getImageInfo();
        EXPECT_EQ(rss.getShaderChannelSelectAlpha(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
        EXPECT_EQ(rss.getYOffsetForUOrUvPlane(), imgInfo.yOffsetForUVPlane);
        EXPECT_EQ(rss.getXOffsetForUOrUvPlane(), imgInfo.xOffset);
    } else {
        EXPECT_EQ(rss.getYOffsetForUOrUvPlane(), 0u);
        EXPECT_EQ(rss.getXOffsetForUOrUvPlane(), 0u);
    }

    EXPECT_EQ(rss.getSurfaceMinLOD(), 0u);
    EXPECT_EQ(rss.getMIPCountLOD(), 0u);

    if (!isMediaFormatLayout) {
        EXPECT_EQ(rss.getShaderChannelSelectRed(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_RED);
        EXPECT_EQ(rss.getShaderChannelSelectGreen(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_GREEN);
        EXPECT_EQ(rss.getShaderChannelSelectBlue(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_BLUE);
        EXPECT_EQ(rss.getShaderChannelSelectAlpha(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_ALPHA);
    } else {
        EXPECT_EQ(rss.getShaderChannelSelectRed(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_RED);
        EXPECT_EQ(rss.getShaderChannelSelectGreen(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_GREEN);
        EXPECT_EQ(rss.getShaderChannelSelectBlue(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_BLUE);
    }

    EXPECT_EQ(rss.getNumberOfMultisamples(), FamilyType::RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1);
}

std::pair<ze_image_format_layout_t, ze_image_format_type_t> validFormats[] = {
    {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_Y8, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_AYUV, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_P010, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_Y410, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_P012, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_Y216, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_P016, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_RGBP, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_BRGP, ZE_IMAGE_FORMAT_TYPE_UNORM}};

INSTANTIATE_TEST_SUITE_P(
    validImageFormats,
    TestImageFormats,
    testing::ValuesIn(validFormats));

TEST(ImageFormatDescHelperTest, givenUnsupportedImageFormatLayoutAndTypeThenProperClEnumIsReturned) {
    auto invalid = static_cast<cl_channel_type>(CL_INVALID_VALUE);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_UNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_UNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
}

TEST(ImageFormatDescHelperTest, givenSupportedImageFormatLayoutAndTypeThenProperClEnumIsReturned) {
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_HALF_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_HALF_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_HALF_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_HALF_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32_32, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT_101010_2));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_SHORT_565));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_SHORT_555));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_NV12_INTEL));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_YUYV_INTEL));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_VYUY_INTEL));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_YVYU_INTEL));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UYVY_INTEL));
}

TEST(ImageFormatDescHelperTest, givenSwizzlesThenEqualityIsProperlyDetermined) {
    Swizzles ref{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A};
    EXPECT_FALSE((ref == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_FALSE((ref == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_FALSE((ref == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_FALSE((ref == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_TRUE((ref == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A}));
}

TEST(ImageFormatDescHelperTest, givenSupportedSwizzlesThenProperClEnumIsReturned) {
    ze_image_format_t format{};

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_R));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_A));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_RG));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_RA));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_RGB));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_RGBA));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_B;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_ARGB));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_B;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_ABGR));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_B;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_BGRA));
}

using ImageGetMemoryProperties = Test<DeviceFixture>;

HWTEST_F(ImageGetMemoryProperties, givenImageMemoryPropertiesExpStructureWhenGetMemroyPropertiesThenProperDataAreSet) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32,
                     ZE_IMAGE_FORMAT_TYPE_UINT,
                     ZE_IMAGE_FORMAT_SWIZZLE_R,
                     ZE_IMAGE_FORMAT_SWIZZLE_G,
                     ZE_IMAGE_FORMAT_SWIZZLE_B,
                     ZE_IMAGE_FORMAT_SWIZZLE_A};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    std::unique_ptr<L0::Image> image(imagePtr);

    ASSERT_NE(image, nullptr);

    ze_image_memory_properties_exp_t imageMemoryPropertiesExp = {};

    image->getMemoryProperties(&imageMemoryPropertiesExp);

    auto imageInfo = image->getImageInfo();

    EXPECT_EQ(imageInfo.surfaceFormat->imageElementSizeInBytes, imageMemoryPropertiesExp.size);
    EXPECT_EQ(imageInfo.slicePitch, imageMemoryPropertiesExp.slicePitch);
    EXPECT_EQ(imageInfo.rowPitch, imageMemoryPropertiesExp.rowPitch);
}

HWTEST_F(ImageGetMemoryProperties, givenDebugFlagSetWhenCreatingImageThenEnableCompression) {
    DebugManagerStateRestore restore;

    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedImages = true;

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32,
                     ZE_IMAGE_FORMAT_TYPE_UINT,
                     ZE_IMAGE_FORMAT_SWIZZLE_R,
                     ZE_IMAGE_FORMAT_SWIZZLE_G,
                     ZE_IMAGE_FORMAT_SWIZZLE_B,
                     ZE_IMAGE_FORMAT_SWIZZLE_A};

    {
        Image *imagePtr = nullptr;
        auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_NE(nullptr, imagePtr);
        std::unique_ptr<L0::Image> image(imagePtr);

        auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
        EXPECT_EQ(l0GfxCoreHelper.imageCompressionSupported(device->getHwInfo()), image->getAllocation()->isCompressionEnabled());
    }

    {
        NEO::debugManager.flags.RenderCompressedImagesEnabled.set(1);

        ze_external_memory_import_win32_handle_t compressionHint = {};
        compressionHint.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        compressionHint.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED;

        zeDesc.pNext = &compressionHint;

        Image *imagePtr = nullptr;
        auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_NE(nullptr, imagePtr);
        std::unique_ptr<L0::Image> image(imagePtr);

        EXPECT_FALSE(image->getAllocation()->isCompressionEnabled());

        zeDesc.pNext = nullptr;
    }

    {
        NEO::debugManager.flags.RenderCompressedImagesEnabled.set(1);

        Image *imagePtr = nullptr;
        auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_NE(nullptr, imagePtr);
        std::unique_ptr<L0::Image> image(imagePtr);

        EXPECT_TRUE(image->getAllocation()->isCompressionEnabled());
    }

    {
        NEO::debugManager.flags.RenderCompressedImagesEnabled.set(0);

        Image *imagePtr = nullptr;
        auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_NE(nullptr, imagePtr);
        std::unique_ptr<L0::Image> image(imagePtr);

        EXPECT_FALSE(image->getAllocation()->isCompressionEnabled());
    }
}

HWTEST_F(ImageGetMemoryProperties, givenDebugFlagSetWhenCreatingLinearImageThenDontEnableCompression) {
    DebugManagerStateRestore restore;

    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedImages = true;

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_1D;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32,
                     ZE_IMAGE_FORMAT_TYPE_UINT,
                     ZE_IMAGE_FORMAT_SWIZZLE_R,
                     ZE_IMAGE_FORMAT_SWIZZLE_G,
                     ZE_IMAGE_FORMAT_SWIZZLE_B,
                     ZE_IMAGE_FORMAT_SWIZZLE_A};

    Image *imagePtr = nullptr;
    auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(nullptr, imagePtr);
    std::unique_ptr<L0::Image> image(imagePtr);

    EXPECT_FALSE(image->getAllocation()->isCompressionEnabled());
}

HWTEST2_F(ImageCreate, givenImageSizeZeroThenDummyImageIsCreated, IsAtMostXeHpgCore) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = desc.format.y = desc.format.z = desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.width = 0;
    desc.height = 0;
    desc.depth = 0;

    L0::Image *imagePtr;

    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto image = whiteboxCast(imagePtr);
    ASSERT_NE(nullptr, image);

    auto alloc = image->getAllocation();
    ASSERT_NE(nullptr, alloc);

    auto renderSurfaceState = FamilyType::cmdInitRenderSurfaceState;
    image->copySurfaceStateToSSH(&renderSurfaceState, 0u, NEO::BindlessImageSlot::image, false);

    EXPECT_EQ(1u, renderSurfaceState.getWidth());
    EXPECT_EQ(1u, renderSurfaceState.getHeight());
    EXPECT_EQ(1u, renderSurfaceState.getDepth());

    image->destroy();
}

HWTEST2_F(ImageCreate, WhenGettingImagePropertiesThenFilterFlagsAreValid, IsAtMostDg2) {
    ze_image_properties_t properties;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = desc.format.y = desc.format.z = desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.width = 10;
    desc.height = 10;
    desc.depth = 10;

    auto result = device->imageGetProperties(&desc, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto samplerFilterFlagsValid = (properties.samplerFilterFlags ==
                                    ZE_IMAGE_SAMPLER_FILTER_FLAG_POINT) ||
                                   (properties.samplerFilterFlags ==
                                    ZE_IMAGE_SAMPLER_FILTER_FLAG_LINEAR);
    EXPECT_TRUE(samplerFilterFlagsValid);
}

HWTEST2_F(ImageCreate, WhenDestroyingImageThenSuccessIsReturned, IsAtMostDg2) {
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    L0::Image *imagePtr;

    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto image = whiteboxCast(imagePtr);
    ASSERT_NE(nullptr, image);

    result = zeImageDestroy(image->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(ImageCreate, WhenCreatingImageThenNonNullPointerIsReturned, IsAtMostDg2) {
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    L0::Image *imagePtr;

    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto image = whiteboxCast(imagePtr);
    ASSERT_NE(nullptr, image);

    image->destroy();
}

HWTEST2_F(ImageCreate, givenInvalidProductFamilyThenNullIsReturned, IsAtMostDg2) {
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    L0::Image *imagePtr;

    auto result = Image::create(IGFX_UNKNOWN, device, &desc, &imagePtr);
    ASSERT_NE(ZE_RESULT_SUCCESS, result);

    auto image = whiteboxCast(imagePtr);
    ASSERT_EQ(nullptr, image);
}

HWTEST2_F(ImageCreate, WhenImageIsCreatedThenDescMatchesAllocation, IsAtMostDg2) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = desc.format.y = desc.format.z = desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.width = 10;
    desc.height = 10;
    desc.depth = 10;

    L0::Image *imagePtr;

    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto image = whiteboxCast(imagePtr);
    ASSERT_NE(nullptr, image);

    auto alloc = image->getAllocation();
    ASSERT_NE(nullptr, alloc);

    image->destroy();
}

HWTEST2_F(ImageCreate, WhenImageIsCreatedThenDescMatchesSurface, IsAtMostDg2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    auto imageCore = new WhiteBox<ImageCoreFamily<FamilyType::gfxCoreFamily>>();
    ze_result_t ret = imageCore->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto surfaceState = &imageCore->surfaceState;

    ASSERT_EQ(surfaceState->getSurfaceType(), RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D);
    ASSERT_EQ(surfaceState->getSurfaceFormat(), RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UINT);
    ASSERT_EQ(surfaceState->getWidth(), 11u);
    ASSERT_EQ(surfaceState->getHeight(), 13u);
    ASSERT_EQ(surfaceState->getDepth(), 17u);
    ASSERT_EQ(surfaceState->getShaderChannelSelectRed(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
    ASSERT_EQ(surfaceState->getShaderChannelSelectGreen(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
    ASSERT_EQ(surfaceState->getShaderChannelSelectBlue(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE);
    ASSERT_EQ(surfaceState->getShaderChannelSelectAlpha(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);

    delete imageCore;
}

HWTEST2_F(ImageCreate, WhenImageIsCreatedThenDescSwizzlesMatchSurface, IsAtMostDg2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageCore = new WhiteBox<ImageCoreFamily<FamilyType::gfxCoreFamily>>();
    ze_result_t ret = imageCore->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto surfaceState = &imageCore->surfaceState;

    ASSERT_EQ(surfaceState->getShaderChannelSelectRed(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    ASSERT_EQ(surfaceState->getShaderChannelSelectGreen(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
    ASSERT_EQ(surfaceState->getShaderChannelSelectBlue(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    ASSERT_EQ(surfaceState->getShaderChannelSelectAlpha(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);

    delete imageCore;
}

HWTEST2_F(ImageCreate, WhenImageIsCreatedThenDescMatchesSurfaceFormats, IsAtMostDg2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;

    ze_image_desc_t descOdd = {};
    descOdd.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    descOdd.type = ZE_IMAGE_TYPE_3D;
    descOdd.width = 11;
    descOdd.height = 13;
    descOdd.depth = 17;

    // Some media formats require even dimensions due to subsampling.
    ze_image_desc_t descEven = {};
    descEven.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    descEven.type = ZE_IMAGE_TYPE_2D;
    descEven.width = 12;
    descEven.height = 16;
    descEven.depth = 1;

    struct FormatInfo {
        size_t elemBitSize;
        ze_image_format_layout_t formatLayout;
        ze_image_format_type_t formatType;
        SURFACE_FORMAT ssFormat;
        bool requireEven;
    };
    struct FormatInfo testFormats[] = {
        {sizeof(uint8_t) * 8, ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_UINT, false},
        {sizeof(uint32_t) * 4 * 8, ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_UINT, false},
        {sizeof(uint8_t) * 4 * 8, ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM, false},
        {sizeof(int16_t) * 8, ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SNORM,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_SNORM, false},
        {sizeof(float) * 4 * 8, ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_FLOAT, false},
        {sizeof(uint32_t) * 8, ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_UINT,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UINT, false}};
    size_t numFormats = sizeof(testFormats) / sizeof(struct FormatInfo);

    for (size_t i = 0; i < numFormats; i++) {
        for (int j = 0; j < 2; j++) {
            bool odd = (j == 0);

            if (odd && testFormats[i].requireEven) {
                continue;
            }

            ze_image_desc_t *desc = odd ? &descOdd : &descEven;

            desc->format.layout = testFormats[i].formatLayout;
            desc->format.type = testFormats[i].formatType;

            auto imageCore = new WhiteBox<ImageCoreFamily<FamilyType::gfxCoreFamily>>();
            ze_result_t ret = imageCore->initialize(device, desc);
            ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

            auto surfaceState = &imageCore->surfaceState;

            ASSERT_EQ(surfaceState->getSurfaceFormat(), testFormats[i].ssFormat);

            delete imageCore;
        }
    }
}

HWTEST2_F(ImageCreate, WhenCopyingToSshThenSurfacePropertiesAreRetained, IsAtMostDg2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    uint8_t mockSSH[sizeof(RENDER_SURFACE_STATE) * 4] = {0};

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_2D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = desc.format.y = desc.format.z = desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 1;

    auto imageA = new ImageCoreFamily<FamilyType::gfxCoreFamily>();
    ze_result_t ret = imageA->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_32_32;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = desc.format.y = desc.format.z = desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.width = 10;
    desc.height = 10;

    auto imageB = new ImageCoreFamily<FamilyType::gfxCoreFamily>();
    ret = imageB->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    imageA->copySurfaceStateToSSH(mockSSH, 0, NEO::BindlessImageSlot::image, false);
    imageB->copySurfaceStateToSSH(mockSSH, sizeof(RENDER_SURFACE_STATE), NEO::BindlessImageSlot::image, false);

    auto surfaceStateA = reinterpret_cast<RENDER_SURFACE_STATE *>(mockSSH);
    ASSERT_EQ(surfaceStateA->getSurfaceType(), RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D);
    ASSERT_EQ(surfaceStateA->getSurfaceFormat(),
              RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UINT);
    ASSERT_EQ(surfaceStateA->getWidth(), 11u);
    ASSERT_EQ(surfaceStateA->getHeight(), 13u);

    auto surfaceStateB =
        reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(mockSSH, sizeof(RENDER_SURFACE_STATE)));
    ASSERT_EQ(surfaceStateB->getSurfaceType(), RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D);
    ASSERT_EQ(surfaceStateB->getSurfaceFormat(), RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_UINT);
    ASSERT_EQ(surfaceStateB->getWidth(), 10u);
    ASSERT_EQ(surfaceStateB->getHeight(), 10u);

    delete imageA;
    delete imageB;
}

HWTEST_F(ImageCreate, WhenImageViewCreateExpThenSuccessIsReturned) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ze_image_view_planar_exp_desc_t planeYdesc = {};
    planeYdesc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    planeYdesc.planeIndex = 0u; // Y plane

    ze_image_desc_t imageViewDescPlaneY = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                           &planeYdesc,
                                           (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                           ZE_IMAGE_TYPE_2D,
                                           {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                            ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                            ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                           width,
                                           height,
                                           depth,
                                           0,
                                           0};
    ze_image_handle_t planeY;

    ret = imageHW->createView(device, &imageViewDescPlaneY, &planeY);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto nv12Allocation = imageHW->getAllocation();

    auto planeYAllocation = Image::fromHandle(planeY)->getAllocation();

    EXPECT_EQ(nv12Allocation->getGpuBaseAddress(), planeYAllocation->getGpuBaseAddress());

    zeImageDestroy(planeY);
}

HWTEST_F(ImageCreate, WhenImageViewCreateExtThenSuccessIsReturned) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ze_image_view_planar_ext_desc_t planeYdesc = {};
    planeYdesc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXT_DESC;
    planeYdesc.planeIndex = 0u; // Y plane

    ze_image_desc_t imageViewDescPlaneY = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                           &planeYdesc,
                                           (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                           ZE_IMAGE_TYPE_2D,
                                           {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                            ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                            ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                           width,
                                           height,
                                           depth,
                                           0,
                                           0};
    ze_image_handle_t planeY;

    ret = imageHW->createView(device, &imageViewDescPlaneY, &planeY);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto nv12Allocation = imageHW->getAllocation();

    auto planeYAllocation = Image::fromHandle(planeY)->getAllocation();

    EXPECT_EQ(nv12Allocation->getGpuBaseAddress(), planeYAllocation->getGpuBaseAddress());

    zeImageDestroy(planeY);
}

struct ImageSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily >= IGFX_SKYLAKE && NEO::HwMapper<productFamily>::GfxProduct::supportsSampler;
    }
};

HWTEST2_F(ImageCreate, GivenNonBindlessImageWhenGettingDeviceOffsetThenErrorIsReturned, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(nullptr);

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(nullptr, imageHW->getBindlessSlot());

    uint64_t deviceOffset = 0;
    ret = imageHW->getDeviceOffset(&deviceOffset);
    ASSERT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, ret);
}

HWTEST2_F(ImageCreate, GivenNoBindlessHelperAndBindlessImageFlagWhenCreatingImageThenErrorIsReturned, ImageSupport) {
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.reset();

    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);
}

HWTEST2_F(ImageCreate, GivenBindlessImageWhenGettingDeviceOffsetThenBindlessSlotIsReturned, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_NE(nullptr, imageHW->getBindlessSlot());

    uint64_t deviceOffset = 0;

    ret = imageHW->getDeviceOffset(&deviceOffset);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto ssHeapInfo = imageHW->getBindlessSlot();
    ASSERT_NE(nullptr, ssHeapInfo);

    EXPECT_EQ(ssHeapInfo->surfaceStateOffset, deviceOffset);
}

HWTEST2_F(ImageCreate, GivenBindlessImageWhenBindlessSlotAllocationFailsThenImageInitializationFails, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    bindlessHelper->failAllocateSS = true;
    bindlessHelper->globalSsh->getSpace(bindlessHelper->globalSsh->getAvailableSpace());

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, ret);
}

HWTEST2_P(ImageCreateUsmPool, GivenBindlessImageWhenImageCreatedWithInvalidPitchedPtrThenErrorIsReturned, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_image_pitched_exp_desc_t pitchedDesc = {};
    pitchedDesc.stype = ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC;
    pitchedDesc.ptr = reinterpret_cast<void *>(0x1234000); // invalid USM device pointer

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = &pitchedDesc;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};
    {
        auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
        auto ret = imageHW->initialize(device, &srcImgDesc);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);
    }
    if (auto usmPool = neoDevice->getUsmMemAllocPool()) {
        pitchedDesc.ptr = reinterpret_cast<MockUsmMemAllocPool *>(usmPool)->pool; // not allocated ptr within USM pool
        auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
        auto ret = imageHW->initialize(device, &srcImgDesc);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);
    }
}

HWTEST2_P(ImageCreateUsmPool, GivenBindlessImageWhenImageCreatedWithDeviceUSMPitchedPtrThenImageIsCreated, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    const size_t size = 4096;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto ret = context->allocDeviceMem(device,
                                       &deviceDesc,
                                       size,
                                       0,
                                       &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    const auto gpuAddress = allocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress();
    const auto offset = ptrDiff(castToUint64(ptr), gpuAddress);

    ze_image_pitched_exp_desc_t pitchedDesc = {};
    pitchedDesc.stype = ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC;
    pitchedDesc.ptr = ptr; // USM device pointer

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = &pitchedDesc;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_TRUE(imageHW->bindlessImage);
    EXPECT_TRUE(imageHW->imageFromBuffer);
    EXPECT_EQ(offset, imageHW->imgInfo.offset);

    size_t rowPitch = 0;
    imageHW->getPitchFor2dImage(device->toHandle(), width, height, 1, &rowPitch);
    EXPECT_EQ(rowPitch, imageHW->imgInfo.rowPitch);

    uint64_t deviceOffset = 0;
    ret = imageHW->getDeviceOffset(&deviceOffset);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    auto ssHeapInfo = imageHW->getBindlessSlot();
    ASSERT_NE(nullptr, ssHeapInfo);
    EXPECT_EQ(ssHeapInfo->surfaceStateOffset, deviceOffset);

    // Allocation in image is equal to allocation from USM memory
    EXPECT_EQ(allocData->gpuAllocations.getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex()), imageHW->getAllocation());
    // Allocation should not have bindless offset allocated
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), imageHW->getAllocation()->getBindlessOffset());

    imageHW.reset(nullptr);

    ret = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(ImageCreate, GivenBindlessImageWhenImageViewCreatedWithDeviceUMSPitchedPtrThenErrorIsReturned, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    const size_t size = 4096;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto ret = context->allocDeviceMem(device,
                                       &deviceDesc,
                                       size,
                                       0,
                                       &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC, &bindlessExtDesc, ZE_IMAGE_FLAG_KERNEL_WRITE, ZE_IMAGE_TYPE_2D, {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT, ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A}, width, height, depth, 0, 0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_TRUE(imageHW->bindlessImage);

    ze_image_pitched_exp_desc_t pitchedDesc = {};
    pitchedDesc.stype = ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC;
    pitchedDesc.ptr = ptr; // USM device pointer

    ze_image_desc_t imgViewDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC, &pitchedDesc, ZE_IMAGE_FLAG_KERNEL_WRITE, ZE_IMAGE_TYPE_2D, {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT, ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A}, width, height, depth, 0, 0};

    ze_image_handle_t imageView = {};
    ret = imageHW->createView(device, &imgViewDesc, &imageView);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);

    ret = context->freeMem(ptr);
}

HWTEST2_P(ImageCreateUsmPool, GivenBindlessImageWhenImageViewCreatedWithTheSameFormatThenImageViewHasCorrectImgInfo, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    const size_t size = 4096;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto ret = context->allocDeviceMem(device,
                                       &deviceDesc,
                                       size,
                                       0,
                                       &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    const auto gpuAddress = allocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress();
    const auto offset = ptrDiff(castToUint64(ptr), gpuAddress);

    ze_image_pitched_exp_desc_t pitchedDesc = {};
    pitchedDesc.stype = ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC;
    pitchedDesc.ptr = ptr; // USM device pointer

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = &pitchedDesc;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC, &bindlessExtDesc, ZE_IMAGE_FLAG_KERNEL_WRITE, ZE_IMAGE_TYPE_2D, {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT, ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A}, width, height, depth, 0, 0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_TRUE(imageHW->bindlessImage);

    ze_image_handle_t imageView = {};
    bindlessExtDesc.pNext = nullptr;
    ret = imageHW->createView(device, &srcImgDesc, &imageView);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imageViewObject = static_cast<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>> *>(Image::fromHandle(imageView));

    EXPECT_TRUE(imageViewObject->bindlessImage);
    EXPECT_TRUE(imageViewObject->imageFromBuffer);

    EXPECT_EQ(imageHW->imgInfo.slicePitch, imageViewObject->getImageInfo().slicePitch);
    EXPECT_NE(0u, imageViewObject->getImageInfo().slicePitch);
    EXPECT_EQ(imageHW->imgInfo.rowPitch, imageViewObject->getImageInfo().rowPitch);
    EXPECT_NE(0u, imageViewObject->getImageInfo().rowPitch);
    EXPECT_EQ(imageHW->imgInfo.offset, imageViewObject->getImageInfo().offset);
    EXPECT_EQ(offset, imageViewObject->getImageInfo().offset);
    EXPECT_EQ(imageHW->imgInfo.size, imageViewObject->getImageInfo().size);
    EXPECT_NE(0u, imageViewObject->getImageInfo().size);
    EXPECT_EQ(imageHW->imgInfo.linearStorage, imageViewObject->getImageInfo().linearStorage);
    EXPECT_TRUE(imageViewObject->getImageInfo().linearStorage);

    Image::fromHandle(imageView)->destroy();
    ret = context->freeMem(ptr);
}

INSTANTIATE_TEST_SUITE_P(UsmPoolDisabledEnabled, ImageCreateUsmPool, ::testing::Values(0, 2));

HWTEST2_F(ImageCreate, GivenBindlessImageWhenInitializedThenSurfaceStateCopiedToSSH, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto ssHeapInfo = imageHW->getBindlessSlot();
    ASSERT_NE(nullptr, ssHeapInfo);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto *surfaceState = static_cast<RENDER_SURFACE_STATE *>(ssHeapInfo->ssPtr);
    ASSERT_EQ(surfaceState->getSurfaceType(), RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D);
    ASSERT_EQ(surfaceState->getSurfaceFormat(),
              RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UINT);
    ASSERT_EQ(surfaceState->getWidth(), width);
    ASSERT_EQ(surfaceState->getHeight(), height);
    ASSERT_EQ(surfaceState->getDepth(), depth);
}

HWTEST2_F(ImageCreate, GivenBindlessSampledImageWhenCreatedThenSampledImageFlagAndSamplerDescIsSet, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_sampler_desc_t samplerDesc = {
        ZE_STRUCTURE_TYPE_SAMPLER_DESC,
        nullptr,
        ZE_SAMPLER_ADDRESS_MODE_CLAMP,
        ZE_SAMPLER_FILTER_MODE_LINEAR,
        false};

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = &samplerDesc;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS | ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_TRUE(imageHW->bindlessImage);

    EXPECT_TRUE(imageHW->sampledImage);
    EXPECT_EQ(imageHW->samplerDesc.addressMode, samplerDesc.addressMode);
    EXPECT_EQ(imageHW->samplerDesc.filterMode, samplerDesc.filterMode);
    EXPECT_EQ(imageHW->samplerDesc.isNormalized, samplerDesc.isNormalized);
    EXPECT_EQ(nullptr, imageHW->samplerDesc.pNext);
}

HWTEST2_F(ImageCreate, GivenBindlessSampledImageWhenImageViewCreatedWithCorrectFlagsThenImageCreationIsSuccessful, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_sampler_desc_t samplerDesc = {
        ZE_STRUCTURE_TYPE_SAMPLER_DESC,
        nullptr,
        ZE_SAMPLER_ADDRESS_MODE_CLAMP,
        ZE_SAMPLER_FILTER_MODE_LINEAR,
        false};

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = &samplerDesc;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS | ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_TRUE(imageHW->bindlessImage);

    ze_image_handle_t imageView = {};
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;
    bindlessExtDesc.pNext = nullptr;
    ret = imageHW->createView(device, &srcImgDesc, &imageView);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);

    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS | ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;
    bindlessExtDesc.pNext = nullptr;
    ret = imageHW->createView(device, &srcImgDesc, &imageView);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);

    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS | ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;
    bindlessExtDesc.pNext = &samplerDesc;
    ret = imageHW->createView(device, &srcImgDesc, &imageView);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imageViewObject = static_cast<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>> *>(Image::fromHandle(imageView));

    EXPECT_TRUE(imageViewObject->bindlessImage);
    EXPECT_TRUE(imageViewObject->sampledImage);

    EXPECT_EQ(ZE_SAMPLER_ADDRESS_MODE_CLAMP, imageViewObject->samplerDesc.addressMode);
    EXPECT_EQ(ZE_SAMPLER_FILTER_MODE_LINEAR, imageViewObject->samplerDesc.filterMode);

    EXPECT_EQ(imageHW->imgInfo.slicePitch, imageViewObject->getImageInfo().slicePitch);
    EXPECT_NE(0u, imageViewObject->getImageInfo().slicePitch);
    EXPECT_EQ(imageHW->imgInfo.rowPitch, imageViewObject->getImageInfo().rowPitch);
    EXPECT_NE(0u, imageViewObject->getImageInfo().rowPitch);
    EXPECT_EQ(imageHW->imgInfo.offset, imageViewObject->getImageInfo().offset);
    EXPECT_EQ(imageHW->imgInfo.size, imageViewObject->getImageInfo().size);
    EXPECT_NE(0u, imageViewObject->getImageInfo().size);
    EXPECT_EQ(imageHW->imgInfo.linearStorage, imageViewObject->getImageInfo().linearStorage);

    Image::fromHandle(imageView)->destroy();
}

HWTEST2_F(ImageCreate, GivenNoBindlessFlagWhenSampledImageCreatedThenErrorIsReturned, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_sampler_desc_t samplerDesc = {
        ZE_STRUCTURE_TYPE_SAMPLER_DESC,
        nullptr,
        ZE_SAMPLER_ADDRESS_MODE_CLAMP,
        ZE_SAMPLER_FILTER_MODE_LINEAR,
        false};

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = &samplerDesc;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);
}

HWTEST2_F(ImageCreate, GivenNoSamplerDescWhenSampledImageCreatedThenErrorIsReturned, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS | ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);
}

HWTEST2_F(ImageCreate, GivenBindlessSampledImageWhenInitializedThenSamplerStateCopiedToSSH, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_sampler_desc_t samplerDesc = {
        ZE_STRUCTURE_TYPE_SAMPLER_DESC,
        nullptr,
        ZE_SAMPLER_ADDRESS_MODE_CLAMP,
        ZE_SAMPLER_FILTER_MODE_LINEAR,
        false};

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = &samplerDesc;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS | ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto ssHeapInfo = imageHW->getBindlessSlot();
    ASSERT_NE(nullptr, ssHeapInfo);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto *surfaceState = static_cast<RENDER_SURFACE_STATE *>(ssHeapInfo->ssPtr);
    ASSERT_EQ(surfaceState->getSurfaceType(), RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D);
    ASSERT_EQ(surfaceState->getSurfaceFormat(),
              RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UINT);
    ASSERT_EQ(surfaceState->getWidth(), width);
    ASSERT_EQ(surfaceState->getHeight(), height);
    ASSERT_EQ(surfaceState->getDepth(), depth);

    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    auto *samplerState = static_cast<SAMPLER_STATE *>(ptrOffset(ssHeapInfo->ssPtr, sizeof(RENDER_SURFACE_STATE) * NEO::BindlessImageSlot::sampler));

    ASSERT_EQ(samplerState->getTcxAddressControlMode(), SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP);
    ASSERT_EQ(samplerState->getTcyAddressControlMode(), SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP);
    ASSERT_EQ(samplerState->getTczAddressControlMode(), SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP);

    ASSERT_EQ(samplerState->getMinModeFilter(), SAMPLER_STATE::MIN_MODE_FILTER_LINEAR);
    ASSERT_EQ(samplerState->getMagModeFilter(), SAMPLER_STATE::MAG_MODE_FILTER_LINEAR);
}

HWTEST2_F(ImageCreate, GivenBindlessSampledImageViewFromUnsampledImageWhenInitializedThenSamplerStateCopiedToSSH, ImageSupport) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_sampler_desc_t samplerDesc = {
        ZE_STRUCTURE_TYPE_SAMPLER_DESC,
        nullptr,
        ZE_SAMPLER_ADDRESS_MODE_CLAMP,
        ZE_SAMPLER_FILTER_MODE_LINEAR,
        false};

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  &bindlessExtDesc,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &srcImgDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_TRUE(imageHW->bindlessImage);
    EXPECT_FALSE(imageHW->sampledImage);

    ze_image_handle_t imageView = {};
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS | ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;
    bindlessExtDesc.pNext = &samplerDesc;
    ret = imageHW->createView(device, &srcImgDesc, &imageView);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imageViewObject = static_cast<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>> *>(Image::fromHandle(imageView));

    EXPECT_TRUE(imageViewObject->bindlessImage);
    EXPECT_TRUE(imageViewObject->sampledImage);

    EXPECT_EQ(ZE_SAMPLER_ADDRESS_MODE_CLAMP, imageViewObject->samplerDesc.addressMode);
    EXPECT_EQ(ZE_SAMPLER_FILTER_MODE_LINEAR, imageViewObject->samplerDesc.filterMode);

    EXPECT_EQ(imageHW->imgInfo.slicePitch, imageViewObject->getImageInfo().slicePitch);
    EXPECT_NE(0u, imageViewObject->getImageInfo().slicePitch);
    EXPECT_EQ(imageHW->imgInfo.rowPitch, imageViewObject->getImageInfo().rowPitch);
    EXPECT_NE(0u, imageViewObject->getImageInfo().rowPitch);
    EXPECT_EQ(imageHW->imgInfo.offset, imageViewObject->getImageInfo().offset);
    EXPECT_EQ(imageHW->imgInfo.size, imageViewObject->getImageInfo().size);
    EXPECT_NE(0u, imageViewObject->getImageInfo().size);
    EXPECT_EQ(imageHW->imgInfo.linearStorage, imageViewObject->getImageInfo().linearStorage);

    auto ssHeapInfo = imageViewObject->getBindlessSlot();
    ASSERT_NE(nullptr, ssHeapInfo);
    EXPECT_NE(imageHW->getBindlessSlot(), ssHeapInfo);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto *surfaceState = static_cast<RENDER_SURFACE_STATE *>(ssHeapInfo->ssPtr);
    ASSERT_EQ(surfaceState->getSurfaceType(), RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D);
    ASSERT_EQ(surfaceState->getSurfaceFormat(), RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UINT);
    ASSERT_EQ(surfaceState->getWidth(), width);
    ASSERT_EQ(surfaceState->getHeight(), height);
    ASSERT_EQ(surfaceState->getDepth(), depth);

    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    auto *samplerState = static_cast<SAMPLER_STATE *>(ptrOffset(ssHeapInfo->ssPtr, sizeof(RENDER_SURFACE_STATE) * NEO::BindlessImageSlot::sampler));

    ASSERT_EQ(samplerState->getTcxAddressControlMode(), SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP);
    ASSERT_EQ(samplerState->getTcyAddressControlMode(), SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP);
    ASSERT_EQ(samplerState->getTczAddressControlMode(), SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP);

    ASSERT_EQ(samplerState->getMinModeFilter(), SAMPLER_STATE::MIN_MODE_FILTER_LINEAR);
    ASSERT_EQ(samplerState->getMagModeFilter(), SAMPLER_STATE::MAG_MODE_FILTER_LINEAR);

    Image::fromHandle(imageView)->destroy();
}

HWTEST2_F(ImageCreate, given2DImageFormatWithPixelSizeOf3BytesWhenRowPitchIsQueriedThenCorrectRowPitchIsReturned, ImageSupport) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_2D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
    desc.width = 12;
    desc.height = 12;
    desc.depth = 1;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    size_t rowPitch = 0;
    uint32_t pixelSizeInBytes = 3;
    imageHW->getPitchFor2dImage(device->toHandle(), desc.width, desc.height, pixelSizeInBytes, &rowPitch);
    EXPECT_EQ(rowPitch, imageHW->imgInfo.rowPitch);
}

HWTEST2_F(ImageCreate, given2DImageFormatWithPixelSizeOf6BytesWhenRowPitchIsQueriedThenCorrectRowPitchIsReturned, ImageSupport) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_2D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_16_16_16;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_FLOAT;
    desc.width = 12;
    desc.height = 12;
    desc.depth = 1;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    size_t rowPitch = 0;
    uint32_t pixelSizeInBytes = 6;
    imageHW->getPitchFor2dImage(device->toHandle(), desc.width, desc.height, pixelSizeInBytes, &rowPitch);
    EXPECT_EQ(rowPitch, imageHW->imgInfo.rowPitch);
}

HWTEST_F(ImageCreate, givenValidImageDescriptionFor3ChannelWhenImageCreateThenImageIsCreatedCorrectly) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 100u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_8_8_8,
                     ZE_IMAGE_FORMAT_TYPE_UNORM,
                     ZE_IMAGE_FORMAT_SWIZZLE_R,
                     ZE_IMAGE_FORMAT_SWIZZLE_G,
                     ZE_IMAGE_FORMAT_SWIZZLE_B,
                     ZE_IMAGE_FORMAT_SWIZZLE_1};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    std::unique_ptr<L0::Image> image(imagePtr);

    ASSERT_NE(image, nullptr);
}

HWTEST_F(ImageCreate, givenValidImageDescriptionFor3Channel16BitFloatWhenImageCreateThenImageIsCreatedCorrectly) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 100u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_16_16_16,
                     ZE_IMAGE_FORMAT_TYPE_FLOAT,
                     ZE_IMAGE_FORMAT_SWIZZLE_R,
                     ZE_IMAGE_FORMAT_SWIZZLE_G,
                     ZE_IMAGE_FORMAT_SWIZZLE_B,
                     ZE_IMAGE_FORMAT_SWIZZLE_1};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    std::unique_ptr<L0::Image> image(imagePtr);

    ASSERT_NE(image, nullptr);
}

HWTEST_F(ImageCreateExternalMemoryTest, givenNTHandleWhenCreatingInteropImageThenSuccessIsReturned) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    desc.pNext = &importNTHandle;

    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    std::unique_ptr<L0::Image> image(imagePtr);

    ASSERT_NE(image, nullptr);
}

HWTEST_F(ImageCreate, givenFDWhenCreatingImageWith3Channel8bitUintThenSuccessIsReturned) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;

    ze_external_memory_import_fd_t importFd = {};
    importFd.fd = 1;
    importFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    importFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    desc.pNext = &importFd;

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    std::unique_ptr<L0::Image> image(imagePtr);

    ASSERT_NE(image, nullptr);
}

HWTEST_F(ImageCreateExternalMemoryTest, givenNtHandleWhenCreatingImageWith3Channel8bitUintThenNotSupportedIsReturned) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;

    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    desc.pNext = &importNTHandle;

    Image *imagePtr = nullptr;
    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    std::unique_ptr<L0::Image> image(imagePtr);
    EXPECT_EQ(image, nullptr);
}

HWTEST_F(ImageCreate, givenFDWhenCreatingImageWith3Channel16bitUintThenSuccessIsReturned) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_16_16_16;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;

    ze_external_memory_import_fd_t importFd = {};
    importFd.fd = 1;
    importFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    importFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    desc.pNext = &importFd;

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    std::unique_ptr<L0::Image> image(imagePtr);

    ASSERT_NE(image, nullptr);
}

HWTEST_F(ImageCreate, givenValidImageDescriptionFor3Channel32BitFloatWhenImageCreateThenUnsupportedIsReturned) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 100u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32_32_32,
                     ZE_IMAGE_FORMAT_TYPE_FLOAT,
                     ZE_IMAGE_FORMAT_SWIZZLE_R,
                     ZE_IMAGE_FORMAT_SWIZZLE_G,
                     ZE_IMAGE_FORMAT_SWIZZLE_B,
                     ZE_IMAGE_FORMAT_SWIZZLE_1};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    std::unique_ptr<L0::Image> image(imagePtr);

    EXPECT_EQ(image, nullptr);
}

HWTEST_F(ImageView, given3ChannelImageWhenCreateImageViewWithNtHandleIsCalledThenNotSupportedIsReturned) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_1},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    Image *imagePtr;
    auto result = Image::create(productFamily, device, &srcImgDesc, &imagePtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_image_handle_t imgHandle = imagePtr->toHandle();

    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    srcImgDesc.pNext = &importNTHandle;

    ze_image_handle_t viewHandle;
    auto ret = L0::Image::fromHandle(imgHandle)->createView(device, &srcImgDesc, &viewHandle);
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);

    zeImageDestroy(imgHandle);
}

} // namespace ult
} // namespace L0
