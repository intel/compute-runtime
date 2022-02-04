/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gmock/gmock.h"

using namespace NEO;
using namespace ::testing;

class ImageSetArgTest : public ClDeviceFixture,
                        public testing::Test {

  public:
    ImageSetArgTest() = default;

  protected:
    template <typename FamilyType>
    void SetupChannels(int imgChannelOrder) {
        typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

        expectedChannelRed = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
        expectedChannelGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
        expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;

        if (imgChannelOrder == CL_A) {
            expectedChannelRed = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            expectedChannelGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
        } else if (imgChannelOrder == CL_RA ||
                   imgChannelOrder == CL_R ||
                   imgChannelOrder == CL_Rx) {
            expectedChannelGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
        } else if (imgChannelOrder == CL_RG ||
                   imgChannelOrder == CL_RGx) {
            expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
        }
    }

    void SetUp() override {
        ClDeviceFixture::SetUp();
        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        // define kernel info
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

        // setup kernel arg offsets
        pKernelInfo->addArgImage(0, 0x00);
        pKernelInfo->addArgImage(1, 0x40);

        program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
        retVal = CL_INVALID_VALUE;
        pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(program.get(), MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex), &retVal);
        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
        context = new MockContext(pClDevice);
        srcImage = Image3dHelper<>::create(context);
        srcAllocation = srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
        ASSERT_NE(nullptr, srcImage);

        expectedChannelRed = 0;
        expectedChannelGreen = 0;
        expectedChannelBlue = 0;
    }

    void TearDown() override {
        delete srcImage;
        delete pMultiDeviceKernel;

        delete context;
        ClDeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    std::unique_ptr<MockProgram> program;
    MockKernel *pKernel = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char surfaceStateHeap[0x80] = {};
    Image *srcImage = nullptr;
    GraphicsAllocation *srcAllocation = nullptr;
    int expectedChannelRed;
    int expectedChannelGreen;
    int expectedChannelBlue;
};

HWTEST_F(ImageSetArgTest, WhenSettingKernelArgImageThenSurfaceBaseAddressIsSetCorrectly) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    srcImage->setImageArg(const_cast<RENDER_SURFACE_STATE *>(surfaceState), false, 0, pClDevice->getRootDeviceIndex(), false);

    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(srcAllocation->getGpuAddress(), surfaceAddress);

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(0u, surfaces.size());
}

HWTEST_F(ImageSetArgTest, GivenMediaBlockImageWhenSettingImageArgThenCorrectValueIsSet) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE surfaceState;

    srcImage->setImageArg(&surfaceState, true, 0, pClDevice->getRootDeviceIndex(), false);

    auto computedWidth = surfaceState.getWidth();
    auto expectedWidth = (srcImage->getImageDesc().image_width * srcImage->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes) / sizeof(uint32_t);

    EXPECT_EQ(expectedWidth, computedWidth);
}

HWTEST_F(ImageSetArgTest, GivenNormalImageWhenSettingImageArgThenCorrectValueIsSet) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE surfaceState;

    srcImage->setImageArg(&surfaceState, true, 0, pClDevice->getRootDeviceIndex(), false);

    auto computedWidth = surfaceState.getWidth();

    EXPECT_EQ(srcImage->getImageDesc().image_width, computedWidth);
    EXPECT_EQ(0u, surfaceState.getMipCountLod());
}

HWTEST_F(ImageSetArgTest, givenImageWhenSettingMipTailStartLodThenProgramValueFromGmmResourceinfo) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily == IGFX_GEN8_CORE) {
        GTEST_SKIP();
    }

    RENDER_SURFACE_STATE surfaceState = {};
    const uint32_t mipTailStartLod = 4;

    auto gmm = srcAllocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    auto mockGmmResourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());

    mockGmmResourceInfo->setMipTailStartLod(mipTailStartLod);

    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);
    EXPECT_EQ(mipTailStartLod, surfaceState.getMipTailStartLod());

    // default value
    delete gmm;
    srcAllocation->setDefaultGmm(nullptr);

    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);
    EXPECT_EQ(0u, surfaceState.getMipTailStartLod());
}

HWTEST_F(ImageSetArgTest, givenCubeMapIndexWhenSetKernelArgImageIsCalledThenModifySurfaceState) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    uint32_t cubeFaceIndex = 2;

    Image *src2dImage = Image2dHelper<>::create(context);

    src2dImage->setCubeFaceIndex(cubeFaceIndex);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    src2dImage->setImageArg(const_cast<RENDER_SURFACE_STATE *>(surfaceState), false, 0, pClDevice->getRootDeviceIndex(), false);

    auto renderTargetViewExtent = surfaceState->getRenderTargetViewExtent();
    auto minimumArrayElement = surfaceState->getMinimumArrayElement();
    auto isImageArray = surfaceState->getSurfaceArray();
    auto depth = surfaceState->getDepth();

    EXPECT_EQ(renderTargetViewExtent, 1u);
    EXPECT_EQ(minimumArrayElement, cubeFaceIndex);
    EXPECT_EQ(depth, (__GMM_MAX_CUBE_FACE - cubeFaceIndex));
    EXPECT_TRUE(isImageArray);

    delete src2dImage;
}

struct ImageSetArgSurfaceArrayTest : ImageSetArgTest {
    template <typename FamilyType>
    void testSurfaceArrayProgramming(cl_mem_object_type imageType, size_t imageArraySize, bool expectedSurfaceArray) {
        using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
        RENDER_SURFACE_STATE surfaceState;

        cl_image_desc imageDesc = Image2dDefaults::imageDesc;
        imageDesc.image_array_size = imageArraySize;
        imageDesc.image_type = imageType;
        std::unique_ptr<Image> image{Image2dHelper<>::create(context, &imageDesc)};
        image->setCubeFaceIndex(__GMM_NO_CUBE_MAP);

        image->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);
        EXPECT_EQ(expectedSurfaceArray, surfaceState.getSurfaceArray());
    }
};

HWTEST_F(ImageSetArgSurfaceArrayTest, givenImage1DArrayAndImageArraySizeIsZeroWhenCallingSetImageArgThenDoNotProgramSurfaceArray) {
    testSurfaceArrayProgramming<FamilyType>(CL_MEM_OBJECT_IMAGE1D_ARRAY, 0u, false);
}

HWTEST_F(ImageSetArgSurfaceArrayTest, givenImage2DArrayAndImageArraySizeIsZeroWhenCallingSetImageArgThenDoNotProgramSurfaceArray) {
    testSurfaceArrayProgramming<FamilyType>(CL_MEM_OBJECT_IMAGE2D_ARRAY, 0u, false);
}

HWTEST_F(ImageSetArgSurfaceArrayTest, givenImage1DArrayAndImageArraySizeIsOneWhenCallingSetImageArgThenDoNotProgramSurfaceArray) {
    testSurfaceArrayProgramming<FamilyType>(CL_MEM_OBJECT_IMAGE1D_ARRAY, 1u, false);
}

HWTEST_F(ImageSetArgSurfaceArrayTest, givenImage2DArrayAndImageArraySizeIsOneWhenCallingSetImageArgThenDoNotProgramSurfaceArray) {
    testSurfaceArrayProgramming<FamilyType>(CL_MEM_OBJECT_IMAGE2D_ARRAY, 1u, false);
}

HWTEST_F(ImageSetArgSurfaceArrayTest, givenImage1DArrayAndImageArraySizeIsGreaterThanOneWhenCallingSetImageArgThenProgramSurfaceArray) {
    testSurfaceArrayProgramming<FamilyType>(CL_MEM_OBJECT_IMAGE1D_ARRAY, 2u, true);
}

HWTEST_F(ImageSetArgSurfaceArrayTest, givenImage2DArrayAndImageArraySizeIsGreaterThanOneWhenCallingSetImageArgThenProgramSurfaceArray) {
    testSurfaceArrayProgramming<FamilyType>(CL_MEM_OBJECT_IMAGE2D_ARRAY, 2u, true);
}

HWTEST_F(ImageSetArgSurfaceArrayTest, givenNonArrayImageWhenCallingSetImageArgThenDoNotProgramSurfaceArray) {
    testSurfaceArrayProgramming<FamilyType>(CL_MEM_OBJECT_IMAGE1D_BUFFER, 2u, false);
}

HWTEST_F(ImageSetArgTest, givenImageArraySizeGreaterThanOneButTypeIsNotImageArrayWhenCallingSetImageArgThenDoNotProgramSurfaceArray) {
    MockContext context;
    McsSurfaceInfo mcsSurfaceInfo = {};
    MockGraphicsAllocation *allocation = new MockGraphicsAllocation(0, 0x1000);
    ImageInfo imageInfo = {};

    ClSurfaceFormatInfo surfaceFormatInfo{};
    surfaceFormatInfo.surfaceFormat.GMMSurfaceFormat = GMM_FORMAT_B8G8R8A8_UNORM;
    surfaceFormatInfo.surfaceFormat.ImageElementSizeInBytes = 4u;
    imageInfo.surfaceFormat = &surfaceFormatInfo.surfaceFormat;
    cl_image_desc imageDesc = Image2dDefaults::imageDesc;
    imageDesc.image_array_size = 3u;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    imageInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imageInfo.plane = GMM_NO_PLANE;

    auto gmm = MockGmm::queryImgParams(context.getDevice(0)->getGmmClientContext(), imageInfo, false);
    allocation->setDefaultGmm(gmm.release());

    auto image = std::unique_ptr<Image>{Image::createSharedImage(
        &context,
        nullptr,
        mcsSurfaceInfo,
        GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation),
        nullptr,
        CL_MEM_READ_WRITE,
        0,
        &surfaceFormatInfo,
        imageInfo,
        0, 0, 0)};
    image->setCubeFaceIndex(__GMM_NO_CUBE_MAP);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState{};
    image->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);
    EXPECT_FALSE(surfaceState.getSurfaceArray());
}

HWTEST_F(ImageSetArgTest, givenNonCubeMapIndexWhenSetKernelArgImageIsCalledThenDontModifySurfaceState) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    EXPECT_EQ(srcImage->getCubeFaceIndex(), __GMM_NO_CUBE_MAP);
    srcImage->setImageArg(const_cast<RENDER_SURFACE_STATE *>(surfaceState), false, 0, pClDevice->getRootDeviceIndex(), false);

    auto renderTargetViewExtent = surfaceState->getRenderTargetViewExtent();
    auto minimumArrayElement = surfaceState->getMinimumArrayElement();
    auto isImageArray = surfaceState->getSurfaceArray();
    auto depth = surfaceState->getDepth();
    auto hAlign = static_cast<uint32_t>(surfaceState->getSurfaceHorizontalAlignment());
    auto vAlign = static_cast<uint32_t>(surfaceState->getSurfaceVerticalAlignment());

    auto expectedHAlign = static_cast<uint32_t>(MockGmmResourceInfo::getHAlignSurfaceStateResult);
    auto expectedVAlign = static_cast<uint32_t>(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);

    // 3D image
    EXPECT_EQ(renderTargetViewExtent, srcImage->getImageDesc().image_depth);
    EXPECT_EQ(minimumArrayElement, 0u);
    EXPECT_EQ(depth, srcImage->getImageDesc().image_depth);
    EXPECT_EQ(expectedHAlign, hAlign);
    EXPECT_EQ(expectedVAlign, vAlign);
    EXPECT_FALSE(isImageArray);
}

HWTEST_F(ImageSetArgTest, givenOffsetedBufferWhenSetKernelArgImageIscalledThenFullGPuPointerIsPatched) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    auto graphicsAllocation = srcAllocation;
    graphicsAllocation->setGpuBaseAddress(12345u);

    srcImage->setImageArg(const_cast<RENDER_SURFACE_STATE *>(surfaceState), false, 0, pClDevice->getRootDeviceIndex(), false);

    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(srcAllocation->getGpuAddress(), surfaceAddress);

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(0u, surfaces.size());
}

HWTEST_F(ImageSetArgTest, WhenSettingKernelArgThenPropertiesAreSetCorrectly) {
    auto gmmHelper = pDevice->getGmmHelper();
    auto imageMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE);
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    cl_mem memObj = srcImage;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    size_t rPitch = srcImage->getImageDesc().image_row_pitch;

    SetupChannels<FamilyType>(srcImage->getImageFormat().image_channel_order);

    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(srcAllocation->getGpuAddress(), surfaceAddress);
    EXPECT_EQ(srcImage->getImageDesc().image_width, surfaceState->getWidth());
    EXPECT_EQ(srcImage->getImageDesc().image_height, surfaceState->getHeight());
    EXPECT_EQ(srcImage->getImageDesc().image_depth, surfaceState->getDepth());
    EXPECT_EQ(srcImage->getImageDesc().image_depth, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(rPitch, surfaceState->getSurfacePitch());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);
    EXPECT_EQ(srcImage->getSurfaceFormatInfo().surfaceFormat.GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D, surfaceState->getSurfaceType());
    EXPECT_EQ(expectedChannelRed, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(expectedChannelGreen, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(expectedChannelBlue, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, surfaceState->getShaderChannelSelectAlpha());
    EXPECT_EQ(imageMocs, surfaceState->getMemoryObjectControlState());
    EXPECT_EQ(0u, surfaceState->getCoherencyType());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());

    for (auto &surface : surfaces) {
        delete surface;
    }
}

HWTEST_F(ImageSetArgTest, givenImage2DWithMipMapsWhenSetKernelArgIsCalledThenMipLevelAndMipCountIsSet) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    cl_mem memObj = srcImage;
    int mipLevel = 2;
    uint32_t mipCount = 3;
    srcImage->setBaseMipLevel(mipLevel);
    srcImage->setMipCount(mipCount);
    EXPECT_EQ(mipLevel, srcImage->peekBaseMipLevel());
    EXPECT_EQ(3u, srcImage->peekMipCount());

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));
    EXPECT_EQ((uint32_t)mipLevel, surfaceState->getSurfaceMinLod());
    EXPECT_EQ((uint32_t)mipCount, surfaceState->getMipCountLod() + 1);
}

HWTEST_F(ImageSetArgTest, Given2dArrayWhenSettingKernelArgThenPropertiesAreSetCorrectly) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    Image *image2Darray = Image2dArrayHelper<>::create(context);
    auto graphicsAllocation = image2Darray->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    cl_mem memObj = image2Darray;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    size_t rPitch = srcImage->getImageDesc().image_row_pitch;

    SetupChannels<FamilyType>(image2Darray->getImageFormat().image_channel_order);

    EXPECT_EQ(graphicsAllocation->getGpuAddress(), surfaceAddress);
    EXPECT_EQ(image2Darray->getImageDesc().image_width, surfaceState->getWidth());
    EXPECT_EQ(image2Darray->getImageDesc().image_height, surfaceState->getHeight());
    EXPECT_EQ(image2Darray->getImageDesc().image_array_size, surfaceState->getDepth());
    EXPECT_EQ(image2Darray->getImageDesc().image_array_size, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(rPitch, surfaceState->getSurfacePitch());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);
    EXPECT_EQ(image2Darray->getSurfaceFormatInfo().surfaceFormat.GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D, surfaceState->getSurfaceType());
    EXPECT_TRUE((GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceArray());

    EXPECT_EQ(expectedChannelRed, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(expectedChannelGreen, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(expectedChannelBlue, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, surfaceState->getShaderChannelSelectAlpha());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());
    for (auto &surface : surfaces) {
        delete surface;
    }
    delete image2Darray;
}

HWTEST_F(ImageSetArgTest, Given1dArrayWhenSettingKernelArgThenPropertiesAreSetCorrectly) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    Image *image1Darray = Image1dArrayHelper<>::create(context);
    auto graphicsAllocation = image1Darray->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    cl_mem memObj = image1Darray;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    SetupChannels<FamilyType>(image1Darray->getImageFormat().image_channel_order);

    EXPECT_EQ(graphicsAllocation->getGpuAddress(), surfaceAddress);
    EXPECT_EQ(image1Darray->getImageDesc().image_width, surfaceState->getWidth());
    EXPECT_EQ(1u, surfaceState->getHeight());
    EXPECT_EQ(image1Darray->getImageDesc().image_array_size, surfaceState->getDepth());
    EXPECT_EQ(image1Darray->getImageDesc().image_array_size, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(image1Darray->getImageDesc().image_row_pitch, surfaceState->getSurfacePitch());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);
    EXPECT_EQ(graphicsAllocation->getDefaultGmm()->queryQPitch(GMM_RESOURCE_TYPE::RESOURCE_1D), surfaceState->getSurfaceQpitch());

    EXPECT_EQ(image1Darray->getSurfaceFormatInfo().surfaceFormat.GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D, surfaceState->getSurfaceType());
    EXPECT_TRUE((GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceArray());

    EXPECT_EQ(expectedChannelRed, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(expectedChannelGreen, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(expectedChannelBlue, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, surfaceState->getShaderChannelSelectAlpha());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());
    for (auto &surface : surfaces) {
        delete surface;
    }
    delete image1Darray;
}

HWTEST_F(ImageSetArgTest, givenMcsAllocationWhenSetArgIsCalledWithoutUnifiedAuxCapabilityThenProgramAuxFieldsForMultisamples) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    mcsAlloc->setDefaultGmm(new Gmm(pDevice->getGmmClientContext(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;

    auto image = Image2dHelper<>::create(context, &imgDesc);
    image->setMcsSurfaceInfo(msi);
    image->setMcsAllocation(mcsAlloc);
    cl_mem memObj = image;

    EXPECT_FALSE(mcsAlloc->getDefaultGmm()->unifiedAuxTranslationCapable());

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    EXPECT_FALSE(Image::isDepthFormat(image->getImageFormat()));
    EXPECT_TRUE(surfaceState->getMultisampledSurfaceStorageFormat() ==
                RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);
    EXPECT_TRUE(surfaceState->getAuxiliarySurfaceMode() == (typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE)1);
    EXPECT_EQ(msi.pitch, surfaceState->getAuxiliarySurfacePitch());
    EXPECT_EQ(msi.qPitch, surfaceState->getAuxiliarySurfaceQpitch());
    EXPECT_EQ(msi.multisampleCount, static_cast<uint32_t>(surfaceState->getNumberOfMultisamples()));
    EXPECT_EQ(mcsAlloc->getGpuAddress(), surfaceState->getAuxiliarySurfaceBaseAddress());

    delete image;
}

HWTEST_F(ImageSetArgTest, givenDepthFormatWhenSetArgIsCalledThenProgramAuxFields) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    McsSurfaceInfo msi = {0, 0, 3};
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    cl_image_format imgFormat = {CL_DEPTH, CL_FLOAT};

    auto image = Image2dHelper<>::create(context, &imgDesc, &imgFormat);
    image->setMcsSurfaceInfo(msi);
    cl_mem memObj = image;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    EXPECT_TRUE(Image::isDepthFormat(image->getImageFormat()));
    EXPECT_TRUE(surfaceState->getMultisampledSurfaceStorageFormat() ==
                RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
    EXPECT_TRUE(surfaceState->getAuxiliarySurfaceMode() == AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(1u, surfaceState->getAuxiliarySurfacePitch());
    EXPECT_EQ(0u, surfaceState->getAuxiliarySurfaceQpitch());
    EXPECT_EQ(msi.multisampleCount, static_cast<uint32_t>(surfaceState->getNumberOfMultisamples()));
    EXPECT_EQ(0u, surfaceState->getAuxiliarySurfaceBaseAddress());

    delete image;
}

HWTEST_F(ImageSetArgTest, givenMultisampledR32Floatx8x24DepthStencilFormatWhenSetArgIsCalledThenSetMssSurfaceStateStorageParam) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;

    McsSurfaceInfo msi = {0, 0, 3};
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    cl_image_format imgFormat = {CL_DEPTH_STENCIL, CL_FLOAT};

    std::unique_ptr<Image> image(ImageHelper<ImageReadOnly<Image2dDefaults>>::create(context, &imgDesc, &imgFormat));
    image->setMcsSurfaceInfo(msi);
    cl_mem memObj = image.get();

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                           pKernelInfo->argAsImg(0).bindful));

    EXPECT_TRUE(Image::isDepthFormat(image->getImageFormat()));
    EXPECT_TRUE(surfaceState->getSurfaceFormat() == SURFACE_FORMAT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS);
    EXPECT_TRUE(surfaceState->getMultisampledSurfaceStorageFormat() ==
                RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);
}

HWTEST_F(ImageSetArgTest, givenMcsAllocationAndCompressionWhenSetArgOnMultisampledImgIsCalledThenProgramAuxFieldsWithMcsParams) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    mcsAlloc->setDefaultGmm(new Gmm(pDevice->getGmmClientContext(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;

    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(context, &imgDesc));
    auto graphicsAllocation = image->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    graphicsAllocation->getDefaultGmm()->isCompressionEnabled = true;
    image->setMcsSurfaceInfo(msi);
    image->setMcsAllocation(mcsAlloc);
    cl_mem memObj = image.get();

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                                 pKernelInfo->argAsImg(0).bindful));

    EXPECT_TRUE(surfaceState->getMultisampledSurfaceStorageFormat() ==
                RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);
    EXPECT_TRUE(surfaceState->getAuxiliarySurfaceMode() == (typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE)1);
    EXPECT_EQ(msi.pitch, surfaceState->getAuxiliarySurfacePitch());
    EXPECT_EQ(msi.qPitch, surfaceState->getAuxiliarySurfaceQpitch());
    EXPECT_EQ(msi.multisampleCount, static_cast<uint32_t>(surfaceState->getNumberOfMultisamples()));
    EXPECT_EQ(mcsAlloc->getGpuAddress(), surfaceState->getAuxiliarySurfaceBaseAddress());
}

HWTEST_F(ImageSetArgTest, givenDepthFormatAndCompressionWhenSetArgOnMultisampledImgIsCalledThenDontProgramAuxFields) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    McsSurfaceInfo msi = {0, 0, 3};
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    cl_image_format imgFormat = {CL_DEPTH, CL_FLOAT};
    imgDesc.num_samples = 8;

    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(context, &imgDesc, &imgFormat));
    auto graphicsAllocation = image->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    graphicsAllocation->getDefaultGmm()->isCompressionEnabled = true;
    image->setMcsSurfaceInfo(msi);
    cl_mem memObj = image.get();

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                                 pKernelInfo->argAsImg(0).bindful));

    EXPECT_TRUE(Image::isDepthFormat(image->getImageFormat()));
    EXPECT_TRUE(surfaceState->getMultisampledSurfaceStorageFormat() ==
                RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
    EXPECT_TRUE(surfaceState->getAuxiliarySurfaceMode() == AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(1u, surfaceState->getAuxiliarySurfacePitch());
    EXPECT_EQ(0u, surfaceState->getAuxiliarySurfaceQpitch());
    EXPECT_EQ(msi.multisampleCount, static_cast<uint32_t>(surfaceState->getNumberOfMultisamples()));
    EXPECT_EQ(0u, surfaceState->getAuxiliarySurfaceBaseAddress());
}

HWTEST_F(ImageSetArgTest, givenMcsAllocationWhenSetArgIsCalledWithUnifiedAuxCapabilityThenProgramAuxFieldsForCcs) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    mcsAlloc->setDefaultGmm(new Gmm(pDevice->getGmmClientContext(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;

    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(context, &imgDesc));
    image->setMcsSurfaceInfo(msi);
    image->setMcsAllocation(mcsAlloc);
    cl_mem memObj = image.get();

    auto mockMcsGmmResInfo = static_cast<MockGmmResourceInfo *>(mcsAlloc->getDefaultGmm()->gmmResourceInfo.get());
    mockMcsGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_TRUE(mcsAlloc->getDefaultGmm()->unifiedAuxTranslationCapable());

    mcsAlloc->getDefaultGmm()->isCompressionEnabled = true;

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                           pKernelInfo->argAsImg(0).bindful));

    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(surfaceState, mcsAlloc->getDefaultGmm()));
    EXPECT_EQ(1u, surfaceState->getAuxiliarySurfacePitch());
    EXPECT_EQ(0u, surfaceState->getAuxiliarySurfaceQpitch());
}

HWTEST_F(ImageSetArgTest, givenMcsAllocationWhenSetArgIsCalledWithUnifiedAuxCapabilityAndMcsThenAuxBaseAddressIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    mcsAlloc->setDefaultGmm(new Gmm(pDevice->getGmmClientContext(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;

    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(context, &imgDesc));
    image->setMcsSurfaceInfo(msi);
    image->setMcsAllocation(mcsAlloc);
    cl_mem memObj = image.get();

    auto mockMcsGmmResInfo = static_cast<MockGmmResourceInfo *>(mcsAlloc->getDefaultGmm()->gmmResourceInfo.get());
    mockMcsGmmResInfo->setUnifiedAuxTranslationCapable();
    mockMcsGmmResInfo->setMultisampleControlSurface();
    EXPECT_TRUE(mcsAlloc->getDefaultGmm()->unifiedAuxTranslationCapable());

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                                 pKernelInfo->argAsImg(0).bindful));

    EXPECT_NE(0u, surfaceState->getAuxiliarySurfaceBaseAddress());
}

HWTEST_F(ImageSetArgTest, givenMcsAllocationWhenSetArgIsCalledWithUnifiedAuxCapabilityAndMcsThenAuxSurfPitchAndQPitchIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    mcsAlloc->setDefaultGmm(new Gmm(pDevice->getGmmClientContext(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;

    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(context, &imgDesc));
    image->setMcsSurfaceInfo(msi);
    image->setMcsAllocation(mcsAlloc);
    cl_mem memObj = image.get();

    auto mockMcsGmmResInfo = static_cast<MockGmmResourceInfo *>(mcsAlloc->getDefaultGmm()->gmmResourceInfo.get());
    mockMcsGmmResInfo->setUnifiedAuxTranslationCapable();
    mockMcsGmmResInfo->setMultisampleControlSurface();

    uint32_t pitchValue = 4u;
    uint32_t qPitchValue = 12u;

    mockMcsGmmResInfo->setUnifiedAuxPitchTiles(pitchValue);
    mockMcsGmmResInfo->setAuxQPitch(qPitchValue);
    EXPECT_TRUE(mcsAlloc->getDefaultGmm()->unifiedAuxTranslationCapable());

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                                 pKernelInfo->argAsImg(0).bindful));

    EXPECT_EQ(pitchValue, surfaceState->getAuxiliarySurfacePitch());
    EXPECT_EQ(qPitchValue, surfaceState->getAuxiliarySurfaceQpitch());
}

HWTEST_F(ImageSetArgTest, GivenImageFrom1dBufferWhenSettingKernelArgThenPropertiesAreSetCorrectly) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    auto buffer = clCreateBuffer(context, 0, 4096 * 10, nullptr, nullptr);
    ASSERT_NE(nullptr, buffer);

    cl_image_desc imageDesc = {0};
    imageDesc.buffer = buffer;
    imageDesc.image_width = 6400;
    // 2 * (1 << 21) + 5 * (1 << 7) + 0;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;

    cl_image_format imageFormat = {0};
    imageFormat.image_channel_data_type = CL_FLOAT;
    imageFormat.image_channel_order = CL_RGBA;
    cl_int retVal;
    auto imageFromBuffer = Image::validateAndCreateImage(context, nullptr, 0, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, imageFromBuffer);

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(imageFromBuffer),
        &imageFromBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    auto image = castToObject<Image>(imageFromBuffer);

    EXPECT_EQ(image->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), surfaceAddress);
    // Width is 7 bits
    EXPECT_EQ(128u, surfaceState->getWidth());
    // Height is 14 bits
    EXPECT_EQ(50u, surfaceState->getHeight());
    // Depth is 11 bits
    EXPECT_EQ(1u, surfaceState->getDepth());

    EXPECT_EQ(1u, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);

    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch());
    EXPECT_EQ(image->getSurfaceFormatInfo().surfaceFormat.GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfaceState->getSurfaceType());
    EXPECT_FALSE((GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceArray());

    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, surfaceState->getShaderChannelSelectAlpha());

    clReleaseMemObject(imageFromBuffer);
    clReleaseMemObject(buffer);
}

HWTEST_F(ImageSetArgTest, GivenImageWithClLuminanceFormatWhenSettingKernelArgThenPropertiesAreSetCorrectly) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    Image *luminanceImage = Image3dHelper<LuminanceImage>::create(context);
    cl_mem memObj = luminanceImage;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));
    //for CL_LUMINANCE format we override channels to RED to be spec complaint.
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, surfaceState->getShaderChannelSelectAlpha());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());
    for (auto &surface : surfaces) {
        delete surface;
    }
    delete luminanceImage;
}

HWTEST_F(ImageSetArgTest, WhenSettingArgThenImageIsReturned) {
    cl_mem memObj = srcImage;

    retVal = pKernel->setArg(
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(memObj, pKernel->getKernelArg(0));

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());

    for (auto &surface : surfaces) {
        delete surface;
    }
}

HWTEST_F(ImageSetArgTest, givenCompressedResourceWhenSettingImgArgThenSetCorrectAuxParams) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;

    srcAllocation->getDefaultGmm()->isCompressionEnabled = true;

    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);

    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(&surfaceState, srcAllocation->getDefaultGmm()));
    EXPECT_EQ(1u, surfaceState.getAuxiliarySurfacePitch());
    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceQpitch());
}

HWTEST_F(ImageSetArgTest, givenNonCompressedResourceWhenSettingImgArgThenDontSetAuxParams) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    typedef typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE AUXILIARY_SURFACE_MODE;
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;

    auto gmm = srcAllocation->getDefaultGmm();
    auto mockGmmResInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    gmm->isCompressionEnabled = false;
    mockGmmResInfo->getUnifiedAuxSurfaceOffsetCalled = 0u;

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceQpitch());
    EXPECT_EQ(1u, surfaceState.getAuxiliarySurfacePitch());

    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);

    EXPECT_TRUE(surfaceState.getAuxiliarySurfaceMode() == AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(1u, surfaceState.getAuxiliarySurfacePitch());
    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceQpitch());
    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_EQ(0u, mockGmmResInfo->getUnifiedAuxSurfaceOffsetCalled);
}

/* cl_intel_media_block_io */

class ImageMediaBlockSetArgTest : public ImageSetArgTest {
  protected:
    void SetUp() override {
        ClDeviceFixture::SetUp();
        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        // define kernel info
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

        // setup kernel arg offsets
        pKernelInfo->addArgImage(0, 0x00, iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA_BLOCK);
        pKernelInfo->addArgImage(0, 0x40, iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA_BLOCK);

        program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
        retVal = CL_INVALID_VALUE;
        pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(program.get(), MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex), &retVal);
        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
        context = new MockContext(pClDevice);
        srcImage = Image3dHelper<>::create(context);
        srcAllocation = srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
        ASSERT_NE(nullptr, srcImage);
    }
};

HWTEST_F(ImageMediaBlockSetArgTest, WhenSettingKernelArgImageThenPropertiesAreCorrect) {
    auto gmmHelper = pDevice->getGmmHelper();
    auto imageMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE);
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    cl_mem memObj = srcImage;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    size_t rPitch = srcImage->getImageDesc().image_row_pitch;

    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(srcAllocation->getGpuAddress(), surfaceAddress);

    uint32_t element_size = static_cast<uint32_t>(srcImage->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes);

    SetupChannels<FamilyType>(srcImage->getImageFormat().image_channel_order);

    EXPECT_EQ(srcImage->getImageDesc().image_width * element_size / sizeof(uint32_t), surfaceState->getWidth());
    EXPECT_EQ(srcImage->getImageDesc().image_height, surfaceState->getHeight());
    EXPECT_EQ(srcImage->getImageDesc().image_depth, surfaceState->getDepth());
    EXPECT_EQ(srcImage->getImageDesc().image_depth, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(rPitch, surfaceState->getSurfacePitch());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);
    EXPECT_EQ(srcImage->getSurfaceFormatInfo().surfaceFormat.GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D, surfaceState->getSurfaceType());
    EXPECT_EQ(expectedChannelRed, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(expectedChannelGreen, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(expectedChannelBlue, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, surfaceState->getShaderChannelSelectAlpha());
    EXPECT_EQ(imageMocs, surfaceState->getMemoryObjectControlState());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());

    for (auto &surface : surfaces) {
        delete surface;
    }
}

typedef ImageSetArgTest ImageShaderChannelValueTest;

HWTEST_F(ImageShaderChannelValueTest, GivenChannelAWhenGettingShaderChannelValueThenOutputChannelIsCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;

    int outputChannel = 0;
    int inputChannel = 0;

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_A);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_A);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_A);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_A);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, outputChannel);
}

HWTEST_F(ImageShaderChannelValueTest, GivenChannelRaWhenGettingShaderChannelValueThenOutputChannelIsCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;

    int outputChannel = 0;
    int inputChannel = 0;

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_R);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_R);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_R);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_R);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_Rx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_Rx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_Rx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_Rx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, outputChannel);
}

HWTEST_F(ImageShaderChannelValueTest, GivenChannelRgaWhenGettingShaderChannelValueThenOutputChannelIsCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;

    int outputChannel = 0;
    int inputChannel = 0;

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RG);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RG);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RG);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RG);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN, outputChannel);
}

HWTEST_F(ImageShaderChannelValueTest, GivenChannelRgbaWhenGettingShaderChannelValueThenOutputChannelIsCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;

    int outputChannel = 0;
    int inputChannel = 0;

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGBA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGBA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGBA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGBA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE, outputChannel);
}

HWTEST_F(ImageSetArgTest, givenImageWithOffsetGreaterThan4GBWhenSurfaceStateIsProgrammedThenCorrectStataBaseAddressIsSet) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState;

    uint64_t surfaceOffset = 8 * GB;

    srcImage->setSurfaceOffsets(surfaceOffset, 0, 0, 0);
    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);

    auto expectedAddress = srcAllocation->getGpuAddress() + surfaceOffset;
    auto surfaceAddress = surfaceState.getSurfaceBaseAddress();

    EXPECT_EQ(expectedAddress, surfaceAddress);
}

HWTEST_F(ImageSetArgTest, givenMediaCompressedResourceThenSurfaceModeIsNone) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    RENDER_SURFACE_STATE surfaceState;

    auto gmm = srcAllocation->getDefaultGmm();

    gmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;
    gmm->isCompressionEnabled = true;
    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);

    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
}
