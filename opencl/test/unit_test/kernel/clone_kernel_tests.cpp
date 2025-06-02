/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/accelerators/intel_accelerator.h"
#include "opencl/source/accelerators/intel_motion_estimation.h"
#include "opencl/source/helpers/sampler_helpers.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_sampler.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class CloneKernelTest : public MultiRootDeviceWithSubDevicesFixture {
  public:
    CloneKernelTest() {
    }

  protected:
    void SetUp() override {
        MultiRootDeviceWithSubDevicesFixture::SetUp();
        pProgram = std::make_unique<MockProgram>(context.get(), false, context->getDevices());

        // define kernel info
        pKernelInfo = std::make_unique<MockKernelInfo>();

        pKernelInfo->kernelDescriptor.payloadMappings.explicitArgs.resize(1);
        pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors.resize(1);

        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
        pKernelInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize = 72;
        pKernelInfo->setPrivateMemory(0x10, false, 8, 64, 64);
        pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;

        KernelInfoContainer kernelInfos;
        kernelInfos.resize(3);
        kernelInfos[0] = kernelInfos[1] = kernelInfos[2] = pKernelInfo.get();

        KernelVectorType sourceKernels;
        sourceKernels.resize(3);

        KernelVectorType clonedKernels;
        clonedKernels.resize(3);

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {

            pSourceKernel[rootDeviceIndex] = new MockKernel(pProgram.get(), *pKernelInfo, *deviceFactory->rootDevices[rootDeviceIndex]);
            ASSERT_EQ(CL_SUCCESS, pSourceKernel[rootDeviceIndex]->initialize());
            sourceKernels[rootDeviceIndex] = pSourceKernel[rootDeviceIndex];

            pClonedKernel[rootDeviceIndex] = new MockKernel(pProgram.get(), *pKernelInfo, *deviceFactory->rootDevices[rootDeviceIndex]);
            ASSERT_EQ(CL_SUCCESS, pClonedKernel[rootDeviceIndex]->initialize());
            clonedKernels[rootDeviceIndex] = pClonedKernel[rootDeviceIndex];
        }

        pSourceMultiDeviceKernel = std::make_unique<MultiDeviceKernel>(sourceKernels, kernelInfos);
        pClonedMultiDeviceKernel = std::make_unique<MultiDeviceKernel>(clonedKernels, kernelInfos);
    }

    void TearDown() override {
        pClonedMultiDeviceKernel.reset();
        pSourceMultiDeviceKernel.reset();
        pKernelInfo.reset();
        pProgram.reset();
        MultiRootDeviceWithSubDevicesFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockProgram> pProgram;
    std::unique_ptr<MultiDeviceKernel> pSourceMultiDeviceKernel;
    std::unique_ptr<MultiDeviceKernel> pClonedMultiDeviceKernel;
    MockKernel *pSourceKernel[3] = {nullptr};
    MockKernel *pClonedKernel[3] = {nullptr};
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char surfaceStateHeap[128];
};

TEST_F(CloneKernelTest, givenKernelWithPrivateSurfaceWhenCloningKernelThenClonedKernelProgramItsOwnPrivateSurfaceAddress) {
    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pSourcePrivateSurface = pSourceKernel[rootDeviceIndex]->privateSurface;
        auto pClonedPrivateSurface = pClonedKernel[rootDeviceIndex]->privateSurface;
        EXPECT_NE(nullptr, pSourcePrivateSurface);
        EXPECT_NE(nullptr, pClonedPrivateSurface);
        EXPECT_NE(pClonedPrivateSurface, pSourcePrivateSurface);
        {
            auto pSourcePrivateSurfPatchedAddress = reinterpret_cast<uint64_t *>(ptrOffset(pSourceKernel[rootDeviceIndex]->getCrossThreadData(), 64));
            auto pClonedPrivateSurfPatchedAddress = reinterpret_cast<uint64_t *>(ptrOffset(pClonedKernel[rootDeviceIndex]->getCrossThreadData(), 64));

            EXPECT_EQ(pSourcePrivateSurface->getGpuAddressToPatch(), *pSourcePrivateSurfPatchedAddress);
            EXPECT_EQ(pClonedPrivateSurface->getGpuAddressToPatch(), *pClonedPrivateSurfPatchedAddress);
        }

        retVal = pClonedKernel[rootDeviceIndex]->cloneKernel(pSourceKernel[rootDeviceIndex]);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto pClonedPrivateSurface2 = pClonedKernel[rootDeviceIndex]->privateSurface;
        EXPECT_EQ(pClonedPrivateSurface, pClonedPrivateSurface2);
        {
            auto pClonedPrivateSurfPatchedAddress = reinterpret_cast<uint64_t *>(ptrOffset(pClonedKernel[rootDeviceIndex]->getCrossThreadData(), 64));
            EXPECT_EQ(pClonedPrivateSurface->getGpuAddressToPatch(), *pClonedPrivateSurfPatchedAddress);
        }
    }
}

TEST_F(CloneKernelTest, givenUnsetArgWhenCloningKernelThenKernelInfoIsCorrect) {
    pKernelInfo->addArgBuffer(0);
    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(Kernel::NONE_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_EQ(nullptr, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(nullptr, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
        EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_FALSE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);
    }

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);
    }
}

TEST_F(CloneKernelTest, givenArgLocalWhenCloningKernelThenKernelInfoIsCorrect) {
    const size_t slmSize = 0x800;
    pKernelInfo->addArgLocal(0, 0, 1);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        pSourceKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgLocal);
        pClonedKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgLocal);
    }
    retVal = pSourceMultiDeviceKernel->setArg(0, slmSize, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(Kernel::SLM_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);
    }

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

        EXPECT_EQ(alignUp(slmSize, 1024), pClonedKernel[rootDeviceIndex]->slmTotalSize);
    }
}

TEST_F(CloneKernelTest, givenArgBufferWhenCloningKernelThenKernelInfoIsCorrect) {
    pKernelInfo->addArgBuffer(0, 0x20, sizeof(uint64_t));

    auto buffer = clUniquePtr(Buffer::create(context.get(), 0, MemoryConstants::pageSize, nullptr, retVal));
    cl_mem memObj = buffer.get();

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        pSourceKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgBuffer);
        pClonedKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgBuffer);
    }

    retVal = pSourceMultiDeviceKernel->setArg(0, sizeof(cl_mem), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(Kernel::BUFFER_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);
    }

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, *static_cast<const cl_mem *>(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value));
        EXPECT_EQ(pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object, *static_cast<const cl_mem *>(pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value));
        EXPECT_EQ(*static_cast<const cl_mem *>(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value), *static_cast<const cl_mem *>(pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value));

        auto pKernelArg = reinterpret_cast<uint64_t *>(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                                       pClonedKernel[rootDeviceIndex]->getKernelInfo().getArgDescriptorAt(0).as<ArgDescPointer>().stateless);
        EXPECT_EQ(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddressToPatch(), *pKernelArg);
    }
}

TEST_F(CloneKernelTest, givenArgImageWhenCloningKernelThenKernelInfoIsCorrect) {
    pKernelInfo->addArgImage(0, 0x20);
    auto &metaPayload = pKernelInfo->argAsImg(0).metadataPayload;
    metaPayload.imgWidth = 0x4;
    metaPayload.imgHeight = 0x8;
    metaPayload.imgDepth = 0xc;

    auto image = std::unique_ptr<Image>(Image2dHelperUlt<>::create(context.get()));
    ASSERT_NE(nullptr, image);

    auto rootDeviceIndex = *context->getRootDeviceIndices().begin();
    size_t imageWidth = image->getImageDesc().image_width;
    size_t imageHeight = image->getImageDesc().image_height;
    size_t imageDepth = image->getImageDesc().image_depth;

    cl_mem memObj = image.get();

    pSourceKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgImage);
    pClonedKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgImage);

    retVal = pSourceKernel[rootDeviceIndex]->setArg(0, sizeof(cl_mem), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(Kernel::IMAGE_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
    EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pClonedKernel[rootDeviceIndex]->getCrossThreadData());
    auto &clonedArg = pClonedKernel[rootDeviceIndex]->getKernelInfo().getArgDescriptorAt(0).as<ArgDescImage>();

    auto pImgWidth = ptrOffset(crossThreadData, clonedArg.metadataPayload.imgWidth);
    EXPECT_EQ(imageWidth, *pImgWidth);

    auto pImgHeight = ptrOffset(crossThreadData, clonedArg.metadataPayload.imgHeight);
    EXPECT_EQ(imageHeight, *pImgHeight);

    auto pImgDepth = ptrOffset(crossThreadData, clonedArg.metadataPayload.imgDepth);
    EXPECT_EQ(imageDepth, *pImgDepth);
}

TEST_F(CloneKernelTest, givenArgAcceleratorWhenCloningKernelThenKernelInfoIsCorrect) {
    pKernelInfo->addArgAccelerator(0, undefined<SurfaceStateHeapOffset>, 0x4, 0x14, 0x1c, 0xc);

    cl_motion_estimation_desc_intel desc = {
        CL_ME_MB_TYPE_4x4_INTEL,
        CL_ME_SUBPIXEL_MODE_QPEL_INTEL,
        CL_ME_SAD_ADJUST_MODE_HAAR_INTEL,
        CL_ME_SEARCH_PATH_RADIUS_16_12_INTEL};

    cl_accelerator_intel accelerator = VmeAccelerator::create(
        context.get(),
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL, sizeof(desc), &desc,
        retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, accelerator);

    auto rootDeviceIndex = *context->getRootDeviceIndices().begin();
    pSourceKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgAccelerator);
    pClonedKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgAccelerator);

    retVal = pSourceKernel[rootDeviceIndex]->setArg(0, sizeof(cl_accelerator_intel), &accelerator);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(Kernel::ACCELERATOR_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
    EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pClonedKernel[rootDeviceIndex]->getCrossThreadData());
    ASSERT_TRUE(pClonedKernel[rootDeviceIndex]->getKernelInfo().getArgDescriptorAt(0).getExtendedTypeInfo().hasVmeExtendedDescriptor);
    const auto clonedArgDescVme = reinterpret_cast<ArgDescVme *>(pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors[0].get());

    uint32_t *pMbBlockType = ptrOffset(crossThreadData, clonedArgDescVme->mbBlockType);
    EXPECT_EQ(desc.mb_block_type, *pMbBlockType);

    uint32_t *pSubpixelMode = ptrOffset(crossThreadData, clonedArgDescVme->subpixelMode);
    EXPECT_EQ(desc.subpixel_mode, *pSubpixelMode);

    uint32_t *pSadAdjustMode = ptrOffset(crossThreadData, clonedArgDescVme->sadAdjustMode);
    EXPECT_EQ(desc.sad_adjust_mode, *pSadAdjustMode);

    uint32_t *pSearchPathType = ptrOffset(crossThreadData, clonedArgDescVme->searchPathType);
    EXPECT_EQ(desc.search_path_type, *pSearchPathType);

    retVal = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(CloneKernelTest, givenArgSamplerWhenCloningKernelThenKernelInfoIsCorrect) {
    auto sampler = clUniquePtr<Sampler>(new MockSampler(context.get(),
                                                        true,
                                                        (cl_addressing_mode)CL_ADDRESS_MIRRORED_REPEAT,
                                                        (cl_filter_mode)CL_FILTER_NEAREST));

    pKernelInfo->addArgSampler(0, 0x20, 0x8, 0x10, 0x4);

    cl_sampler samplerObj = sampler.get();
    auto rootDeviceIndex = *context->getRootDeviceIndices().begin();

    pSourceKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgSampler);
    pClonedKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgSampler);

    retVal = pSourceKernel[rootDeviceIndex]->setArg(0, sizeof(cl_sampler), &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(Kernel::SAMPLER_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
    EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pClonedKernel[rootDeviceIndex]->getCrossThreadData());
    const auto &clonedArg = pClonedKernel[rootDeviceIndex]->getKernelInfo().getArgDescriptorAt(0).as<ArgDescSampler>();

    auto pSnapWa = ptrOffset(crossThreadData, clonedArg.metadataPayload.samplerSnapWa);
    EXPECT_EQ(sampler->getSnapWaValue(), *pSnapWa);

    auto pAddressingMode = ptrOffset(crossThreadData, clonedArg.metadataPayload.samplerAddressingMode);
    EXPECT_EQ(getAddrModeEnum(sampler->addressingMode), *pAddressingMode);

    auto pNormalizedCoords = ptrOffset(crossThreadData, clonedArg.metadataPayload.samplerNormalizedCoords);
    EXPECT_EQ(getNormCoordsEnum(sampler->normalizedCoordinates), *pNormalizedCoords);
    EXPECT_EQ(3, sampler->getRefInternalCount());
}

TEST_F(CloneKernelTest, givenArgSvmWhenCloningKernelThenKernelInfoIsCorrect) {
    char *svmPtr = new char[256];

    pKernelInfo->addArgBuffer(0, 0x20, sizeof(void *));

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        retVal = pSourceKernel[rootDeviceIndex]->setArgSvm(0, 256, svmPtr, nullptr, 0u);
        ASSERT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(Kernel::SVM_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);
    }

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

        auto pKernelArg = (void **)(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                    pClonedKernel[rootDeviceIndex]->getKernelInfo().getArgDescriptorAt(0).as<ArgDescPointer>().stateless);
        EXPECT_EQ(svmPtr, *pKernelArg);
    }

    delete[] svmPtr;
}

TEST_F(CloneKernelTest, givenArgSvmAllocWhenCloningKernelThenKernelInfoIsCorrect) {
    const ClDeviceInfo &devInfo = device1->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    pKernelInfo->addArgBuffer(0, 0x20, sizeof(void *));

    char memory[100] = {};
    MultiGraphicsAllocation multiGraphicsAllocation(3);
    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto svmAlloc = new MockGraphicsAllocation(rootDeviceIndex, memory, 100);
        multiGraphicsAllocation.addAllocation(svmAlloc);
    }

    retVal = pSourceMultiDeviceKernel->setArgSvmAlloc(0, memory, &multiGraphicsAllocation, 1u);
    ASSERT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex), pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(Kernel::SVM_ALLOC_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);
    }

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

        auto pKernelArg = (void **)(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                    pClonedKernel[rootDeviceIndex]->getKernelInfo().getArgDescriptorAt(0).as<ArgDescPointer>().stateless);
        EXPECT_EQ(memory, *pKernelArg);
        delete multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    }
}

TEST_F(CloneKernelTest, givenArgImmediateWhenCloningKernelThenKernelInfoIsCorrect) {
    pKernelInfo->addArgImmediate(0, sizeof(void *), 0x20);

    using TypeParam = unsigned long;
    auto value = (TypeParam)0xAA55AA55UL;

    retVal = pSourceMultiDeviceKernel->setArg(0, sizeof(TypeParam), &value);
    ASSERT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(Kernel::NONE_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);
    }

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).allocId);

        auto pKernelArg = (TypeParam *)(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                        pClonedKernel[rootDeviceIndex]->getKernelInfo().getArgDescriptorAt(0).as<ArgDescValue>().elements[0].offset);
        EXPECT_EQ(value, *pKernelArg);
    }
}

TEST_F(CloneKernelTest, givenExecInfoWhenCloningKernelThenSvmAllocationIsCorrect) {
    REQUIRE_SVM_OR_SKIP(device1);
    void *ptrSVM = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    ASSERT_NE(nullptr, ptrSVM);

    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(ptrSVM);
    ASSERT_NE(nullptr, svmData);
    auto &pSvmAllocs = svmData->gpuAllocations;

    pSourceMultiDeviceKernel->setSvmKernelExecInfo(pSvmAllocs);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->kernelSvmGfxAllocations.size());
        EXPECT_NE(nullptr, pSourceKernel[rootDeviceIndex]->kernelSvmGfxAllocations.at(0));
        EXPECT_EQ(pSvmAllocs.getGraphicsAllocation(rootDeviceIndex), pSourceKernel[rootDeviceIndex]->kernelSvmGfxAllocations.at(0));
    }

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->kernelSvmGfxAllocations.size(), pClonedKernel[rootDeviceIndex]->kernelSvmGfxAllocations.size());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->kernelSvmGfxAllocations.at(0), pClonedKernel[rootDeviceIndex]->kernelSvmGfxAllocations.at(0));
    }

    context->getSVMAllocsManager()->freeSVMAlloc(ptrSVM);
}

TEST_F(CloneKernelTest, givenUnifiedMemoryExecInfoWhenCloningKernelThenUnifiedMemoryAllocationIsCorrect) {
    REQUIRE_SVM_OR_SKIP(device1);
    void *ptrSVM = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    ASSERT_NE(nullptr, ptrSVM);

    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(ptrSVM);
    ASSERT_NE(nullptr, svmData);
    auto &pSvmAllocs = svmData->gpuAllocations;

    pSourceMultiDeviceKernel->setUnifiedMemoryExecInfo(pSvmAllocs);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->kernelUnifiedMemoryGfxAllocations.size());
        EXPECT_NE(nullptr, pSourceKernel[rootDeviceIndex]->kernelUnifiedMemoryGfxAllocations.at(0));
        EXPECT_EQ(pSvmAllocs.getGraphicsAllocation(rootDeviceIndex), pSourceKernel[rootDeviceIndex]->kernelUnifiedMemoryGfxAllocations.at(0));
    }

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->kernelUnifiedMemoryGfxAllocations.size(), pClonedKernel[rootDeviceIndex]->kernelUnifiedMemoryGfxAllocations.size());
        EXPECT_EQ(pSourceKernel[rootDeviceIndex]->kernelUnifiedMemoryGfxAllocations.at(0), pClonedKernel[rootDeviceIndex]->kernelUnifiedMemoryGfxAllocations.at(0));
    }

    context->getSVMAllocsManager()->freeSVMAlloc(ptrSVM);
}

TEST_F(CloneKernelTest, givenBuiltinSourceKernelWhenCloningThenSetBuiltinFlagToClonedKernel) {
    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        pSourceKernel[rootDeviceIndex]->isBuiltIn = true;
    }
    pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_TRUE(pClonedKernel[rootDeviceIndex]->isBuiltIn);
    }
}
