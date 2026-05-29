/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

using namespace NEO;

template <typename T>
struct KernelArgTypeTag {
    using type = T;
};

template <typename... Types>
struct KernelArgTypeList {
    template <typename F>
    static void forEach(F &&func) {
        (func(KernelArgTypeTag<Types>{}), ...);
    }
};

using KernelArgImmediateTypes = KernelArgTypeList<
    char, float, int, short, long, unsigned char, unsigned int, unsigned short, unsigned long>;

class KernelArgImmediateTest : public MultiRootDeviceWithSubDevicesFixture {
  protected:
    void SetUp() override {
        MultiRootDeviceWithSubDevicesFixture::SetUp();
        this->program = std::make_unique<MockProgram>(this->context.get(), false, this->context->getDevices());
    }

    void TearDown() override {
        this->resetKernel();
        MultiRootDeviceWithSubDevicesFixture::TearDown();
    }

    template <typename T>
    void setupKernel() {
        KernelInfoContainer kernelInfos;
        kernelInfos.resize(3);
        KernelVectorType kernels;
        kernels.resize(3);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            memset(&this->pCrossThreadData[rootDeviceIndex], 0xfe, sizeof(this->pCrossThreadData[rootDeviceIndex]));

            this->pKernelInfo = std::make_unique<MockKernelInfo>();
            this->pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

            this->pKernelInfo->addArgImmediate(0, sizeof(T), 0x50);
            this->pKernelInfo->addArgImmediate(1, sizeof(T), 0x40);
            this->pKernelInfo->addArgImmediate(2, sizeof(T), 0x30);
            this->pKernelInfo->addArgImmediate(3, sizeof(T), 0x20);
            this->pKernelInfo->argAsVal(3).elements.push_back(ArgDescValue::Element{0x28, sizeof(T), 0});
            this->pKernelInfo->argAsVal(3).elements.push_back(ArgDescValue::Element{0x38, sizeof(T), 0});

            kernelInfos[rootDeviceIndex] = this->pKernelInfo.get();
        }

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            this->pKernel[rootDeviceIndex] = new MockKernel(this->program.get(), *this->pKernelInfo, *this->deviceFactory->rootDevices[rootDeviceIndex]);
            kernels[rootDeviceIndex] = this->pKernel[rootDeviceIndex];
            ASSERT_EQ(CL_SUCCESS, this->pKernel[rootDeviceIndex]->initialize());
        }

        this->pMultiDeviceKernel = std::make_unique<MultiDeviceKernel>(kernels, kernelInfos);

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            this->pKernel[rootDeviceIndex]->setCrossThreadData(&this->pCrossThreadData[rootDeviceIndex], sizeof(this->pCrossThreadData[rootDeviceIndex]));
        }
    }

    void resetKernel() {
        this->pMultiDeviceKernel.reset();
        this->pKernelInfo.reset();
        for (auto &p : this->pKernel) {
            p = nullptr;
        }
    }

    template <typename T>
    void runWhenSettingKernelArgThenArgIsSetCorrectly() {
        this->template setupKernel<T>();
        auto val = (T)0xaaaaaaaaULL;
        auto pVal = &val;
        this->pMultiDeviceKernel->setArg(0, sizeof(T), pVal);

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            auto pKernelArg = (T *)(pKernel->getCrossThreadData() +
                                    this->pKernelInfo->argAsVal(0).elements[0].offset);
            EXPECT_EQ(val, *pKernelArg);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenInvalidIndexWhenSettingKernelArgThenInvalidArgIndexErrorIsReturned() {
        this->template setupKernel<T>();
        auto val = (T)0U;
        auto pVal = &val;
        auto ret = this->pMultiDeviceKernel->setArg((uint32_t)-1, sizeof(T), pVal);
        EXPECT_EQ(ret, CL_INVALID_ARG_INDEX);
        this->resetKernel();
    }

    template <typename T>
    void runGivenMultipleArgumentsWhenSettingKernelArgThenEachArgIsSetCorrectly() {
        this->template setupKernel<T>();
        auto val = (T)0xaaaaaaaaULL;
        auto pVal = &val;

        this->pMultiDeviceKernel->setArg(0, sizeof(T), pVal);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            auto pKernelArg = (T *)(pKernel->getCrossThreadData() +
                                    this->pKernelInfo->argAsVal(0).elements[0].offset);
            EXPECT_EQ(val, *pKernelArg);
        }
        val = (T)0xbbbbbbbbULL;
        this->pMultiDeviceKernel->setArg(1, sizeof(T), &val);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            auto pKernelArg = (T *)(pKernel->getCrossThreadData() +
                                    this->pKernelInfo->argAsVal(1).elements[0].offset);
            EXPECT_EQ(val, *pKernelArg);
        }
        val = (T)0xccccccccULL;
        this->pMultiDeviceKernel->setArg(2, sizeof(T), &val);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            auto pKernelArg = (T *)(pKernel->getCrossThreadData() +
                                    this->pKernelInfo->argAsVal(2).elements[0].offset);
            EXPECT_EQ(val, *pKernelArg);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenCrossThreadDataOverwritesWhenSettingKernelArgThenArgsAreSetCorrectly() {
        this->template setupKernel<T>();
        T val = (T)0xaaaaaaaaULL;
        T *pVal = &val;
        this->pMultiDeviceKernel->setArg(0, sizeof(T), pVal);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            T *pKernelArg = (T *)(pKernel->getCrossThreadData() +
                                  this->pKernelInfo->argAsVal(0).elements[0].offset);
            EXPECT_EQ(val, *pKernelArg);
        }
        val = (T)0xbbbbbbbbULL;
        this->pMultiDeviceKernel->setArg(1, sizeof(T), &val);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            auto pKernelArg = (T *)(pKernel->getCrossThreadData() +
                                    this->pKernelInfo->argAsVal(1).elements[0].offset);
            EXPECT_EQ(val, *pKernelArg);
        }
        val = (T)0xccccccccULL;
        this->pMultiDeviceKernel->setArg(0, sizeof(T), &val);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            auto pKernelArg = (T *)(pKernel->getCrossThreadData() +
                                    this->pKernelInfo->argAsVal(0).elements[0].offset);
            EXPECT_EQ(val, *pKernelArg);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenMultipleStructElementsWhenSettingKernelArgThenArgsAreSetCorrectly() {
        this->template setupKernel<T>();
        struct ImmediateStruct {
            T a;
            unsigned char unused[3]; // want to force a gap, ideally unpadded
            T b;
        } immediateStruct;

        immediateStruct.a = (T)0xaaaaaaaaULL;
        immediateStruct.b = (T)0xbbbbbbbbULL;
        immediateStruct.unused[0] = 0xfe;
        immediateStruct.unused[1] = 0xfe;
        immediateStruct.unused[2] = 0xfe;

        auto &elements = this->pKernelInfo->argAsVal(3).elements;
        elements[0].sourceOffset = offsetof(struct ImmediateStruct, a);
        elements[1].sourceOffset = offsetof(struct ImmediateStruct, b);

        this->pKernelInfo->kernelDescriptor.explicitArgsExtendedMetadata[3].typeSize = sizeof(immediateStruct);
        this->pMultiDeviceKernel->setArg(3, sizeof(immediateStruct), &immediateStruct);

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            auto pCrossthreadA = (T *)(pKernel->getCrossThreadData() + elements[0].offset);
            EXPECT_EQ(immediateStruct.a, *pCrossthreadA);
            auto pCrossthreadB = (T *)(pKernel->getCrossThreadData() + elements[1].offset);
            EXPECT_EQ(immediateStruct.b, *pCrossthreadB);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenStructWithTrailingPaddingWhenSettingKernelArgThenArgsAreSetCorrectly() {
        if (sizeof(T) == 1) {
            return; // no trailing padding for char-sized types
        }
        this->template setupKernel<T>();
        struct ImmediateStruct {
            T a;
            char b;
        } immediateStruct;

        immediateStruct.a = (T)0xaaaaaaaaULL;
        immediateStruct.b = 'x';

        auto &elements = this->pKernelInfo->argAsVal(3).elements;
        elements.resize(2);
        elements[0].sourceOffset = offsetof(struct ImmediateStruct, a);
        elements[0].size = sizeof(T);
        elements[1].sourceOffset = offsetof(struct ImmediateStruct, b);
        elements[1].size = sizeof(char);

        this->pKernelInfo->kernelDescriptor.explicitArgsExtendedMetadata[3].typeSize = sizeof(immediateStruct);
        auto retVal = this->pMultiDeviceKernel->setArg(3, sizeof(immediateStruct), &immediateStruct);
        EXPECT_EQ(CL_SUCCESS, retVal);

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            auto pCrossthreadA = (T *)(pKernel->getCrossThreadData() + elements[0].offset);
            EXPECT_EQ(immediateStruct.a, *pCrossthreadA);
            auto pCrossthreadB = pKernel->getCrossThreadData() + elements[1].offset;
            EXPECT_EQ(immediateStruct.b, *pCrossthreadB);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenTooLargePatchSizeWhenSettingArgThenDontReadMemoryBeyondLimit() {
        this->template setupKernel<T>();
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            T memory[2];
            std::memset(&memory[0], 0xaa, sizeof(T));
            std::memset(&memory[1], 0xbb, sizeof(T));

            const auto destinationMemoryAddress = pKernel->getCrossThreadData() +
                                                  this->pKernelInfo->argAsVal(0).elements[0].offset;
            const auto memoryBeyondLimitAddress = destinationMemoryAddress + sizeof(T);
            const auto memoryBeyondLimitBefore = *reinterpret_cast<T *>(memoryBeyondLimitAddress);

            this->pKernelInfo->argAsVal(0).elements[0].size = sizeof(T) + 1;
            auto retVal = pKernel->setArg(0, sizeof(T), &memory[0]);

            const auto memoryBeyondLimitAfter = *reinterpret_cast<T *>(memoryBeyondLimitAddress);
            EXPECT_EQ(memoryBeyondLimitBefore, memoryBeyondLimitAfter);
            EXPECT_EQ(memory[0], *reinterpret_cast<T *>(destinationMemoryAddress));
            EXPECT_EQ(CL_SUCCESS, retVal);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenNotTooLargePatchSizeWhenSettingArgThenDontReadMemoryBeyondLimit() {
        this->template setupKernel<T>();
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            T memory[2];
            std::memset(&memory[0], 0xaa, sizeof(T));
            std::memset(&memory[1], 0xbb, sizeof(T));

            const auto destinationMemoryAddress = pKernel->getCrossThreadData() +
                                                  this->pKernelInfo->argAsVal(0).elements[0].offset;
            const auto memoryBeyondLimitAddress = destinationMemoryAddress + sizeof(T);
            const auto memoryBeyondLimitBefore = *reinterpret_cast<T *>(memoryBeyondLimitAddress);

            this->pKernelInfo->argAsVal(0).elements[0].size = sizeof(T);
            auto retVal = pKernel->setArg(0, sizeof(T), &memory[0]);

            const auto memoryBeyondLimitAfter = *reinterpret_cast<T *>(memoryBeyondLimitAddress);
            EXPECT_EQ(memoryBeyondLimitBefore, memoryBeyondLimitAfter);
            EXPECT_EQ(memory[0], *reinterpret_cast<T *>(destinationMemoryAddress));
            EXPECT_EQ(CL_SUCCESS, retVal);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenMulitplePatchesAndFirstPatchSizeTooLargeWhenSettingArgThenDontReadMemoryBeyondLimit() {
        if (sizeof(T) == 1) {
            return; // multiple patch chars don't make sense
        }
        this->template setupKernel<T>();
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            T memory[2];
            std::memset(&memory[0], 0xaa, sizeof(T));
            std::memset(&memory[1], 0xbb, sizeof(T));

            auto &elements = this->pKernelInfo->argAsVal(3).elements;
            const auto destinationMemoryAddress1 = pKernel->getCrossThreadData() + elements[2].offset;
            const auto destinationMemoryAddress2 = pKernel->getCrossThreadData() + elements[1].offset;
            const auto memoryBeyondLimitAddress1 = destinationMemoryAddress1 + sizeof(T);
            const auto memoryBeyondLimitAddress2 = destinationMemoryAddress2 + sizeof(T) / 2;

            const std::vector<unsigned char> memoryBeyondLimitBefore1(memoryBeyondLimitAddress1, memoryBeyondLimitAddress1 + sizeof(T));
            const std::vector<unsigned char> memoryBeyondLimitBefore2(memoryBeyondLimitAddress2, memoryBeyondLimitAddress2 + sizeof(T) / 2);

            elements[2].sourceOffset = 0;
            elements[1].sourceOffset = sizeof(T) / 2;
            elements[2].size = sizeof(T);
            elements[1].size = sizeof(T) / 2;
            auto retVal = pKernel->setArg(3, sizeof(T), &memory[0]);

            EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore1.data(), memoryBeyondLimitAddress1, sizeof(T)));
            EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore2.data(), memoryBeyondLimitAddress2, sizeof(T) / 2));
            EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress1, sizeof(T)));
            EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress2, sizeof(T) / 2));
            EXPECT_EQ(CL_SUCCESS, retVal);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenMulitplePatchesAndSecondPatchSizeTooLargeWhenSettingArgThenDontReadMemoryBeyondLimit() {
        if (sizeof(T) == 1) {
            return; // multiple patch chars don't make sense
        }
        this->template setupKernel<T>();
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            T memory[2];
            std::memset(&memory[0], 0xaa, sizeof(T));
            std::memset(&memory[1], 0xbb, sizeof(T));

            auto &elements = this->pKernelInfo->argAsVal(3).elements;
            const auto destinationMemoryAddress1 = pKernel->getCrossThreadData() + elements[2].offset;
            const auto destinationMemoryAddress2 = pKernel->getCrossThreadData() + elements[1].offset;
            const auto memoryBeyondLimitAddress1 = destinationMemoryAddress1 + sizeof(T) / 2;
            const auto memoryBeyondLimitAddress2 = destinationMemoryAddress2 + sizeof(T) / 2;

            const std::vector<unsigned char> memoryBeyondLimitBefore1(memoryBeyondLimitAddress1, memoryBeyondLimitAddress1 + sizeof(T) / 2);
            const std::vector<unsigned char> memoryBeyondLimitBefore2(memoryBeyondLimitAddress2, memoryBeyondLimitAddress2 + sizeof(T) / 2);

            elements[0].size = 0;
            elements[2].sourceOffset = 0;
            elements[1].sourceOffset = sizeof(T) / 2;
            elements[2].size = sizeof(T) / 2;
            elements[1].size = sizeof(T);
            auto retVal = pKernel->setArg(3, sizeof(T), &memory[0]);

            EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore1.data(), memoryBeyondLimitAddress1, sizeof(T) / 2));
            EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore2.data(), memoryBeyondLimitAddress2, sizeof(T) / 2));
            EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress1, sizeof(T) / 2));
            EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress2, sizeof(T) / 2));
            EXPECT_EQ(CL_SUCCESS, retVal);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenMultiplePatchesAndOneSourceOffsetBeyondArgumentWhenSettingArgThenDontCopyThisPatch() {
        this->template setupKernel<T>();
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            auto pKernel = this->pMultiDeviceKernel->getKernel(rootDeviceIndex);
            T memory[2];
            std::memset(&memory[0], 0xaa, sizeof(T));
            std::memset(&memory[1], 0xbb, sizeof(T));

            auto &elements = this->pKernelInfo->argAsVal(3).elements;
            const auto destinationMemoryAddress1 = pKernel->getCrossThreadData() + elements[1].offset;
            const auto destinationMemoryAddress2 = pKernel->getCrossThreadData() + elements[2].offset;
            const auto memoryBeyondLimitAddress1 = destinationMemoryAddress1 + sizeof(T);
            const auto memoryBeyondLimitAddress2 = destinationMemoryAddress2;

            const std::vector<unsigned char> memoryBeyondLimitBefore1(memoryBeyondLimitAddress1, memoryBeyondLimitAddress1 + sizeof(T));
            const std::vector<unsigned char> memoryBeyondLimitBefore2(memoryBeyondLimitAddress2, memoryBeyondLimitAddress2 + sizeof(T));

            elements[0].size = 0;
            elements[1].sourceOffset = 0;
            elements[1].size = sizeof(T);
            elements[2].sourceOffset = sizeof(T);
            elements[2].size = 1;
            auto retVal = pKernel->setArg(3, sizeof(T), &memory[0]);

            EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore1.data(), memoryBeyondLimitAddress1, memoryBeyondLimitBefore1.size()));
            EXPECT_EQ(0, std::memcmp(memoryBeyondLimitBefore2.data(), memoryBeyondLimitAddress2, memoryBeyondLimitBefore2.size()));
            EXPECT_EQ(0, std::memcmp(&memory[0], destinationMemoryAddress1, sizeof(T)));
            EXPECT_EQ(CL_SUCCESS, retVal);
        }
        this->resetKernel();
    }

    template <typename T>
    void runGivenArgSizeSmallerThanRequiredWhenSettingArgThenInvalidArgSizeIsReturned() {
        this->template setupKernel<T>();
        T val = (T)0xaaaaaaaaULL;
        auto retVal = this->pMultiDeviceKernel->setArg(0, sizeof(T) - 1, &val);
        EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
        this->resetKernel();
    }

    template <typename T>
    void runGivenMultipleElementsAndArgSizeSmallerThanRequiredWhenSettingArgThenInvalidArgSizeIsReturned() {
        this->template setupKernel<T>();
        struct ImmediateStruct {
            T a;
            T b;
            T c;
        } immediateStruct;

        immediateStruct.a = (T)0xaaaaaaaaULL;
        immediateStruct.b = (T)0xbbbbbbbbULL;
        immediateStruct.c = (T)0xccccccccULL;

        auto &elements = this->pKernelInfo->argAsVal(3).elements;
        elements[0].sourceOffset = offsetof(struct ImmediateStruct, a);
        elements[0].size = sizeof(T);
        elements[1].sourceOffset = offsetof(struct ImmediateStruct, b);
        elements[1].size = sizeof(T);
        elements[2].sourceOffset = offsetof(struct ImmediateStruct, c);
        elements[2].size = sizeof(T);

        this->pKernelInfo->kernelDescriptor.explicitArgsExtendedMetadata[3].typeSize = sizeof(ImmediateStruct);
        auto retVal = this->pMultiDeviceKernel->setArg(3, sizeof(T) * 2, &immediateStruct);
        EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
        this->resetKernel();
    }

    template <typename T>
    void runGivenMultipleElementsAndExactArgSizeWhenSettingArgThenSuccessIsReturned() {
        this->template setupKernel<T>();
        struct ImmediateStruct {
            T a;
            T b;
            T c;
        } immediateStruct;

        immediateStruct.a = (T)0xaaaaaaaaULL;
        immediateStruct.b = (T)0xbbbbbbbbULL;
        immediateStruct.c = (T)0xccccccccULL;

        auto &elements = this->pKernelInfo->argAsVal(3).elements;
        elements[0].sourceOffset = offsetof(struct ImmediateStruct, a);
        elements[0].size = sizeof(T);
        elements[1].sourceOffset = offsetof(struct ImmediateStruct, b);
        elements[1].size = sizeof(T);
        elements[2].sourceOffset = offsetof(struct ImmediateStruct, c);
        elements[2].size = sizeof(T);

        this->pKernelInfo->kernelDescriptor.explicitArgsExtendedMetadata[3].typeSize = sizeof(ImmediateStruct);
        auto retVal = this->pMultiDeviceKernel->setArg(3, sizeof(ImmediateStruct), &immediateStruct);
        EXPECT_EQ(CL_SUCCESS, retVal);
        this->resetKernel();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel;
    MockKernel *pKernel[3] = {nullptr};
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char pCrossThreadData[3][0x60];
};

TEST_F(KernelArgImmediateTest, WhenSettingKernelArgThenArgIsSetCorrectly) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runWhenSettingKernelArgThenArgIsSetCorrectly<T>();
    });
}

TEST_F(KernelArgImmediateTest, GivenInvalidIndexWhenSettingKernelArgThenInvalidArgIndexErrorIsReturned) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenInvalidIndexWhenSettingKernelArgThenInvalidArgIndexErrorIsReturned<T>();
    });
}

TEST_F(KernelArgImmediateTest, GivenMultipleArgumentsWhenSettingKernelArgThenEachArgIsSetCorrectly) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenMultipleArgumentsWhenSettingKernelArgThenEachArgIsSetCorrectly<T>();
    });
}

TEST_F(KernelArgImmediateTest, GivenCrossThreadDataOverwritesWhenSettingKernelArgThenArgsAreSetCorrectly) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenCrossThreadDataOverwritesWhenSettingKernelArgThenArgsAreSetCorrectly<T>();
    });
}

TEST_F(KernelArgImmediateTest, GivenMultipleStructElementsWhenSettingKernelArgThenArgsAreSetCorrectly) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenMultipleStructElementsWhenSettingKernelArgThenArgsAreSetCorrectly<T>();
    });
}

TEST_F(KernelArgImmediateTest, GivenStructWithTrailingPaddingWhenSettingKernelArgThenArgsAreSetCorrectly) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenStructWithTrailingPaddingWhenSettingKernelArgThenArgsAreSetCorrectly<T>();
    });
}

TEST_F(KernelArgImmediateTest, givenTooLargePatchSizeWhenSettingArgThenDontReadMemoryBeyondLimit) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenTooLargePatchSizeWhenSettingArgThenDontReadMemoryBeyondLimit<T>();
    });
}

TEST_F(KernelArgImmediateTest, givenNotTooLargePatchSizeWhenSettingArgThenDontReadMemoryBeyondLimit) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenNotTooLargePatchSizeWhenSettingArgThenDontReadMemoryBeyondLimit<T>();
    });
}

TEST_F(KernelArgImmediateTest, givenMulitplePatchesAndFirstPatchSizeTooLargeWhenSettingArgThenDontReadMemoryBeyondLimit) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenMulitplePatchesAndFirstPatchSizeTooLargeWhenSettingArgThenDontReadMemoryBeyondLimit<T>();
    });
}

TEST_F(KernelArgImmediateTest, givenMulitplePatchesAndSecondPatchSizeTooLargeWhenSettingArgThenDontReadMemoryBeyondLimit) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenMulitplePatchesAndSecondPatchSizeTooLargeWhenSettingArgThenDontReadMemoryBeyondLimit<T>();
    });
}

TEST_F(KernelArgImmediateTest, givenMultiplePatchesAndOneSourceOffsetBeyondArgumentWhenSettingArgThenDontCopyThisPatch) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenMultiplePatchesAndOneSourceOffsetBeyondArgumentWhenSettingArgThenDontCopyThisPatch<T>();
    });
}

TEST_F(KernelArgImmediateTest, GivenArgSizeSmallerThanRequiredWhenSettingArgThenInvalidArgSizeIsReturned) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenArgSizeSmallerThanRequiredWhenSettingArgThenInvalidArgSizeIsReturned<T>();
    });
}

TEST_F(KernelArgImmediateTest, GivenMultipleElementsAndArgSizeSmallerThanRequiredWhenSettingArgThenInvalidArgSizeIsReturned) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenMultipleElementsAndArgSizeSmallerThanRequiredWhenSettingArgThenInvalidArgSizeIsReturned<T>();
    });
}

TEST_F(KernelArgImmediateTest, GivenMultipleElementsAndExactArgSizeWhenSettingArgThenSuccessIsReturned) {
    KernelArgImmediateTypes::forEach([this](auto tag) {
        using T = typename decltype(tag)::type;
        this->template runGivenMultipleElementsAndExactArgSizeWhenSettingArgThenSuccessIsReturned<T>();
    });
}
