/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

using namespace NEO;

template <typename T>
class KernelArgImmediateTest : public MultiRootDeviceWithSubDevicesFixture {
  public:
  protected:
    void SetUp() override {
        MultiRootDeviceWithSubDevicesFixture::SetUp();
        program = std::make_unique<MockProgram>(context.get(), false, context->getDevices());

        KernelInfoContainer kernelInfos;
        kernelInfos.resize(3);
        KernelVectorType kernels;
        kernels.resize(3);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            memset(&pCrossThreadData[rootDeviceIndex], 0xfe, sizeof(pCrossThreadData[rootDeviceIndex]));

            // define kernel info
            pKernelInfo[rootDeviceIndex] = std::make_unique<KernelInfo>();
            pKernelInfo[rootDeviceIndex]->kernelDescriptor.kernelAttributes.simdSize = 1;

            // setup kernel arg offsets
            KernelArgPatchInfo kernelArgPatchInfo;

            pKernelInfo[rootDeviceIndex]->kernelArgInfo.resize(4);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

            pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].crossthreadOffset = 0x38;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].crossthreadOffset = 0x28;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset = 0x40;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x50;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].size = sizeof(T);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].size = sizeof(T);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[0].size = sizeof(T);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[2].kernelArgPatchInfoVector[0].size = sizeof(T);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[1].kernelArgPatchInfoVector[0].size = sizeof(T);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = sizeof(T);

            kernelInfos[rootDeviceIndex] = pKernelInfo[rootDeviceIndex].get();
        }

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            pKernel[rootDeviceIndex] = new MockKernel(program.get(), *pKernelInfo[rootDeviceIndex], *deviceFactory->rootDevices[rootDeviceIndex]);
            kernels[rootDeviceIndex] = pKernel[rootDeviceIndex];
            ASSERT_EQ(CL_SUCCESS, pKernel[rootDeviceIndex]->initialize());
        }

        pMultiDeviceKernel = std::make_unique<MultiDeviceKernel>(kernels, kernelInfos);

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            pKernel[rootDeviceIndex]->setCrossThreadData(&pCrossThreadData[rootDeviceIndex], sizeof(pCrossThreadData[rootDeviceIndex]));
        }
    }

    void TearDown() override {
        MultiRootDeviceWithSubDevicesFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel;
    MockKernel *pKernel[3] = {nullptr};
    std::unique_ptr<KernelInfo> pKernelInfo[3];
    char pCrossThreadData[3][0x60];
};

typedef ::testing::Types<
    char,
    float,
    int,
    short,
    long,
    unsigned char,
    unsigned int,
    unsigned short,
    unsigned long>
    KernelArgImmediateTypes;

TYPED_TEST_CASE(KernelArgImmediateTest, KernelArgImmediateTypes);

TYPED_TEST(KernelArgImmediateTest, WhenSettingKernelArgThenArgIsSetCorrectly) {
    auto val = (TypeParam)0xaaaaaaaaULL;
    auto pVal = &val;
    this->pMultiDeviceKernel->setArg(0, sizeof(TypeParam), pVal);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        auto pKernelArg = (TypeParam *)(pKernel->getCrossThreadData() +
                                        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

        EXPECT_EQ(val, *pKernelArg);
    }
}

TYPED_TEST(KernelArgImmediateTest, GivenInvalidIndexWhenSettingKernelArgThenInvalidArgIndexErrorIsReturned) {
    auto val = (TypeParam)0U;
    auto pVal = &val;
    auto ret = this->pMultiDeviceKernel->setArg((uint32_t)-1, sizeof(TypeParam), pVal);

    EXPECT_EQ(ret, CL_INVALID_ARG_INDEX);
}

TYPED_TEST(KernelArgImmediateTest, GivenMultipleArgumentsWhenSettingKernelArgThenEachArgIsSetCorrectly) {
    auto val = (TypeParam)0xaaaaaaaaULL;
    auto pVal = &val;

    this->pMultiDeviceKernel->setArg(0, sizeof(TypeParam), pVal);
    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);

        auto pKernelArg = (TypeParam *)(pKernel->getCrossThreadData() +
                                        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

        EXPECT_EQ(val, *pKernelArg);
    }
    val = (TypeParam)0xbbbbbbbbULL;
    this->pMultiDeviceKernel->setArg(1, sizeof(TypeParam), &val);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        auto pKernelArg = (TypeParam *)(pKernel->getCrossThreadData() +
                                        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset);

        EXPECT_EQ(val, *pKernelArg);
    }
    val = (TypeParam)0xccccccccULL;
    this->pMultiDeviceKernel->setArg(2, sizeof(TypeParam), &val);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        auto pKernelArg = (TypeParam *)(pKernel->getCrossThreadData() +
                                        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset);

        EXPECT_EQ(val, *pKernelArg);
    }
}

TYPED_TEST(KernelArgImmediateTest, GivenCrossThreadDataOverwritesWhenSettingKernelArgThenArgsAreSetCorrectly) {
    TypeParam val = (TypeParam)0xaaaaaaaaULL;
    TypeParam *pVal = &val;
    this->pMultiDeviceKernel->setArg(0, sizeof(TypeParam), pVal);
    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);

        TypeParam *pKernelArg = (TypeParam *)(pKernel->getCrossThreadData() +
                                              this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

        EXPECT_EQ(val, *pKernelArg);
    }

    val = (TypeParam)0xbbbbbbbbULL;
    this->pMultiDeviceKernel->setArg(1, sizeof(TypeParam), &val);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        auto pKernelArg = (TypeParam *)(pKernel->getCrossThreadData() +
                                        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset);

        EXPECT_EQ(val, *pKernelArg);
    }
    val = (TypeParam)0xccccccccULL;
    this->pMultiDeviceKernel->setArg(0, sizeof(TypeParam), &val);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        auto pKernelArg = (TypeParam *)(pKernel->getCrossThreadData() +
                                        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

        EXPECT_EQ(val, *pKernelArg);
    }
}

TYPED_TEST(KernelArgImmediateTest, GivenMultipleStructElementsWhenSettingKernelArgThenArgsAreSetCorrectly) {
    struct ImmediateStruct {
        TypeParam a;
        unsigned char unused[3]; // want to force a gap, ideally unpadded
        TypeParam b;
    } immediateStruct;

    immediateStruct.a = (TypeParam)0xaaaaaaaaULL;
    immediateStruct.b = (TypeParam)0xbbbbbbbbULL;
    immediateStruct.unused[0] = 0xfe;
    immediateStruct.unused[1] = 0xfe;
    immediateStruct.unused[2] = 0xfe;

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[0].sourceOffset = offsetof(struct ImmediateStruct, a);
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].sourceOffset = offsetof(struct ImmediateStruct, b);
    }
    this->pMultiDeviceKernel->setArg(3, sizeof(immediateStruct), &immediateStruct);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        auto pCrossthreadA = (TypeParam *)(pKernel->getCrossThreadData() +
                                           this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[0].crossthreadOffset);
        EXPECT_EQ(immediateStruct.a, *pCrossthreadA);

        auto pCrossthreadB = (TypeParam *)(pKernel->getCrossThreadData() +
                                           this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].crossthreadOffset);
        EXPECT_EQ(immediateStruct.b, *pCrossthreadB);
    }
}

TYPED_TEST(KernelArgImmediateTest, givenTooLargePatchSizeWhenSettingArgThenDontReadMemoryBeyondLimit) {

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        TypeParam memory[2];
        std::memset(&memory[0], 0xaa, sizeof(TypeParam));
        std::memset(&memory[1], 0xbb, sizeof(TypeParam));

        const auto destinationMemoryAddress = pKernel->getCrossThreadData() +
                                              this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset;
        const auto memoryBeyondLimitAddress = destinationMemoryAddress + sizeof(TypeParam);

        const auto memoryBeyondLimitBefore = *reinterpret_cast<TypeParam *>(memoryBeyondLimitAddress);

        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = sizeof(TypeParam) + 1;
        auto retVal = pKernel->setArg(0, sizeof(TypeParam), &memory[0]);

        const auto memoryBeyondLimitAfter = *reinterpret_cast<TypeParam *>(memoryBeyondLimitAddress);
        EXPECT_EQ(memoryBeyondLimitBefore, memoryBeyondLimitAfter);
        EXPECT_EQ(memory[0], *reinterpret_cast<TypeParam *>(destinationMemoryAddress));

        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TYPED_TEST(KernelArgImmediateTest, givenNotTooLargePatchSizeWhenSettingArgThenDontReadMemoryBeyondLimit) {

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        TypeParam memory[2];
        std::memset(&memory[0], 0xaa, sizeof(TypeParam));
        std::memset(&memory[1], 0xbb, sizeof(TypeParam));

        const auto destinationMemoryAddress = pKernel->getCrossThreadData() +
                                              this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset;
        const auto memoryBeyondLimitAddress = destinationMemoryAddress + sizeof(TypeParam);

        const auto memoryBeyondLimitBefore = *reinterpret_cast<TypeParam *>(memoryBeyondLimitAddress);

        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = sizeof(TypeParam);
        auto retVal = pKernel->setArg(0, sizeof(TypeParam), &memory[0]);

        const auto memoryBeyondLimitAfter = *reinterpret_cast<TypeParam *>(memoryBeyondLimitAddress);
        EXPECT_EQ(memoryBeyondLimitBefore, memoryBeyondLimitAfter);
        EXPECT_EQ(memory[0], *reinterpret_cast<TypeParam *>(destinationMemoryAddress));

        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TYPED_TEST(KernelArgImmediateTest, givenMulitplePatchesAndFirstPatchSizeTooLargeWhenSettingArgThenDontReadMemoryBeyondLimit) {
    if (sizeof(TypeParam) == 1)
        return; // multiple patch chars don't make sense

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        TypeParam memory[2];
        std::memset(&memory[0], 0xaa, sizeof(TypeParam));
        std::memset(&memory[1], 0xbb, sizeof(TypeParam));

        const auto destinationMemoryAddress1 = pKernel->getCrossThreadData() +
                                               this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].crossthreadOffset;
        const auto destinationMemoryAddress2 = pKernel->getCrossThreadData() +
                                               this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].crossthreadOffset;
        const auto memoryBeyondLimitAddress1 = destinationMemoryAddress1 + sizeof(TypeParam);
        const auto memoryBeyondLimitAddress2 = destinationMemoryAddress2 + sizeof(TypeParam) / 2;

        const std::vector<unsigned char> memoryBeyondLimitBefore1(memoryBeyondLimitAddress1, memoryBeyondLimitAddress1 + sizeof(TypeParam));
        const std::vector<unsigned char> memoryBeyondLimitBefore2(memoryBeyondLimitAddress2, memoryBeyondLimitAddress2 + sizeof(TypeParam) / 2);

        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].sourceOffset = 0;
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].sourceOffset = sizeof(TypeParam) / 2;
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].size = sizeof(TypeParam);
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].size = sizeof(TypeParam) / 2;
        auto retVal = pKernel->setArg(3, sizeof(TypeParam), &memory[0]);

        EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore1.data(), memoryBeyondLimitAddress1, sizeof(TypeParam)));
        EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore2.data(), memoryBeyondLimitAddress2, sizeof(TypeParam) / 2));

        EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress1, sizeof(TypeParam)));
        EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress2, sizeof(TypeParam) / 2));

        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TYPED_TEST(KernelArgImmediateTest, givenMulitplePatchesAndSecondPatchSizeTooLargeWhenSettingArgThenDontReadMemoryBeyondLimit) {
    if (sizeof(TypeParam) == 1)
        return; // multiple patch chars don't make sense

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        TypeParam memory[2];
        std::memset(&memory[0], 0xaa, sizeof(TypeParam));
        std::memset(&memory[1], 0xbb, sizeof(TypeParam));

        const auto destinationMemoryAddress1 = pKernel->getCrossThreadData() +
                                               this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].crossthreadOffset;
        const auto destinationMemoryAddress2 = pKernel->getCrossThreadData() +
                                               this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].crossthreadOffset;
        const auto memoryBeyondLimitAddress1 = destinationMemoryAddress1 + sizeof(TypeParam) / 2;
        const auto memoryBeyondLimitAddress2 = destinationMemoryAddress2 + sizeof(TypeParam) / 2;

        const std::vector<unsigned char> memoryBeyondLimitBefore1(memoryBeyondLimitAddress1, memoryBeyondLimitAddress1 + sizeof(TypeParam) / 2);
        const std::vector<unsigned char> memoryBeyondLimitBefore2(memoryBeyondLimitAddress2, memoryBeyondLimitAddress2 + sizeof(TypeParam) / 2);

        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[0].size = 0;
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].sourceOffset = 0;
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].sourceOffset = sizeof(TypeParam) / 2;
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].size = sizeof(TypeParam) / 2;
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].size = sizeof(TypeParam);
        auto retVal = pKernel->setArg(3, sizeof(TypeParam), &memory[0]);

        EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore1.data(), memoryBeyondLimitAddress1, sizeof(TypeParam) / 2));
        EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore2.data(), memoryBeyondLimitAddress2, sizeof(TypeParam) / 2));

        EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress1, sizeof(TypeParam) / 2));
        EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress2, sizeof(TypeParam) / 2));

        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TYPED_TEST(KernelArgImmediateTest, givenMultiplePatchesAndOneSourceOffsetBeyondArgumentWhenSettingArgThenDontCopyThisPatch) {
    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
        TypeParam memory[2];
        std::memset(&memory[0], 0xaa, sizeof(TypeParam));
        std::memset(&memory[1], 0xbb, sizeof(TypeParam));

        const auto destinationMemoryAddress1 = pKernel->getCrossThreadData() +
                                               this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].crossthreadOffset;
        const auto destinationMemoryAddress2 = pKernel->getCrossThreadData() +
                                               this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].crossthreadOffset;
        const auto memoryBeyondLimitAddress1 = destinationMemoryAddress1 + sizeof(TypeParam);
        const auto memoryBeyondLimitAddress2 = destinationMemoryAddress2;

        const std::vector<unsigned char> memoryBeyondLimitBefore1(memoryBeyondLimitAddress1, memoryBeyondLimitAddress1 + sizeof(TypeParam));
        const std::vector<unsigned char> memoryBeyondLimitBefore2(memoryBeyondLimitAddress2, memoryBeyondLimitAddress2 + sizeof(TypeParam));

        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[0].size = 0;
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].sourceOffset = 0;
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[1].size = sizeof(TypeParam);
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].sourceOffset = sizeof(TypeParam);
        this->pKernelInfo[rootDeviceIndex]->kernelArgInfo[3].kernelArgPatchInfoVector[2].size = 1;
        auto retVal = pKernel->setArg(3, sizeof(TypeParam), &memory[0]);

        EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore1.data(), memoryBeyondLimitAddress1, memoryBeyondLimitBefore1.size()));
        EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore2.data(), memoryBeyondLimitAddress2, memoryBeyondLimitBefore2.size()));

        EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress1, sizeof(TypeParam)));

        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}
