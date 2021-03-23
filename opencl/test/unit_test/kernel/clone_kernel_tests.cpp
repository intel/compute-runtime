/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/accelerators/intel_accelerator.h"
#include "opencl/source/accelerators/intel_motion_estimation.h"
#include "opencl/source/helpers/sampler_helpers.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/pipe.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_device_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_pipe.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_sampler.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"
#include "test.h"

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

        KernelInfoContainer kernelInfos;
        kernelInfos.resize(3);

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            // define kernel info
            pKernelInfo[rootDeviceIndex] = std::make_unique<KernelInfo>();
            pKernelInfo[rootDeviceIndex]->kernelDescriptor.kernelAttributes.simdSize = 1;

            // setup kernel arg offsets
            KernelArgPatchInfo kernelArgPatchInfo;

            pKernelInfo[rootDeviceIndex]->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
            pKernelInfo[rootDeviceIndex]->heapInfo.pSsh = surfaceStateHeap;
            pKernelInfo[rootDeviceIndex]->usesSsh = true;
            pKernelInfo[rootDeviceIndex]->requiresSshForBuffers = true;

            pKernelInfo[rootDeviceIndex]->kernelArgInfo.resize(1);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = (uint32_t)sizeof(void *);

            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetHeap = 0x20;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetObjectId = 0x0;

            // image
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetImgWidth = 0x4;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetImgHeight = 0x8;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetImgDepth = 0xc;

            // sampler
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetSamplerSnapWa = 0x4;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetSamplerAddressingMode = 0x8;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetSamplerNormalizedCoords = 0x10;

            // accelerator
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].samplerArgumentType = iOpenCL::SAMPLER_OBJECT_VME;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetVmeMbBlockType = 0x4;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetVmeSubpixelMode = 0xc;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetVmeSadAdjustMode = 0x14;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetVmeSearchPathType = 0x1c;

            kernelInfos[rootDeviceIndex] = pKernelInfo[rootDeviceIndex].get();
        }

        KernelVectorType sourceKernels;
        sourceKernels.resize(3);

        KernelVectorType clonedKernels;
        clonedKernels.resize(3);

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {

            pSourceKernel[rootDeviceIndex] = new MockKernel(pProgram.get(), *pKernelInfo[rootDeviceIndex], *deviceFactory->rootDevices[rootDeviceIndex]);
            ASSERT_EQ(CL_SUCCESS, pSourceKernel[rootDeviceIndex]->initialize());
            char pSourceCrossThreadData[64] = {};
            sourceKernels[rootDeviceIndex] = pSourceKernel[rootDeviceIndex];

            pClonedKernel[rootDeviceIndex] = new MockKernel(pProgram.get(), *pKernelInfo[rootDeviceIndex], *deviceFactory->rootDevices[rootDeviceIndex]);
            ASSERT_EQ(CL_SUCCESS, pClonedKernel[rootDeviceIndex]->initialize());
            char pClonedCrossThreadData[64] = {};
            clonedKernels[rootDeviceIndex] = pClonedKernel[rootDeviceIndex];

            pSourceKernel[rootDeviceIndex]->setCrossThreadData(pSourceCrossThreadData, sizeof(pSourceCrossThreadData));
            pClonedKernel[rootDeviceIndex]->setCrossThreadData(pClonedCrossThreadData, sizeof(pClonedCrossThreadData));
        }

        pSourceMultiDeviceKernel = std::make_unique<MultiDeviceKernel>(sourceKernels, kernelInfos);
        pClonedMultiDeviceKernel = std::make_unique<MultiDeviceKernel>(clonedKernels, kernelInfos);
    }

    void TearDown() override {
        MultiRootDeviceWithSubDevicesFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockProgram> pProgram;
    std::unique_ptr<MultiDeviceKernel> pSourceMultiDeviceKernel;
    std::unique_ptr<MultiDeviceKernel> pClonedMultiDeviceKernel;
    MockKernel *pSourceKernel[3] = {nullptr};
    MockKernel *pClonedKernel[3] = {nullptr};
    std::unique_ptr<KernelInfo> pKernelInfo[3];
    char surfaceStateHeap[128];
};

TEST_F(CloneKernelTest, GivenUnsetArgWhenCloningKernelThenKernelInfoIsCorrect) {

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(Kernel::NONE_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_EQ(nullptr, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(nullptr, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
        EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(0u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_FALSE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
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
    }
}

TEST_F(CloneKernelTest, GivenArgLocalWhenCloningKernelThenKernelInfoIsCorrect) {
    const size_t slmSize = 0x800;

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

        EXPECT_EQ(alignUp(slmSize, 1024), pClonedKernel[rootDeviceIndex]->slmTotalSize);
    }
}

TEST_F(CloneKernelTest, GivenArgBufferWhenCloningKernelThenKernelInfoIsCorrect) {
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

        auto pKernelArg = (cl_mem *)(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                     pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
        EXPECT_EQ(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddressToPatch(), reinterpret_cast<uint64_t>(*pKernelArg));
    }
}

TEST_F(CloneKernelTest, GivenArgPipeWhenCloningKernelThenKernelInfoIsCorrect) {
    auto pipe = clUniquePtr(Pipe::create(context.get(), 0, 1, 20, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_mem memObj = pipe.get();

    auto rootDeviceIndex = *context->getRootDeviceIndices().begin();

    pSourceKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgPipe);
    pClonedKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgPipe);

    retVal = pSourceKernel[rootDeviceIndex]->setArg(0, sizeof(cl_mem), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(Kernel::PIPE_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    auto pKernelArg = (cl_mem *)(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                 pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
    EXPECT_EQ(pipe->getGraphicsAllocation(rootDeviceIndex)->getGpuAddressToPatch(), reinterpret_cast<uint64_t>(*pKernelArg));
}

TEST_F(CloneKernelTest, GivenArgImageWhenCloningKernelThenKernelInfoIsCorrect) {
    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(context.get()));
    ASSERT_NE(nullptr, image);

    auto rootDeviceIndex = *context->getRootDeviceIndices().begin();
    uint32_t objectId = pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].offsetHeap;
    size_t imageWidth = image->getImageDesc().image_width;
    size_t imageHeight = image->getImageDesc().image_height;
    size_t imageDepth = image->getImageDesc().image_depth;

    cl_mem memObj = image.get();

    pSourceKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgImage);
    pClonedKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgImage);

    retVal = pSourceMultiDeviceKernel->setArg(0, sizeof(cl_mem), &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(Kernel::IMAGE_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pClonedKernel[rootDeviceIndex]->getCrossThreadData());
    EXPECT_EQ(objectId, *crossThreadData);

    const auto &argInfo = pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelArgInfo[0];

    auto pImgWidth = ptrOffset(crossThreadData, argInfo.offsetImgWidth);
    EXPECT_EQ(imageWidth, *pImgWidth);

    auto pImgHeight = ptrOffset(crossThreadData, argInfo.offsetImgHeight);
    EXPECT_EQ(imageHeight, *pImgHeight);

    auto pImgDepth = ptrOffset(crossThreadData, argInfo.offsetImgDepth);
    EXPECT_EQ(imageDepth, *pImgDepth);
}

TEST_F(CloneKernelTest, GivenArgAcceleratorWhenCloningKernelThenKernelInfoIsCorrect) {
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

    retVal = pSourceMultiDeviceKernel->setArg(0, sizeof(cl_accelerator_intel), &accelerator);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(Kernel::ACCELERATOR_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pClonedKernel[rootDeviceIndex]->getCrossThreadData());

    const auto &argInfo = pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelArgInfo[0];

    uint32_t *pMbBlockType = ptrOffset(crossThreadData, argInfo.offsetVmeMbBlockType);
    EXPECT_EQ(desc.mb_block_type, *pMbBlockType);

    uint32_t *pSubpixelMode = ptrOffset(crossThreadData, argInfo.offsetVmeSubpixelMode);
    EXPECT_EQ(desc.subpixel_mode, *pSubpixelMode);

    uint32_t *pSadAdjustMode = ptrOffset(crossThreadData, argInfo.offsetVmeSadAdjustMode);
    EXPECT_EQ(desc.sad_adjust_mode, *pSadAdjustMode);

    uint32_t *pSearchPathType = ptrOffset(crossThreadData, argInfo.offsetVmeSearchPathType);
    EXPECT_EQ(desc.search_path_type, *pSearchPathType);

    retVal = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(CloneKernelTest, GivenArgSamplerWhenCloningKernelThenKernelInfoIsCorrect) {
    auto sampler = clUniquePtr<Sampler>(new MockSampler(context.get(),
                                                        true,
                                                        (cl_addressing_mode)CL_ADDRESS_MIRRORED_REPEAT,
                                                        (cl_filter_mode)CL_FILTER_NEAREST));

    uint32_t objectId = SAMPLER_OBJECT_ID_SHIFT + pKernelInfo[*context->getRootDeviceIndices().begin()]->kernelArgInfo[0].offsetHeap;

    cl_sampler samplerObj = sampler.get();
    auto rootDeviceIndex = *context->getRootDeviceIndices().begin();

    pSourceKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgSampler);
    pClonedKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgSampler);

    retVal = pSourceMultiDeviceKernel->setArg(0, sizeof(cl_sampler), &samplerObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(Kernel::SAMPLER_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pClonedKernel[rootDeviceIndex]->getCrossThreadData());
    EXPECT_EQ(objectId, *crossThreadData);

    const auto &argInfo = pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelArgInfo[0];

    auto pSnapWa = ptrOffset(crossThreadData, argInfo.offsetSamplerSnapWa);
    EXPECT_EQ(sampler->getSnapWaValue(), *pSnapWa);

    auto pAddressingMode = ptrOffset(crossThreadData, argInfo.offsetSamplerAddressingMode);
    EXPECT_EQ(GetAddrModeEnum(sampler->addressingMode), *pAddressingMode);

    auto pNormalizedCoords = ptrOffset(crossThreadData, argInfo.offsetSamplerNormalizedCoords);
    EXPECT_EQ(GetNormCoordsEnum(sampler->normalizedCoordinates), *pNormalizedCoords);
    EXPECT_EQ(3, sampler->getRefInternalCount());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CloneKernelTest, GivenArgDeviceQueueWhenCloningKernelThenKernelInfoIsCorrect) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device1);

    cl_queue_properties queueProps[5] = {
        CL_QUEUE_PROPERTIES,
        CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
        0, 0, 0};

    MockDeviceQueueHw<FamilyType> mockDevQueue(context.get(), device1, queueProps[0]);
    auto clDeviceQueue = static_cast<cl_command_queue>(&mockDevQueue);
    auto rootDeviceIndex = *context->getRootDeviceIndices().begin();

    pSourceKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgDevQueue);
    pClonedKernel[rootDeviceIndex]->setKernelArgHandler(0, &Kernel::setArgDevQueue);

    retVal = pSourceMultiDeviceKernel->setArg(0, sizeof(cl_command_queue), &clDeviceQueue);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(Kernel::DEVICE_QUEUE_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArguments().size(), pClonedKernel[rootDeviceIndex]->getKernelArguments().size());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).value, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).value);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum(), pClonedKernel[rootDeviceIndex]->getPatchedArgumentsNum());
    EXPECT_EQ(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched, pClonedKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);

    auto pKernelArg = (uintptr_t *)(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                    pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
    EXPECT_EQ(static_cast<uintptr_t>(mockDevQueue.getQueueBuffer()->getGpuAddressToPatch()), *pKernelArg);
}

TEST_F(CloneKernelTest, GivenArgSvmWhenCloningKernelThenKernelInfoIsCorrect) {
    char *svmPtr = new char[256];

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        retVal = pSourceKernel[rootDeviceIndex]->setArgSvm(0, 256, svmPtr, nullptr, 0u);
        ASSERT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(Kernel::SVM_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
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

        auto pKernelArg = (void **)(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                    pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
        EXPECT_EQ(svmPtr, *pKernelArg);
    }

    delete[] svmPtr;
}

TEST_F(CloneKernelTest, GivenArgSvmAllocWhenCloningKernelThenKernelInfoIsCorrect) {
    char memory[100] = {};
    MultiGraphicsAllocation multiGraphicsAllocation(3);
    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto svmAlloc = new MockGraphicsAllocation(rootDeviceIndex, memory, 100);
        multiGraphicsAllocation.addAllocation(svmAlloc);
    }

    retVal = pSourceMultiDeviceKernel->setArgSvmAlloc(0, memory, &multiGraphicsAllocation);
    ASSERT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getKernelArguments().size());
        EXPECT_EQ(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex), pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).object);
        EXPECT_EQ(Kernel::SVM_ALLOC_OBJ, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).type);
        EXPECT_NE(0u, pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).size);
        EXPECT_EQ(1u, pSourceKernel[rootDeviceIndex]->getPatchedArgumentsNum());
        EXPECT_TRUE(pSourceKernel[rootDeviceIndex]->getKernelArgInfo(0).isPatched);
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

        auto pKernelArg = (void **)(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                    pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
        EXPECT_EQ(memory, *pKernelArg);
        delete multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    }
}

TEST_F(CloneKernelTest, GivenArgImmediateWhenCloningKernelThenKernelInfoIsCorrect) {
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

        auto pKernelArg = (TypeParam *)(pClonedKernel[rootDeviceIndex]->getCrossThreadData() +
                                        pClonedKernel[rootDeviceIndex]->getKernelInfo().kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
        EXPECT_EQ(value, *pKernelArg);
    }
}

TEST_F(CloneKernelTest, GivenExecInfoWhenCloningKernelThenSvmAllocationIsCorrect) {
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

TEST_F(CloneKernelTest, GivenUnifiedMemoryExecInfoWhenCloningKernelThenUnifiedMemoryAllocationIsCorrect) {
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
