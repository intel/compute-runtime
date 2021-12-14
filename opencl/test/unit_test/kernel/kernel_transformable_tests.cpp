/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_sampler.h"

#include <memory>

using namespace NEO;

class KernelTransformableTest : public ::testing::Test {
  public:
    void SetUp() override {
        context = std::make_unique<MockContext>(deviceFactory.rootDevices[rootDeviceIndex]);
        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

        pKernelInfo->addArgSampler(0, 0);
        pKernelInfo->addArgSampler(1, 0);
        pKernelInfo->addArgImage(2, firstImageOffset);
        pKernelInfo->addArgImage(3, secondImageOffset);
        pKernelInfo->kernelDescriptor.kernelAttributes.numArgsToPatch = 4;

        program = std::make_unique<MockProgram>(context.get(), false, toClDeviceVector(*context->getDevice(0)));
        pKernel.reset(new MockKernel(program.get(), *pKernelInfo, *deviceFactory.rootDevices[rootDeviceIndex]));
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
    UltClDeviceFactory deviceFactory{2, 0};
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<Sampler> sampler;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    std::unique_ptr<MockKernel> pKernel;

    std::unique_ptr<Image> image;
    SKernelBinaryHeaderCommon kernelHeader;
    char surfaceStateHeap[0x80];
    const uint32_t rootDeviceIndex = 1;
};

HWTEST_F(KernelTransformableTest, givenKernelThatCannotTranformImagesWithTwoTransformableImagesAndTwoTransformableSamplersWhenAllArgsAreSetThenImagesAreNotTransformed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image3dHelper<>::create(context.get()));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->argAt(2).getExtendedTypeInfo().isTransformable = true;
    pKernelInfo->argAt(3).getExtendedTypeInfo().isTransformable = true;
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

    image.reset(Image3dHelper<>::create(context.get()));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->argAt(2).getExtendedTypeInfo().isTransformable = true;
    pKernelInfo->argAt(3).getExtendedTypeInfo().isTransformable = true;

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

    image.reset(Image3dHelper<>::create(context.get()));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->argAt(2).getExtendedTypeInfo().isTransformable = true;
    pKernelInfo->argAt(3).getExtendedTypeInfo().isTransformable = true;

    pKernel->setArg(0, sizeof(clSampler), &clSampler);
    pKernel->setArg(1, sizeof(clSampler), &clSampler);
    pKernel->setArg(2, sizeof(clImage), &clImage);
    pKernel->setArg(3, sizeof(clImage), &clImage);

    auto ssh = pKernel->getSurfaceStateHeap();

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));
    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);

    pKernelInfo->argAt(3).getExtendedTypeInfo().isTransformable = false;
    pKernel->setArg(3, sizeof(clImage), &clImage);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, firstSurfaceState->getSurfaceType());
    EXPECT_TRUE(firstSurfaceState->getSurfaceArray());
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(KernelTransformableTest, givenKernelWithOneTransformableImageAndTwoTransformableSamplersWhenAnyArgIsResetThenOnlyOneImageIsTransformed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    image.reset(Image3dHelper<>::create(context.get()));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->argAt(2).getExtendedTypeInfo().isTransformable = true;
    pKernelInfo->argAt(3).getExtendedTypeInfo().isTransformable = false;

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

    image.reset(Image2dHelper<>::create(context.get()));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->argAt(2).getExtendedTypeInfo().isTransformable = true;
    pKernelInfo->argAt(3).getExtendedTypeInfo().isTransformable = true;

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

    image.reset(Image3dHelper<>::create(context.get()));
    sampler.reset(createTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->argAt(2).getExtendedTypeInfo().isTransformable = true;
    pKernelInfo->argAt(3).getExtendedTypeInfo().isTransformable = true;

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

    image.reset(Image3dHelper<>::create(context.get()));
    sampler.reset(createNonTransformableSampler());
    cl_mem clImage = image.get();
    cl_sampler clSampler = sampler.get();
    pKernelInfo->argAt(2).getExtendedTypeInfo().isTransformable = true;
    pKernelInfo->argAt(3).getExtendedTypeInfo().isTransformable = true;

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

    image.reset(Image3dHelper<>::create(context.get()));
    cl_mem clImage = image.get();

    pKernelInfo->kernelDescriptor.payloadMappings.explicitArgs.clear();
    pKernelInfo->addArgImage(0, 0);
    pKernelInfo->addArgImage(1, 0);
    pKernelInfo->addArgImage(2, firstImageOffset);
    pKernelInfo->argAt(2).getExtendedTypeInfo().isTransformable = true;
    pKernelInfo->addArgImage(3, secondImageOffset);
    pKernelInfo->argAt(3).getExtendedTypeInfo().isTransformable = true;

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
