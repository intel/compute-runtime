/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "hw_cmds.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/surface.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"

#include "gmock/gmock.h"
#include "test.h"

using namespace OCLRT;
using namespace ::testing;

class ImageSetArgTest : public DeviceFixture,
                        public testing::Test {

  public:
    ImageSetArgTest()

    {
        memset(&kernelHeader, 0, sizeof(kernelHeader));
    }

  protected:
    template <typename FamilyType>
    void SetupChannels(int imgChannelOrder) {
        typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

        expectedChannelRed = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED;
        expectedChannelGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN_GREEN;
        expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE_BLUE;

        if (imgChannelOrder == CL_A) {
            expectedChannelRed = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO;
            expectedChannelGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN_ZERO;
            expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE_ZERO;
        } else if (imgChannelOrder == CL_RA ||
                   imgChannelOrder == CL_R ||
                   imgChannelOrder == CL_Rx) {
            expectedChannelGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN_ZERO;
            expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE_ZERO;
        } else if (imgChannelOrder == CL_RG ||
                   imgChannelOrder == CL_RGx) {
            expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE_ZERO;
        }
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        pKernelInfo = KernelInfo::create();

        // define kernel info
        kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
        pKernelInfo->usesSsh = true;

        // setup kernel arg offsets
        pKernelInfo->kernelArgInfo.resize(2);
        pKernelInfo->kernelArgInfo[1].offsetHeap = 0x00;
        pKernelInfo->kernelArgInfo[0].offsetHeap = 0x40;

        pKernelInfo->kernelArgInfo[1].isImage = true;
        pKernelInfo->kernelArgInfo[0].isImage = true;

        pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);
        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
        context = new MockContext(pDevice);
        srcImage = Image3dHelper<>::create(context);
        ASSERT_NE(nullptr, srcImage);

        expectedChannelRed = 0;
        expectedChannelGreen = 0;
        expectedChannelBlue = 0;
    }

    void TearDown() override {
        delete pKernelInfo;
        delete srcImage;
        delete pKernel;
        delete context;
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    MockProgram program;
    MockKernel *pKernel = nullptr;
    SKernelBinaryHeaderCommon kernelHeader;
    KernelInfo *pKernelInfo = nullptr;
    char surfaceStateHeap[0x80];
    Image *srcImage = nullptr;
    int expectedChannelRed;
    int expectedChannelGreen;
    int expectedChannelBlue;
};

HWTEST_F(ImageSetArgTest, setKernelArgImage) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    srcImage->setImageArg(const_cast<RENDER_SURFACE_STATE *>(surfaceState), false, 0);

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(srcImage->getCpuAddress(), surfaceAddress);

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(0u, surfaces.size());
}

HWTEST_F(ImageSetArgTest, setKernelArgImageUsingMediaBlockImage) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE surfaceState;

    srcImage->setImageArg(&surfaceState, true, 0);

    auto computedWidth = surfaceState.getWidth();
    auto expectedWidth = (srcImage->getImageDesc().image_width * srcImage->getSurfaceFormatInfo().ImageElementSizeInBytes) / sizeof(uint32_t);

    EXPECT_EQ(expectedWidth, computedWidth);
}

HWTEST_F(ImageSetArgTest, setKernelArgImageUsingNormalImage) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE surfaceState;

    srcImage->setImageArg(&surfaceState, true, 0);

    auto computedWidth = surfaceState.getWidth();

    EXPECT_EQ(srcImage->getImageDesc().image_width, computedWidth);
    EXPECT_EQ(0u, surfaceState.getMipCountLod());
}

HWTEST_F(ImageSetArgTest, givenCubeMapIndexWhenSetKernelArgImageIsCalledThenModifySurfaceState) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    uint32_t cubeFaceIndex = 2;

    Image *src2dImage = Image2dHelper<>::create(context);

    src2dImage->setCubeFaceIndex(cubeFaceIndex);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    src2dImage->setImageArg(const_cast<RENDER_SURFACE_STATE *>(surfaceState), false, 0);

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

HWTEST_F(ImageSetArgTest, givenNonCubeMapIndexWhenSetKernelArgImageIsCalledThenDontModifySurfaceState) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    EXPECT_EQ(srcImage->getCubeFaceIndex(), __GMM_NO_CUBE_MAP);
    srcImage->setImageArg(const_cast<RENDER_SURFACE_STATE *>(surfaceState), false, 0);

    auto renderTargetViewExtent = surfaceState->getRenderTargetViewExtent();
    auto minimumArrayElement = surfaceState->getMinimumArrayElement();
    auto isImageArray = surfaceState->getSurfaceArray();
    auto depth = surfaceState->getDepth();
    auto hAlign = static_cast<uint32_t>(surfaceState->getSurfaceHorizontalAlignment());
    auto vAlign = static_cast<uint32_t>(surfaceState->getSurfaceVerticalAlignment());

    auto expectedHAlign = static_cast<uint32_t>(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4);
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
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    auto graphicsAllocation = srcImage->getGraphicsAllocation();
    graphicsAllocation->gpuBaseAddress = 12345u;

    srcImage->setImageArg(const_cast<RENDER_SURFACE_STATE *>(surfaceState), false, 0);

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(srcImage->getCpuAddress(), surfaceAddress);

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(0u, surfaces.size());
}

HWTEST_F(ImageSetArgTest, clSetKernelArgImage) {
    auto imageMocs = Gmm::getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE);
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    cl_mem memObj = srcImage;

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    size_t rPitch = srcImage->getImageDesc().image_row_pitch;

    SetupChannels<FamilyType>(srcImage->getImageFormat().image_channel_order);

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(srcImage->getCpuAddress(), surfaceAddress);
    EXPECT_EQ(srcImage->getImageDesc().image_width, surfaceState->getWidth());
    EXPECT_EQ(srcImage->getImageDesc().image_height, surfaceState->getHeight());
    EXPECT_EQ(srcImage->getImageDesc().image_depth, surfaceState->getDepth());
    EXPECT_EQ(srcImage->getImageDesc().image_depth, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(rPitch, surfaceState->getSurfacePitch());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);
    EXPECT_EQ(srcImage->getSurfaceFormatInfo().GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D, surfaceState->getSurfaceType());
    EXPECT_EQ(expectedChannelRed, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(expectedChannelGreen, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(expectedChannelBlue, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA_ALPHA, surfaceState->getShaderChannelSelectAlpha());
    EXPECT_EQ(imageMocs, surfaceState->getMemoryObjectControlState());

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
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));
    EXPECT_EQ((uint32_t)mipLevel, surfaceState->getSurfaceMinLod());
    EXPECT_EQ((uint32_t)mipCount, surfaceState->getMipCountLod() + 1);
}

HWTEST_F(ImageSetArgTest, clSetKernelArgImage2Darray) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    Image *image2Darray = Image2dArrayHelper<>::create(context);
    cl_mem memObj = image2Darray;

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));
    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());

    size_t rPitch = srcImage->getImageDesc().image_row_pitch;

    SetupChannels<FamilyType>(image2Darray->getImageFormat().image_channel_order);

    EXPECT_EQ(image2Darray->getCpuAddress(), surfaceAddress);
    EXPECT_EQ(image2Darray->getImageDesc().image_width, surfaceState->getWidth());
    EXPECT_EQ(image2Darray->getImageDesc().image_height, surfaceState->getHeight());
    EXPECT_EQ(image2Darray->getImageDesc().image_array_size, surfaceState->getDepth());
    EXPECT_EQ(image2Darray->getImageDesc().image_array_size, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(rPitch, surfaceState->getSurfacePitch());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);
    EXPECT_EQ(image2Darray->getSurfaceFormatInfo().GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D, surfaceState->getSurfaceType());
    EXPECT_TRUE((GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceArray());

    EXPECT_EQ(expectedChannelRed, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(expectedChannelGreen, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(expectedChannelBlue, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA_ALPHA, surfaceState->getShaderChannelSelectAlpha());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());
    for (auto &surface : surfaces) {
        delete surface;
    }
    delete image2Darray;
}

HWTEST_F(ImageSetArgTest, clSetKernelArgImage1Darray) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    Image *image1Darray = Image1dArrayHelper<>::create(context);
    cl_mem memObj = image1Darray;

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));
    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());

    SetupChannels<FamilyType>(image1Darray->getImageFormat().image_channel_order);

    EXPECT_EQ(image1Darray->getCpuAddress(), surfaceAddress);
    EXPECT_EQ(image1Darray->getImageDesc().image_width, surfaceState->getWidth());
    EXPECT_EQ(1u, surfaceState->getHeight());
    EXPECT_EQ(image1Darray->getImageDesc().image_array_size, surfaceState->getDepth());
    EXPECT_EQ(image1Darray->getImageDesc().image_array_size, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(image1Darray->getImageDesc().image_row_pitch, surfaceState->getSurfacePitch());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);
    EXPECT_EQ(image1Darray->getGraphicsAllocation()->gmm->queryQPitch(::renderCoreFamily, GMM_RESOURCE_TYPE::RESOURCE_1D), surfaceState->getSurfaceQpitch());

    EXPECT_EQ(image1Darray->getSurfaceFormatInfo().GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D, surfaceState->getSurfaceType());
    EXPECT_TRUE((GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceArray());

    EXPECT_EQ(expectedChannelRed, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(expectedChannelGreen, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(expectedChannelBlue, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA_ALPHA, surfaceState->getShaderChannelSelectAlpha());

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
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemory(4096);
    mcsAlloc->gmm = Gmm::create(nullptr, 1, false);
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;

    auto image = Image2dHelper<>::create(context, &imgDesc);
    image->setMcsSurfaceInfo(msi);
    image->setMcsAllocation(mcsAlloc);
    cl_mem memObj = image;

    EXPECT_FALSE(mcsAlloc->gmm->unifiedAuxTranslationCapable());

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

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
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    McsSurfaceInfo msi = {0, 0, 3};
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    cl_image_format imgFormat = {CL_DEPTH, CL_FLOAT};

    auto image = Image2dHelper<>::create(context, &imgDesc, &imgFormat);
    image->setMcsSurfaceInfo(msi);
    cl_mem memObj = image;

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    EXPECT_TRUE(Image::isDepthFormat(image->getImageFormat()));
    EXPECT_TRUE(surfaceState->getMultisampledSurfaceStorageFormat() ==
                RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
    EXPECT_TRUE(surfaceState->getAuxiliarySurfaceMode() == (typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE)0);
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

    retVal = clSetKernelArg(pKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                           pKernelInfo->kernelArgInfo[0].offsetHeap));

    EXPECT_TRUE(Image::isDepthFormat(image->getImageFormat()));
    EXPECT_TRUE(surfaceState->getSurfaceFormat() == SURFACE_FORMAT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS);
    EXPECT_TRUE(surfaceState->getMultisampledSurfaceStorageFormat() ==
                RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);
}

HWTEST_F(ImageSetArgTest, givenMcsAllocationAndRenderCompressionWhenSetArgOnMultisampledImgIsCalledThenProgramAuxFieldsWithMcsParams) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemory(4096);
    mcsAlloc->gmm = Gmm::create(nullptr, 1, false);
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;

    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(context, &imgDesc));
    image->getGraphicsAllocation()->gmm->isRenderCompressed = true;
    image->setMcsSurfaceInfo(msi);
    image->setMcsAllocation(mcsAlloc);
    cl_mem memObj = image.get();

    retVal = clSetKernelArg(pKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                                 pKernelInfo->kernelArgInfo[0].offsetHeap));

    EXPECT_TRUE(surfaceState->getMultisampledSurfaceStorageFormat() ==
                RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);
    EXPECT_TRUE(surfaceState->getAuxiliarySurfaceMode() == (typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE)1);
    EXPECT_EQ(msi.pitch, surfaceState->getAuxiliarySurfacePitch());
    EXPECT_EQ(msi.qPitch, surfaceState->getAuxiliarySurfaceQpitch());
    EXPECT_EQ(msi.multisampleCount, static_cast<uint32_t>(surfaceState->getNumberOfMultisamples()));
    EXPECT_EQ(mcsAlloc->getGpuAddress(), surfaceState->getAuxiliarySurfaceBaseAddress());
}

HWTEST_F(ImageSetArgTest, givenDepthFormatAndRenderCompressionWhenSetArgOnMultisampledImgIsCalledThenDontProgramAuxFields) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    McsSurfaceInfo msi = {0, 0, 3};
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    cl_image_format imgFormat = {CL_DEPTH, CL_FLOAT};
    imgDesc.num_samples = 8;

    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(context, &imgDesc, &imgFormat));
    image->getGraphicsAllocation()->gmm->isRenderCompressed = true;
    image->setMcsSurfaceInfo(msi);
    cl_mem memObj = image.get();

    retVal = clSetKernelArg(pKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                                 pKernelInfo->kernelArgInfo[0].offsetHeap));

    EXPECT_TRUE(Image::isDepthFormat(image->getImageFormat()));
    EXPECT_TRUE(surfaceState->getMultisampledSurfaceStorageFormat() ==
                RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
    EXPECT_TRUE(surfaceState->getAuxiliarySurfaceMode() == (typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE)0);
    EXPECT_EQ(1u, surfaceState->getAuxiliarySurfacePitch());
    EXPECT_EQ(0u, surfaceState->getAuxiliarySurfaceQpitch());
    EXPECT_EQ(msi.multisampleCount, static_cast<uint32_t>(surfaceState->getNumberOfMultisamples()));
    EXPECT_EQ(0u, surfaceState->getAuxiliarySurfaceBaseAddress());
}

HWTEST_F(ImageSetArgTest, givenMcsAllocationWhenSetArgIsCalledWithUnifiedAuxCapabilityThenProgramAuxFieldsForCcs) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemory(4096);
    mcsAlloc->gmm = Gmm::create(nullptr, 1, false);
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;

    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(context, &imgDesc));
    image->setMcsSurfaceInfo(msi);
    image->setMcsAllocation(mcsAlloc);
    cl_mem memObj = image.get();

    uint32_t expectedRenderAuxPitchTiles = 30;
    uint32_t expectedAuxQPitch = 60;
    uint64_t expectedAuxSurfaceOffset = 0x10000;

    auto mockMcsGmmResInfo = reinterpret_cast<NiceMock<MockGmmResourceInfo> *>(mcsAlloc->gmm->gmmResourceInfo.get());
    mockMcsGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_TRUE(mcsAlloc->gmm->unifiedAuxTranslationCapable());
    EXPECT_CALL(*mockMcsGmmResInfo, getRenderAuxPitchTiles()).Times(1).WillOnce(Return(expectedRenderAuxPitchTiles));
    EXPECT_CALL(*mockMcsGmmResInfo, getAuxQPitch()).Times(1).WillOnce(Return(expectedAuxQPitch));
    EXPECT_CALL(*mockMcsGmmResInfo, getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE::GMM_AUX_CCS)).Times(1).WillOnce(Return(expectedAuxSurfaceOffset));

    retVal = clSetKernelArg(pKernel, 0, sizeof(memObj), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(),
                                                                                 pKernelInfo->kernelArgInfo[0].offsetHeap));

    EXPECT_TRUE(surfaceState->getAuxiliarySurfaceMode() == (typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE)5);
    EXPECT_EQ(expectedRenderAuxPitchTiles, surfaceState->getAuxiliarySurfacePitch());
    EXPECT_EQ(expectedAuxQPitch, surfaceState->getAuxiliarySurfaceQpitch());
    EXPECT_EQ(surfaceState->getSurfaceBaseAddress() + expectedAuxSurfaceOffset, surfaceState->getAuxiliarySurfaceBaseAddress());
}

HWTEST_F(ImageSetArgTest, clSetKernelArgImage1Dbuffer) {
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
    auto imageFromBuffer = clCreateImage(context, 0, &imageFormat, &imageDesc, nullptr, nullptr);
    ASSERT_NE(nullptr, imageFromBuffer);

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(imageFromBuffer),
        &imageFromBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));
    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    auto image = castToObject<Image>(imageFromBuffer);

    EXPECT_EQ((void *)((uintptr_t)image->getGraphicsAllocation()->getGpuAddress()), surfaceAddress);
    // Width is 7 bits
    EXPECT_EQ(128u, surfaceState->getWidth());
    // Height is 14 bits
    EXPECT_EQ(50u, surfaceState->getHeight());
    // Depth is 11 bits
    EXPECT_EQ(1u, surfaceState->getDepth());

    EXPECT_EQ(1u, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);

    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch());
    EXPECT_EQ(image->getSurfaceFormatInfo().GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfaceState->getSurfaceType());
    EXPECT_FALSE((GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceArray());

    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN_GREEN, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE_BLUE, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA_ALPHA, surfaceState->getShaderChannelSelectAlpha());

    clReleaseMemObject(imageFromBuffer);
    clReleaseMemObject(buffer);
}

HWTEST_F(ImageSetArgTest, clSetKernelArgImageWithCLLuminanceFormat) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    Image *luminanceImage = Image3dHelper<LuminanceImage>::create(context);
    cl_mem memObj = luminanceImage;

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));
    //for CL_LUMINANCE format we override channels to RED to be spec complaint.
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN_RED, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE_RED, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA_ALPHA, surfaceState->getShaderChannelSelectAlpha());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());
    for (auto &surface : surfaces) {
        delete surface;
    }
    delete luminanceImage;
}

HWTEST_F(ImageSetArgTest, getKernelArgShouldReturnImage) {
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

HWTEST_F(ImageSetArgTest, givenRenderCompressedResourceWhenSettingImgArgThenSetCorrectAuxParams) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    typedef typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE AUXILIARY_SURFACE_MODE;
    auto surfaceState = RENDER_SURFACE_STATE::sInit();

    auto gmm = srcImage->getGraphicsAllocation()->gmm;
    auto mockGmmResInfo = reinterpret_cast<NiceMock<MockGmmResourceInfo> *>(gmm->gmmResourceInfo.get());
    gmm->isRenderCompressed = true;

    uint32_t expectedRenderAuxPitchTiles = 30;
    uint32_t expectedAuxQPitch = 60;
    uint64_t expectedAuxSurfaceOffset = 0x10000;

    EXPECT_CALL(*mockGmmResInfo, getRenderAuxPitchTiles()).Times(1).WillRepeatedly(Return(expectedRenderAuxPitchTiles));
    EXPECT_CALL(*mockGmmResInfo, getAuxQPitch()).Times(1).WillRepeatedly(Return(expectedAuxQPitch));
    EXPECT_CALL(*mockGmmResInfo, getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE::GMM_AUX_CCS)).Times(1).WillRepeatedly(Return(expectedAuxSurfaceOffset));

    srcImage->setImageArg(&surfaceState, false, 0);

    EXPECT_TRUE(surfaceState.getAuxiliarySurfaceMode() == (typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE)5);
    EXPECT_EQ(expectedRenderAuxPitchTiles, surfaceState.getAuxiliarySurfacePitch());
    EXPECT_EQ(expectedAuxQPitch, surfaceState.getAuxiliarySurfaceQpitch());
    EXPECT_EQ(surfaceState.getSurfaceBaseAddress() + expectedAuxSurfaceOffset, surfaceState.getAuxiliarySurfaceBaseAddress());
}

HWTEST_F(ImageSetArgTest, givenNonRenderCompressedResourceWhenSettingImgArgThenDontSetAuxParams) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    typedef typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE AUXILIARY_SURFACE_MODE;
    auto surfaceState = RENDER_SURFACE_STATE::sInit();

    auto gmm = srcImage->getGraphicsAllocation()->gmm;
    auto mockGmmResInfo = reinterpret_cast<NiceMock<MockGmmResourceInfo> *>(gmm->gmmResourceInfo.get());
    gmm->isRenderCompressed = false;

    EXPECT_CALL(*mockGmmResInfo, getRenderAuxPitchTiles()).Times(0);
    EXPECT_CALL(*mockGmmResInfo, getAuxQPitch()).Times(0);
    EXPECT_CALL(*mockGmmResInfo, getUnifiedAuxSurfaceOffset(_)).Times(0);

    srcImage->setImageArg(&surfaceState, false, 0);

    EXPECT_TRUE(surfaceState.getAuxiliarySurfaceMode() == AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(1u, surfaceState.getAuxiliarySurfacePitch());
    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceQpitch());
    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
}

/* cl_intel_media_block_io */

class ImageMediaBlockSetArgTest : public ImageSetArgTest {
  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        pKernelInfo = KernelInfo::create();

        // define kernel info
        kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
        pKernelInfo->usesSsh = true;

        // setup kernel arg offsets
        pKernelInfo->kernelArgInfo.resize(2);
        pKernelInfo->kernelArgInfo[1].offsetHeap = 0x00;
        pKernelInfo->kernelArgInfo[0].offsetHeap = 0x40;

        pKernelInfo->kernelArgInfo[1].isImage = true;
        pKernelInfo->kernelArgInfo[0].isImage = true;

        pKernelInfo->kernelArgInfo[1].isMediaBlockImage = true;
        pKernelInfo->kernelArgInfo[0].isMediaBlockImage = true;

        pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);
        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
        context = new MockContext(pDevice);
        srcImage = Image3dHelper<>::create(context);
        ASSERT_NE(nullptr, srcImage);
    }
};

HWTEST_F(ImageMediaBlockSetArgTest, clSetKernelArgImage) {
    auto imageMocs = Gmm::getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE);
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    cl_mem memObj = srcImage;

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    size_t rPitch = srcImage->getImageDesc().image_row_pitch;

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(srcImage->getCpuAddress(), surfaceAddress);

    uint32_t element_size = static_cast<uint32_t>(srcImage->getSurfaceFormatInfo().ImageElementSizeInBytes);

    SetupChannels<FamilyType>(srcImage->getImageFormat().image_channel_order);

    EXPECT_EQ(srcImage->getImageDesc().image_width * element_size / sizeof(uint32_t), surfaceState->getWidth());
    EXPECT_EQ(srcImage->getImageDesc().image_height, surfaceState->getHeight());
    EXPECT_EQ(srcImage->getImageDesc().image_depth, surfaceState->getDepth());
    EXPECT_EQ(srcImage->getImageDesc().image_depth, surfaceState->getRenderTargetViewExtent());
    EXPECT_EQ(rPitch, surfaceState->getSurfacePitch());
    EXPECT_EQ(0u, surfaceState->getSurfaceQpitch() % 4);
    EXPECT_EQ(srcImage->getSurfaceFormatInfo().GenxSurfaceFormat, (GFX3DSTATE_SURFACEFORMAT)surfaceState->getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D, surfaceState->getSurfaceType());
    EXPECT_EQ(expectedChannelRed, surfaceState->getShaderChannelSelectRed());
    EXPECT_EQ(expectedChannelGreen, surfaceState->getShaderChannelSelectGreen());
    EXPECT_EQ(expectedChannelBlue, surfaceState->getShaderChannelSelectBlue());
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA_ALPHA, surfaceState->getShaderChannelSelectAlpha());
    EXPECT_EQ(imageMocs, surfaceState->getMemoryObjectControlState());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());

    for (auto &surface : surfaces) {
        delete surface;
    }
}

typedef ImageSetArgTest ImageShaderChanelValueTest;

HWTEST_F(ImageShaderChanelValueTest, ChannelA) {
    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;

    int outputChannel = 0;
    int inputChannel = 0;

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_A);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_A);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_A);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_A);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA, outputChannel);
}

HWTEST_F(ImageShaderChanelValueTest, ChannelRA) {
    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;

    int outputChannel = 0;
    int inputChannel = 0;

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_R);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_R);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_R);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_R);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_Rx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_Rx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_Rx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_Rx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA, outputChannel);
}

HWTEST_F(ImageShaderChanelValueTest, ChannelRGA) {
    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;

    int outputChannel = 0;
    int inputChannel = 0;

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RG);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RG);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RG);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RG);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO, outputChannel);

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGx);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN, outputChannel);
}

HWTEST_F(ImageShaderChanelValueTest, ChannelRGBA) {
    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;

    int outputChannel = 0;
    int inputChannel = 0;

    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGBA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ALPHA, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGBA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGBA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN, outputChannel);
    inputChannel = SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE;
    outputChannel = ImageHw<FamilyType>::getShaderChannelValue(inputChannel, CL_RGBA);
    EXPECT_EQ(SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE, outputChannel);
}

HWTEST_F(ImageSetArgTest, givenImageWithOffsetGreaterThan4GBWhenSurfaceStateIsProgrammedThenCorrectStataBaseAddressIsSet) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState;

    uint64_t surfaceOffset = 8 * GB;

    srcImage->setSurfaceOffsets(surfaceOffset, 0, 0, 0);
    srcImage->setImageArg(&surfaceState, false, 0);

    auto expectedAddress = srcImage->getGraphicsAllocation()->getGpuAddressToPatch() + surfaceOffset;
    auto surfaceAddress = surfaceState.getSurfaceBaseAddress();

    EXPECT_EQ(expectedAddress, surfaceAddress);
}
