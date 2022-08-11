/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_formats.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"

#include "third_party/opencl_headers/CL/cl_ext.h"

namespace L0 {
namespace ult {

using ImageCreate = Test<DeviceFixture>;
using ImageView = Test<DeviceFixture>;

HWTEST2_F(ImageCreate, givenValidImageDescriptionWhenImageCreateThenImageIsCreatedCorrectly, IsAtLeastSkl) {
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
    EXPECT_EQ(imageInfo.imgDesc.imageType, NEO::ImageType::Image2DArray);
    EXPECT_EQ(imageInfo.imgDesc.imageWidth, zeDesc.width);
    EXPECT_EQ(imageInfo.imgDesc.numMipLevels, zeDesc.miplevels);
    EXPECT_EQ(imageInfo.imgDesc.numSamples, 0u);
    EXPECT_EQ(imageInfo.baseMipLevel, 0u);
    EXPECT_EQ(imageInfo.linearStorage, false);
    EXPECT_EQ(imageInfo.mipCount, 0u);
    EXPECT_EQ(imageInfo.plane, GMM_NO_PLANE);
    EXPECT_EQ(imageInfo.useLocalMemory, false);
}

HWTEST2_F(ImageCreate, givenValidImageDescriptionWhenImageCreateWithUnsupportedImageThenNullPtrImageIsReturned, IsAtLeastSkl) {
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

HWTEST2_F(ImageCreate, givenDifferentSwizzleFormatWhenImageInitializeThenCorrectSwizzleInRSSIsSet, IsAtLeastSkl) {
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

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
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

HWTEST2_F(ImageView, givenPlanarImageWhenCreateImageViewThenProperPlaneIsCreated, IsAtLeastSkl) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
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

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
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

    auto nv12Allocation = imageHW->getAllocation();

    auto planeYAllocation = Image::fromHandle(planeY)->getAllocation();
    auto planeUVAllocation = Image::fromHandle(planeUV)->getAllocation();

    EXPECT_EQ(nv12Allocation->getGpuBaseAddress(), planeYAllocation->getGpuBaseAddress());
    EXPECT_EQ(nv12Allocation->getGpuBaseAddress(), planeUVAllocation->getGpuBaseAddress());

    zeImageDestroy(planeY);
    zeImageDestroy(planeUV);
}

HWTEST2_F(ImageView, givenPlanarImageWhenCreateImageWithInvalidStructViewThenProperErrorIsReturned, IsAtLeastSkl) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
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

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
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

HWTEST2_F(ImageCreate, givenFDWhenCreatingImageThenSuccessIsReturned, IsAtLeastSkl) {
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

    ze_external_memory_import_fd_t importFd = {};
    importFd.fd = 1;
    importFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    importFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    desc.pNext = &importFd;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_EQ(static_cast<int>(imageHW->getAllocation()->peekSharedHandle()), importFd.fd);
}

HWTEST2_F(ImageCreate, givenOpaqueFdWhenCreatingImageThenUnsuportedErrorIsReturned, IsAtLeastSkl) {
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

    ze_external_memory_import_fd_t importFd = {};
    importFd.fd = 1;
    importFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    importFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    desc.pNext = &importFd;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, ret);
}

HWTEST2_F(ImageCreate, givenExportStructWhenCreatingImageThenUnsupportedErrorIsReturned, IsAtLeastSkl) {
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

    ze_external_memory_export_fd_t exportFd = {};
    exportFd.fd = 1;
    exportFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    desc.pNext = &exportFd;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, ret);
}

class MemoryManagerNTHandleMock : public NEO::OsAgnosticMemoryManager {
  public:
    MemoryManagerNTHandleMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::OsAgnosticMemoryManager(executionEnvironment) {}

    NEO::GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override {
        auto graphicsAllocation = createMemoryAllocation(AllocationType::INTERNAL_HOST_MEMORY, nullptr, reinterpret_cast<void *>(1), 1,
                                                         4096u, reinterpret_cast<uint64_t>(handle), MemoryPool::SystemCpuInaccessible,
                                                         rootDeviceIndex, false, false, false);
        graphicsAllocation->setSharedHandle(static_cast<osHandle>(reinterpret_cast<uint64_t>(handle)));
        graphicsAllocation->set32BitAllocation(false);
        graphicsAllocation->setDefaultGmm(new Gmm(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
        return graphicsAllocation;
    }
};

HWTEST2_F(ImageCreate, givenNTHandleWhenCreatingImageThenSuccessIsReturned, IsAtLeastSkl) {
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

    uint64_t imageHandle = 0x1;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    desc.pNext = &importNTHandle;

    delete driverHandle->svmAllocsManager;
    execEnv->memoryManager.reset(new MemoryManagerNTHandleMock(*execEnv));
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_EQ(imageHW->getAllocation()->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));

    imageHW.reset(nullptr);
}

HWTEST2_F(ImageCreate, givenNTHandleWhenCreatingNV12ImageThenSuccessIsReturnedAndUVOffsetIsSet, IsAtLeastSkl) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
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

    delete driverHandle->svmAllocsManager;
    execEnv->memoryManager.reset(new MemoryManagerNTHandleMock(*execEnv));
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
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

HWTEST2_F(ImageCreate, givenImageDescWhenFailImageAllocationThenProperErrorIsReturned, IsAtLeastSkl) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
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

    auto isHexadecimalArrayPrefered = NEO::HwHelper::get(NEO::defaultHwInfo->platform.eRenderCoreFamily).isSipKernelAsHexadecimalArrayPreferred();
    if (isHexadecimalArrayPrefered) {
        backupSipInitType = true;
    }

    delete driverHandle->svmAllocsManager;
    execEnv->memoryManager.reset(new FailMemoryManagerMock(*execEnv));
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    L0::Image *imageHandle = nullptr;
    static_cast<FailMemoryManagerMock *>(execEnv->memoryManager.get())->fail = true;
    auto ret = L0::Image::create(neoDevice->getHardwareInfo().platform.eProductFamily, device, &desc, &imageHandle);

    ASSERT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, ret);
    EXPECT_EQ(imageHandle, nullptr);
}

HWTEST2_F(ImageCreate, givenMediaBlockOptionWhenCopySurfaceStateThenSurfaceStateIsSet, IsAtLeastSkl) {
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

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto surfaceState = &imageHW->surfaceState;

    RENDER_SURFACE_STATE rss = {};

    imageHW->copySurfaceStateToSSH(&rss, 0u, true);

    EXPECT_EQ(surfaceState->getWidth(), (static_cast<uint32_t>(imageHW->getImageInfo().surfaceFormat->ImageElementSizeInBytes) * static_cast<uint32_t>(imageHW->getImageInfo().imgDesc.imageWidth)) / sizeof(uint32_t));
}

HWTEST2_P(TestImageFormats, givenValidLayoutAndTypeWhenCreateImageCoreFamilyThenValidImageIsCreated, IsAtLeastSkl) {
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

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();

    imageHW->initialize(device, &zeDesc);

    EXPECT_EQ(imageHW->getAllocation()->getAllocationType(), NEO::AllocationType::IMAGE);
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

    EXPECT_EQ(rss.getSurfaceMinLod(), 0u);
    EXPECT_EQ(rss.getMipCountLod(), 0u);

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

INSTANTIATE_TEST_CASE_P(
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
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_HALF_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_HALF_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_FLOAT));
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
    swizzles ref{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A};
    EXPECT_FALSE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_FALSE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_FALSE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_FALSE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_TRUE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A}));
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

HWTEST2_F(ImageGetMemoryProperties, givenImageMemoryPropertiesExpStructureWhenGetMemroyPropertiesThenProperDataAreSet, IsAtLeastSkl) {
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

    EXPECT_EQ(imageInfo.surfaceFormat->ImageElementSizeInBytes, imageMemoryPropertiesExp.size);
    EXPECT_EQ(imageInfo.slicePitch, imageMemoryPropertiesExp.slicePitch);
    EXPECT_EQ(imageInfo.rowPitch, imageMemoryPropertiesExp.rowPitch);
}

HWTEST2_F(ImageGetMemoryProperties, givenDebugFlagSetWhenCreatingImageThenEnableCompression, IsAtLeastSkl) {
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

        EXPECT_EQ(L0HwHelperHw<FamilyType>::get().imageCompressionSupported(device->getHwInfo()), image->getAllocation()->isCompressionEnabled());
    }

    {
        NEO::DebugManager.flags.RenderCompressedImagesEnabled.set(1);

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
        NEO::DebugManager.flags.RenderCompressedImagesEnabled.set(1);

        Image *imagePtr = nullptr;
        auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_NE(nullptr, imagePtr);
        std::unique_ptr<L0::Image> image(imagePtr);

        EXPECT_TRUE(image->getAllocation()->isCompressionEnabled());
    }

    {
        NEO::DebugManager.flags.RenderCompressedImagesEnabled.set(0);

        Image *imagePtr = nullptr;
        auto result = Image::create(productFamily, device, &zeDesc, &imagePtr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_NE(nullptr, imagePtr);
        std::unique_ptr<L0::Image> image(imagePtr);

        EXPECT_FALSE(image->getAllocation()->isCompressionEnabled());
    }
}

HWTEST2_F(ImageGetMemoryProperties, givenDebugFlagSetWhenCreatingLinearImageThenDontEnableCompression, IsAtLeastSkl) {
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
    image->copySurfaceStateToSSH(&renderSurfaceState, 0u, false);

    EXPECT_EQ(1u, renderSurfaceState.getWidth());
    EXPECT_EQ(1u, renderSurfaceState.getHeight());
    EXPECT_EQ(1u, renderSurfaceState.getDepth());

    image->destroy();
}

using IsAtMostProductDG2 = IsWithinProducts<IGFX_SKYLAKE, IGFX_DG2>;

HWTEST2_F(ImageCreate, WhenGettingImagePropertiesThenFilterFlagsAreValid, IsAtMostProductDG2) {
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

HWTEST2_F(ImageCreate, WhenDestroyingImageThenSuccessIsReturned, IsAtMostProductDG2) {
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

HWTEST2_F(ImageCreate, WhenCreatingImageThenNonNullPointerIsReturned, IsAtMostProductDG2) {
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    L0::Image *imagePtr;

    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto image = whiteboxCast(imagePtr);
    ASSERT_NE(nullptr, image);

    image->destroy();
}

HWTEST2_F(ImageCreate, givenInvalidProductFamilyThenNullIsReturned, IsAtMostProductDG2) {
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    L0::Image *imagePtr;

    auto result = Image::create(IGFX_UNKNOWN, device, &desc, &imagePtr);
    ASSERT_NE(ZE_RESULT_SUCCESS, result);

    auto image = whiteboxCast(imagePtr);
    ASSERT_EQ(nullptr, image);
}

HWTEST2_F(ImageCreate, WhenImageIsCreatedThenDescMatchesAllocation, IsAtMostProductDG2) {
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

HWTEST2_F(ImageCreate, WhenImageIsCreatedThenDescMatchesSurface, IsAtMostProductDG2) {
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

    auto imageCore = new WhiteBox<ImageCoreFamily<gfxCoreFamily>>();
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

HWTEST2_F(ImageCreate, WhenImageIsCreatedThenDescSwizzlesMatchSurface, IsAtMostProductDG2) {
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

    auto imageCore = new WhiteBox<ImageCoreFamily<gfxCoreFamily>>();
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

HWTEST2_F(ImageCreate, WhenImageIsCreatedThenDescMatchesSurfaceFormats, IsAtMostProductDG2) {
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

    // Some formats aren't compilable on all generations, so for those we
    // skip the format check and rely on gen-specific tests defined elsewhere.
    static const typename RENDER_SURFACE_STATE::SURFACE_FORMAT noFormatCheck =
        static_cast<typename RENDER_SURFACE_STATE::SURFACE_FORMAT>(0xffff);

    struct FormatInfo {
        size_t elemBitSize;
        ze_image_format_layout_t formatLayout;
        ze_image_format_type_t formatType;
        SURFACE_FORMAT ssFormat;
        bool requireEven;
        GFXCORE_FAMILY minGen;
    };
    struct FormatInfo testFormats[] = {
        {sizeof(uint8_t) * 8, ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_UINT, false, IGFX_GEN9_CORE},
        {sizeof(uint32_t) * 4 * 8, ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_UINT, false, IGFX_GEN9_CORE},
        {sizeof(uint8_t) * 4 * 8, ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM, false, IGFX_GEN9_CORE},
        {sizeof(int16_t) * 8, ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SNORM,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_SNORM, false, IGFX_GEN9_CORE},
        {sizeof(float) * 4 * 8, ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_FLOAT, false, IGFX_GEN9_CORE},
        {sizeof(uint32_t) * 8, ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_UINT,
         RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UINT, false, IGFX_GEN10_CORE}};
    size_t numFormats = sizeof(testFormats) / sizeof(struct FormatInfo);

    for (size_t i = 0; i < numFormats; i++) {
        for (int j = 0; j < 2; j++) {
            bool odd = (j == 0);

            if (odd && testFormats[i].requireEven) {
                continue;
            }

            if (gfxCoreFamily < testFormats[i].minGen) {
                continue;
            }

            ze_image_desc_t *desc = odd ? &descOdd : &descEven;

            desc->format.layout = testFormats[i].formatLayout;
            desc->format.type = testFormats[i].formatType;

            auto imageCore = new WhiteBox<ImageCoreFamily<gfxCoreFamily>>();
            ze_result_t ret = imageCore->initialize(device, desc);
            ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

            auto surfaceState = &imageCore->surfaceState;

            if (testFormats[i].ssFormat != noFormatCheck) {
                ASSERT_EQ(surfaceState->getSurfaceFormat(), testFormats[i].ssFormat);
            }

            delete imageCore;
        }
    }
}

HWTEST2_F(ImageCreate, WhenCopyingToSshThenSurfacePropertiesAreRetained, IsAtMostProductDG2) {
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

    auto imageA = new ImageCoreFamily<gfxCoreFamily>();
    ze_result_t ret = imageA->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_32_32;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = desc.format.y = desc.format.z = desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.width = 10;
    desc.height = 10;

    auto imageB = new ImageCoreFamily<gfxCoreFamily>();
    ret = imageB->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    imageA->copySurfaceStateToSSH(mockSSH, 0, false);
    imageB->copySurfaceStateToSSH(mockSSH, sizeof(RENDER_SURFACE_STATE), false);

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

} // namespace ult
} // namespace L0
