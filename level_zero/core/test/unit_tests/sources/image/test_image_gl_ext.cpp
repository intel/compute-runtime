/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/image/internal_core_image_ext.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"

namespace L0 {
namespace ult {

class MemoryManagerGlExtNTHandleMock : public NEO::OsAgnosticMemoryManager {
  public:
    MemoryManagerGlExtNTHandleMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::OsAgnosticMemoryManager(executionEnvironment) {}

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

class ImageCreateGlTextureExtFixture : public DeviceFixtureWithCustomMemoryManager<MemoryManagerGlExtNTHandleMock> {
  public:
    void setUp() {
        DeviceFixtureWithCustomMemoryManager<MemoryManagerGlExtNTHandleMock>::setUp();

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

        delete driverHandle->svmAllocsManager;
        driverHandle->setMemoryManager(execEnv->memoryManager.get());
        driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());
    }

    void tearDown() {
        DeviceFixtureWithCustomMemoryManager<MemoryManagerGlExtNTHandleMock>::tearDown();
    }

    ze_image_desc_t desc = {};
    uint64_t imageHandle = 0x1;
};

using ImageCreateGlTextureExtTest = Test<ImageCreateGlTextureExtFixture>;

HWTEST_F(ImageCreateGlTextureExtTest, givenGlTextureExtDescWhenCreatingImageThenSuccessIsReturnedAndSharedHandleIsSet) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    ze_external_gl_texture_ext_desc_t glTextureExtDesc = {};
    glTextureExtDesc.pNext = &importNTHandle;
    glTextureExtDesc.pGmmResInfo = nullptr;
    glTextureExtDesc.textureBufferOffset = 0u;
    glTextureExtDesc.glHWFormat = 0u;
    glTextureExtDesc.isAuxEnabled = false;

    desc.pNext = &glTextureExtDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_NE(nullptr, imageHW->getAllocation());
    ASSERT_EQ(imageHW->getAllocation()->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateGlTextureExtTest, givenGlTextureExtDescWhenCreatingImageThenImgInfoOffsetsAreResetToZero) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    ze_external_gl_texture_ext_desc_t glTextureExtDesc = {};
    glTextureExtDesc.pNext = &importNTHandle;

    desc.pNext = &glTextureExtDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    // For GL shared-handle imports, imgInfo offsets must be reset to zero so that the
    // surface state is programmed against the imported allocation's base address with
    // no intra-resource shift.
    EXPECT_EQ(0u, imageHW->imgInfo.offset);
    EXPECT_EQ(0u, imageHW->imgInfo.xOffset);
    EXPECT_EQ(0u, imageHW->imgInfo.yOffset);
    EXPECT_EQ(0u, imageHW->imgInfo.yOffsetForUVPlane);

    EXPECT_EQ(imageHW->getAllocation()->getGpuAddress(), imageHW->surfaceState.getSurfaceBaseAddress());

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateGlTextureExtTest, givenGlTextureExtDescWithNonZeroTextureBufferOffsetWhenCreatingImageThenSuccessIsReturned) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    ze_external_gl_texture_ext_desc_t glTextureExtDesc = {};
    glTextureExtDesc.pNext = &importNTHandle;
    glTextureExtDesc.textureBufferOffset = 0u;

    desc.pNext = &glTextureExtDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_EQ(imageHW->getAllocation()->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateGlTextureExtTest, givenGlTextureExtDescWithCubeFaceIndexWhenCreatingImageThenMinimumArrayElementMatchesCubeFace) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    ze_external_gl_texture_ext_desc_t glTextureExtDesc = {};
    glTextureExtDesc.pNext = &importNTHandle;
    glTextureExtDesc.cubeFaceIndex = static_cast<uint32_t>(__GMM_CUBE_FACE_POS_Y);

    desc.type = ZE_IMAGE_TYPE_2D;
    desc.depth = 1;
    desc.pNext = &glTextureExtDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(static_cast<uint32_t>(__GMM_CUBE_FACE_POS_Y), imageHW->surfaceState.getMinimumArrayElement());
    EXPECT_TRUE(imageHW->surfaceState.getSurfaceArray());
    EXPECT_EQ(static_cast<uint32_t>(__GMM_CUBE_FACE_POS_Y), imageHW->redescribedSurfaceState.getMinimumArrayElement());
    EXPECT_TRUE(imageHW->redescribedSurfaceState.getSurfaceArray());

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateGlTextureExtTest, givenGlTextureExtDescWithNoCubeMapWhenCreatingImageThenCubeBranchIsNotEntered) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    ze_external_gl_texture_ext_desc_t glTextureExtDesc = {};
    glTextureExtDesc.pNext = &importNTHandle;
    glTextureExtDesc.cubeFaceIndex = static_cast<uint32_t>(__GMM_NO_CUBE_MAP);

    desc.type = ZE_IMAGE_TYPE_2DARRAY;
    desc.depth = 1;
    desc.arraylevels = 12;
    desc.pNext = &glTextureExtDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(0u, imageHW->surfaceState.getMinimumArrayElement());
    EXPECT_EQ(0u, imageHW->redescribedSurfaceState.getMinimumArrayElement());

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateGlTextureExtTest, givenGlTextureExtDescWithMsaaSamplesWhenCreatingImageThenMultisampleFieldsAreSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    ze_external_gl_texture_ext_desc_t glTextureExtDesc = {};
    glTextureExtDesc.pNext = &importNTHandle;
    glTextureExtDesc.numberOfSamples = 4;

    desc.type = ZE_IMAGE_TYPE_2DARRAY;
    desc.depth = 1;
    desc.arraylevels = 6;
    desc.pNext = &glTextureExtDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(4u, imageHW->numSamples);
    EXPECT_EQ(2u, imageHW->mcsMultisampleCount);
    EXPECT_EQ(nullptr, imageHW->mcsAllocation);
    EXPECT_FALSE(imageHW->isUnifiedMcsSurface);

    auto expectedMultisamples = static_cast<typename RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES>(2u);
    EXPECT_EQ(expectedMultisamples, imageHW->surfaceState.getNumberOfMultisamples());
    EXPECT_EQ(expectedMultisamples, imageHW->redescribedSurfaceState.getNumberOfMultisamples());

    EXPECT_EQ(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS,
              imageHW->surfaceState.getMultisampledSurfaceStorageFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS,
              imageHW->redescribedSurfaceState.getMultisampledSurfaceStorageFormat());

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateGlTextureExtTest, givenGlTextureExtDescWith16xMsaaSamplesWhenCreatingImageThenMultisampleCountIsCorrect) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    ze_external_gl_texture_ext_desc_t glTextureExtDesc = {};
    glTextureExtDesc.pNext = &importNTHandle;
    glTextureExtDesc.numberOfSamples = 16;

    desc.type = ZE_IMAGE_TYPE_2DARRAY;
    desc.depth = 1;
    desc.arraylevels = 12;
    desc.pNext = &glTextureExtDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(16u, imageHW->numSamples);
    EXPECT_EQ(4u, imageHW->mcsMultisampleCount);

    auto expectedMultisamples = static_cast<typename RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES>(4u);
    EXPECT_EQ(expectedMultisamples, imageHW->surfaceState.getNumberOfMultisamples());
    EXPECT_EQ(expectedMultisamples, imageHW->redescribedSurfaceState.getNumberOfMultisamples());

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateGlTextureExtTest, givenGlTextureExtDescWithMsaaAndMcsHandleWhenCreatingImageThenMcsAllocationIsCreated) {
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    uint64_t mcsHandle = 0x2;
    ze_external_gl_texture_ext_desc_t glTextureExtDesc = {};
    glTextureExtDesc.pNext = &importNTHandle;
    glTextureExtDesc.numberOfSamples = 4;
    glTextureExtDesc.mcsNtHandle = &mcsHandle;

    desc.type = ZE_IMAGE_TYPE_2DARRAY;
    desc.depth = 1;
    desc.arraylevels = 6;
    desc.pNext = &glTextureExtDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(4u, imageHW->numSamples);
    EXPECT_NE(nullptr, imageHW->mcsAllocation);
    EXPECT_FALSE(imageHW->isUnifiedMcsSurface);

    imageHW.reset(nullptr);
}

HWTEST_F(ImageCreateGlTextureExtTest, givenGlTextureExtDescWithNoMsaaWhenCreatingImageThenMultisampleFieldsAreDefault) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    ze_external_gl_texture_ext_desc_t glTextureExtDesc = {};
    glTextureExtDesc.pNext = &importNTHandle;
    glTextureExtDesc.numberOfSamples = 0;

    desc.pNext = &glTextureExtDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(0u, imageHW->numSamples);
    EXPECT_EQ(0u, imageHW->mcsMultisampleCount);
    EXPECT_EQ(nullptr, imageHW->mcsAllocation);
    EXPECT_FALSE(imageHW->isUnifiedMcsSurface);

    EXPECT_EQ(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1,
              imageHW->surfaceState.getNumberOfMultisamples());

    imageHW.reset(nullptr);
}

} // namespace ult
} // namespace L0
