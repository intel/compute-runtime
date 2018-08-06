/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/program/kernel_info.h"
#include "runtime/sampler/sampler.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_sampler.h"

#include "test.h"
#include <memory>

using namespace OCLRT;

class KernelTransformableTest : public ::testing::Test {
  public:
    void SetUp() override {
        pKernelInfo.reset(KernelInfo::create());
        KernelArgPatchInfo kernelArgPatchInfo;

        kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
        pKernelInfo->usesSsh = true;

        pKernelInfo->kernelArgInfo.resize(4);
        pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        pKernelInfo->kernelArgInfo[0].offsetHeap = 0x0;
        pKernelInfo->kernelArgInfo[0].isSampler = true;
        pKernelInfo->kernelArgInfo[1].offsetHeap = 0x0;
        pKernelInfo->kernelArgInfo[1].isSampler = true;
        pKernelInfo->kernelArgInfo[2].offsetHeap = firstImageOffset;
        pKernelInfo->kernelArgInfo[2].isImage = true;
        pKernelInfo->kernelArgInfo[3].offsetHeap = secondImageOffset;
        pKernelInfo->kernelArgInfo[3].isImage = true;
        pKernelInfo->argumentsToPatchNum = 4;

        program = std::make_unique<MockProgram>(*context.getDevice(0)->getExecutionEnvironment());
        pKernel.reset(new MockKernel(program.get(), *pKernelInfo, *context.getDevice(0)));
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        pKernel->setKernelArgHandler(0, &Kernel::setArgSampler);
        pKernel->setKernelArgHandler(1, &Kernel::setArgSampler);
        pKernel->setKernelArgHandler(2, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(3, &Kernel::setArgImage);
    }

    Sampler *createTransformableSampler() {
        return new MockSampler(nullptr, CL_FALSE, CL_ADDRESS_CLAMP_TO_EDGE, CL_FILTER_NEAREST);
    }
    Sampler *createNonTransformableSampler() {
        return new MockSampler(nullptr, CL_TRUE, CL_ADDRESS_CLAMP_TO_EDGE, CL_FILTER_NEAREST);
    }

    const int firstImageOffset = 0x20;
    const int secondImageOffset = 0x40;

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<Sampler> sampler;
    std::unique_ptr<KernelInfo> pKernelInfo;
    std::unique_ptr<MockKernel> pKernel;

    std::unique_ptr<Image> image;
    SKernelBinaryHeaderCommon kernelHeader;
    char surfaceStateHeap[0x80];
};

HWTEST_F(KernelTransformableTest, givenKernelThatCannotTranformImagesWithTwoTransformableImagesAndTwoTransformableSamplersWhenAllArgsAreSetThenImagesAreNotTransformed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image3dHelper<>::create(&context));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->kernelArgInfo[2].isTransformable = true;
    pKernelInfo->kernelArgInfo[3].isTransformable = true;
    pKernel->canKernelTransformImages = false;

    pKernel->setArg(0, sizeof(clSampler), &clSampler);
    pKernel->setArg(1, sizeof(clSampler), &clSampler);
    pKernel->setArg(2, sizeof(clImage), &clImage);
    pKernel->setArg(3, sizeof(clImage), &clImage);

    auto ssh = pKernel->getSurfaceStateHeap();

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, firstSurfaceState->getSurfaceType());
    EXPECT_FALSE(firstSurfaceState->getSurfaceArray());

    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(KernelTransformableTest, givenKernelWithTwoTransformableImagesAndTwoTransformableSamplersWhenAllArgsAreSetThenImagesAreTransformed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image3dHelper<>::create(&context));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->kernelArgInfo[2].isTransformable = true;
    pKernelInfo->kernelArgInfo[3].isTransformable = true;

    pKernel->setArg(0, sizeof(clSampler), &clSampler);
    pKernel->setArg(1, sizeof(clSampler), &clSampler);
    pKernel->setArg(2, sizeof(clImage), &clImage);
    pKernel->setArg(3, sizeof(clImage), &clImage);

    auto ssh = pKernel->getSurfaceStateHeap();

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, firstSurfaceState->getSurfaceType());
    EXPECT_TRUE(firstSurfaceState->getSurfaceArray());

    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, secondSurfaceState->getSurfaceType());
    EXPECT_TRUE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(KernelTransformableTest, givenKernelWithTwoTransformableImagesAndTwoTransformableSamplersWhenAnyArgIsResetThenImagesAreTransformedAgain) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image3dHelper<>::create(&context));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->kernelArgInfo[2].isTransformable = true;
    pKernelInfo->kernelArgInfo[3].isTransformable = true;

    pKernel->setArg(0, sizeof(clSampler), &clSampler);
    pKernel->setArg(1, sizeof(clSampler), &clSampler);
    pKernel->setArg(2, sizeof(clImage), &clImage);
    pKernel->setArg(3, sizeof(clImage), &clImage);

    auto ssh = pKernel->getSurfaceStateHeap();

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));
    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);

    pKernelInfo->kernelArgInfo[3].isTransformable = false;
    pKernel->setArg(3, sizeof(clImage), &clImage);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, firstSurfaceState->getSurfaceType());
    EXPECT_TRUE(firstSurfaceState->getSurfaceArray());
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(KernelTransformableTest, givenKernelWithOneTransformableImageAndTwoTransformableSamplersWhenAnyArgIsResetThenOnlyOneImageIsTransformed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image3dHelper<>::create(&context));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->kernelArgInfo[2].isTransformable = true;
    pKernelInfo->kernelArgInfo[3].isTransformable = false;

    pKernel->setArg(0, sizeof(clSampler), &clSampler);
    pKernel->setArg(1, sizeof(clSampler), &clSampler);
    pKernel->setArg(2, sizeof(clImage), &clImage);
    pKernel->setArg(3, sizeof(clImage), &clImage);

    auto ssh = pKernel->getSurfaceStateHeap();

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, firstSurfaceState->getSurfaceType());
    EXPECT_TRUE(firstSurfaceState->getSurfaceArray());
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(KernelTransformableTest, givenKernelWithImages2dAndTwoTransformableSamplersWhenAnyArgIsResetThenImagesAreNotTransformed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image2dHelper<>::create(&context));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->kernelArgInfo[2].isTransformable = true;
    pKernelInfo->kernelArgInfo[3].isTransformable = true;

    auto ssh = pKernel->getSurfaceStateHeap();

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    pKernel->setArg(0, sizeof(clSampler), &clSampler);
    pKernel->setArg(1, sizeof(clSampler), &clSampler);
    pKernel->setArg(2, sizeof(clImage), &clImage);
    pKernel->setArg(3, sizeof(clImage), &clImage);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, firstSurfaceState->getSurfaceType());
    EXPECT_FALSE(firstSurfaceState->getSurfaceArray());
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(KernelTransformableTest, givenKernelWithTwoTransformableImagesAndTwoTransformableSamplersWhenChangeSamplerToNontransformableThenImagesAreTransformedTo3d) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image3dHelper<>::create(&context));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->kernelArgInfo[2].isTransformable = true;
    pKernelInfo->kernelArgInfo[3].isTransformable = true;

    pKernel->setArg(0, sizeof(clSampler), &clSampler);
    pKernel->setArg(1, sizeof(clSampler), &clSampler);
    pKernel->setArg(2, sizeof(clImage), &clImage);
    pKernel->setArg(3, sizeof(clImage), &clImage);

    auto ssh = pKernel->getSurfaceStateHeap();

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    std::unique_ptr<Sampler> sampler2(createNonTransformableSampler());
    cl_sampler clSampler2 = sampler2.get();
    pKernel->setArg(1, sizeof(clSampler2), &clSampler2);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, firstSurfaceState->getSurfaceType());
    EXPECT_FALSE(firstSurfaceState->getSurfaceArray());
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
    pKernel.reset();
}

HWTEST_F(KernelTransformableTest, givenKernelWithNonTransformableSamplersWhenResetSamplerWithNontransformableThenImagesNotChangedAgain) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image3dHelper<>::create(&context));
    sampler.reset(createNonTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->kernelArgInfo[2].isTransformable = true;
    pKernelInfo->kernelArgInfo[3].isTransformable = true;

    pKernel->setArg(0, sizeof(clSampler), &clSampler);
    pKernel->setArg(1, sizeof(clSampler), &clSampler);
    pKernel->setArg(2, sizeof(clImage), &clImage);
    pKernel->setArg(3, sizeof(clImage), &clImage);

    auto ssh = pKernel->getSurfaceStateHeap();

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);

    pKernel->setArg(0, sizeof(clSampler), &clSampler);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL, firstSurfaceState->getSurfaceType());
    EXPECT_FALSE(firstSurfaceState->getSurfaceArray());
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(KernelTransformableTest, givenKernelWithoutSamplersAndTransformableImagesWhenResolveKernelThenImagesAreTransformed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image3dHelper<>::create(&context));
    cl_mem clImage = image.get();

    pKernelInfo->kernelArgInfo[0].isSampler = false;
    pKernelInfo->kernelArgInfo[0].isImage = true;
    pKernelInfo->kernelArgInfo[1].isSampler = false;
    pKernelInfo->kernelArgInfo[1].isImage = true;
    pKernelInfo->kernelArgInfo[2].isTransformable = true;
    pKernelInfo->kernelArgInfo[3].isTransformable = true;

    pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
    pKernel->setKernelArgHandler(1, &Kernel::setArgImage);

    pKernel->setArg(0, sizeof(clImage), &clImage);
    pKernel->setArg(1, sizeof(clImage), &clImage);
    pKernel->setArg(2, sizeof(clImage), &clImage);
    pKernel->setArg(3, sizeof(clImage), &clImage);

    auto ssh = pKernel->getSurfaceStateHeap();

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, firstSurfaceState->getSurfaceType());
    EXPECT_TRUE(firstSurfaceState->getSurfaceArray());
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, secondSurfaceState->getSurfaceType());
    EXPECT_TRUE(secondSurfaceState->getSurfaceArray());
}
