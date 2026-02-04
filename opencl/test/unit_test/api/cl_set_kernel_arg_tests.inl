/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/device_binary_format/zebin/zeinfo.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_kernel_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

class KernelArgSamplerApiFixture : public ApiFixture {
  protected:
    void setUp() {
        ApiFixture::setUp();

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->heapInfo.pDsh = samplerStateHeap;
        pKernelInfo->heapInfo.dynamicStateHeapSize = sizeof(samplerStateHeap);

        pKernelInfo->addArgSampler(0, 0x40);

        cl_int retVal{CL_SUCCESS};
        pMockMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex), retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        pMockKernel = static_cast<MockKernel *>(pMockMultiDeviceKernel->getKernel(testedRootDeviceIndex));
        ASSERT_NE(nullptr, pMockKernel);

        pMockKernel->setKernelArgHandler(0, &Kernel::setArgSampler);

        uint32_t crossThreadData[0x40] = {};
        pMockKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));

        pSampler = Sampler::create(pContext, CL_TRUE, CL_ADDRESS_MIRRORED_REPEAT, CL_FILTER_NEAREST, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pSampler);
    }

    void tearDown() {
        if (pSampler) {
            pSampler->release();
        }
        if (pMockMultiDeviceKernel) {
            delete pMockMultiDeviceKernel;
        }

        ApiFixture::tearDown();
    }

    MockKernel *pMockKernel = nullptr;
    MultiDeviceKernel *pMockMultiDeviceKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char samplerStateHeap[0x80];
    Sampler *pSampler = nullptr;
};

using clSetKernelArgSamplerTests = Test<KernelArgSamplerApiFixture>;

TEST_F(clSetKernelArgSamplerTests, GivenSamplerArgAndArgSizeSmallerThanRequiredWhenSettingKernelArgThenInvalidArgSizeIsReturned) {
    cl_sampler samplerObj = pSampler;

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(cl_sampler) / 2, &samplerObj);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
}

TEST_F(clSetKernelArgSamplerTests, GivenSamplerArgAndExactArgSizeWhenSettingKernelArgThenSuccessIsReturned) {
    cl_sampler samplerObj = pSampler;

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(cl_sampler), &samplerObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

template <bool multiElement>
class KernelArgImmediateApiFixture : public ApiFixture {
  protected:
    void setUp() {
        ApiFixture::setUp();

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(pSshLocal);
        pKernelInfo->heapInfo.pSsh = pSshLocal;

        pKernelInfo->addArgImmediate(0, sizeof(uint32_t), 0x10);
        if (multiElement) {
            pKernelInfo->addArgImmediate(0, sizeof(uint32_t), 0x20, sizeof(uint32_t));
        }

        cl_int retVal{CL_SUCCESS};
        pMockMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex), retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        pMockKernel = static_cast<MockKernel *>(pMockMultiDeviceKernel->getKernel(testedRootDeviceIndex));
        ASSERT_NE(nullptr, pMockKernel);
        pMockKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void tearDown() {
        if (pMockMultiDeviceKernel) {
            delete pMockMultiDeviceKernel;
        }

        ApiFixture::tearDown();
    }

    MockKernel *pMockKernel = nullptr;
    MultiDeviceKernel *pMockMultiDeviceKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char pSshLocal[64]{};
    char pCrossThreadData[64]{};
};

using clSetKernelArgImmediateTests = Test<KernelArgImmediateApiFixture<false>>;

TEST_F(clSetKernelArgImmediateTests, GivenImmediateArgAndArgSizeSmallerThanRequiredWhenSettingKernelArgThenInvalidArgSizeIsReturned) {
    uint16_t argValue = 0xabcd;

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(uint16_t), &argValue);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
}

TEST_F(clSetKernelArgImmediateTests, GivenImmediateArgAndExactArgSizeWhenSettingKernelArgThenSuccessIsReturned) {
    uint32_t argValue = 0x12345678;

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(uint32_t), &argValue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

using clSetKernelArgImmediateMultiElementTests = Test<KernelArgImmediateApiFixture<true>>;

TEST_F(clSetKernelArgImmediateMultiElementTests, GivenMultiElementImmediateArgAndArgSizeSmallerThanRequiredWhenSettingKernelArgThenInvalidArgSizeIsReturned) {
    struct {
        uint32_t a;
        uint32_t b;
    } argValue;

    argValue.a = 0xaaaaaaaa;
    argValue.b = 0xbbbbbbbb;

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(uint32_t), &argValue);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
}

TEST_F(clSetKernelArgImmediateMultiElementTests, GivenMultiElementImmediateArgAndExactArgSizeWhenSettingKernelArgThenSuccessIsReturned) {
    struct {
        uint32_t a;
        uint32_t b;
    } argValue;

    argValue.a = 0xaaaaaaaa;
    argValue.b = 0xbbbbbbbb;

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(argValue), &argValue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

class KernelArgImmediateWithTrailingPaddingApiFixture : public ApiFixture {
  protected:
    void setUp() {
        ApiFixture::setUp();

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(pSshLocal);
        pKernelInfo->heapInfo.pSsh = pSshLocal;

        pKernelInfo->addArgImmediate(0, sizeof(uint32_t), 0x10, 0);
        pKernelInfo->addArgImmediate(0, sizeof(char), 0x20, sizeof(uint32_t));

        // Override typeSize to the actual struct size which includes trailing padding
        pKernelInfo->kernelDescriptor.explicitArgsExtendedMetadata[0].typeSize = sizeof(PaddedStruct);

        cl_int retVal{CL_SUCCESS};
        pMockMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex), retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        pMockKernel = static_cast<MockKernel *>(pMockMultiDeviceKernel->getKernel(testedRootDeviceIndex));
        ASSERT_NE(nullptr, pMockKernel);
        pMockKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void tearDown() {
        if (pMockMultiDeviceKernel) {
            delete pMockMultiDeviceKernel;
        }

        ApiFixture::tearDown();
    }

    struct PaddedStruct {
        uint32_t a;
        char b;
    };

    MockKernel *pMockKernel = nullptr;
    MultiDeviceKernel *pMockMultiDeviceKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char pSshLocal[64]{};
    char pCrossThreadData[64]{};
};

using clSetKernelArgImmediateWithTrailingPaddingTests = Test<KernelArgImmediateWithTrailingPaddingApiFixture>;

TEST_F(clSetKernelArgImmediateWithTrailingPaddingTests, GivenStructWithTrailingPaddingAndExactArgSizeWhenSettingKernelArgThenSuccessIsReturned) {
    PaddedStruct argValue;
    argValue.a = 0xaaaaaaaa;
    argValue.b = 'x';

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(PaddedStruct), &argValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pCrossthreadA = reinterpret_cast<uint32_t *>(pMockKernel->getCrossThreadData() + 0x10);
    EXPECT_EQ(argValue.a, *pCrossthreadA);

    auto pCrossthreadB = pMockKernel->getCrossThreadData() + 0x20;
    EXPECT_EQ(argValue.b, *pCrossthreadB);
}

TEST_F(clSetKernelArgImmediateWithTrailingPaddingTests, GivenStructWithTrailingPaddingAndSizeTooSmallWhenSettingKernelArgThenInvalidArgSizeIsReturned) {
    PaddedStruct argValue;
    argValue.a = 0xaaaaaaaa;
    argValue.b = 'x';

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(uint32_t) + sizeof(char), &argValue);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
}

template <bool zeInfoPresent>
class KernelArgImmediateWithTrailingPaddingFromZeInfoApiFixture : public ApiFixture {
  protected:
    void setUp() {
        ApiFixture::setUp();

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
        pKernelInfo->kernelDescriptor.kernelMetadata.kernelName = "test_kernel";

        pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(pSshLocal);
        pKernelInfo->heapInfo.pSsh = pSshLocal;

        pKernelInfo->addArgImmediate(0, sizeof(uint32_t), 0x10, 0);
        pKernelInfo->addArgImmediate(0, sizeof(char), 0x20, sizeof(uint32_t));

        // Clear extended metadata so it gets populated from zeinfo
        pKernelInfo->kernelDescriptor.explicitArgsExtendedMetadata.clear();
        if (zeInfoPresent) {
            setupZeInfo();
        }

        cl_int retVal{CL_SUCCESS};
        pMockMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex), retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        pMockKernel = static_cast<MockKernel *>(pMockMultiDeviceKernel->getKernel(testedRootDeviceIndex));
        ASSERT_NE(nullptr, pMockKernel);
        pMockKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void setupZeInfo() {
        pProgram->callBasePopulateZebinExtendedArgsMetadataOnce = true;

        auto &buildInfo = pProgram->buildInfos[testedRootDeviceIndex];
        buildInfo.kernelInfoArray.push_back(pKernelInfo.get());

        zeInfo = R"===(
kernels:
  - name:                 test_kernel
    simd_size:            1
kernels_misc_info:
  - name:                 test_kernel
    args_info:
     - index:             0
       name:              arg1
       address_qualifier: __private
       access_qualifier:  NONE
       type_name:         'PaddedStruct;8'
       type_qualifiers:   NONE
...
)===";

        constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
        MockElfEncoder<numBits> elfEncoder;
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_REL;
        elfEncoder.appendSection(Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO,
                                 Zebin::Elf::SectionNames::zeInfo,
                                 ArrayRef<const uint8_t>::fromAny(zeInfo.c_str(), zeInfo.size()));
        elfStorage = elfEncoder.encode();
        buildInfo.unpackedDeviceBinary.reset(reinterpret_cast<char *>(elfStorage.data()));
        buildInfo.unpackedDeviceBinarySize = elfStorage.size();
        buildInfo.kernelMiscInfoPos = zeInfo.find(Zebin::ZeInfo::Tags::kernelMiscInfo.str());
        ASSERT_NE(std::string::npos, buildInfo.kernelMiscInfoPos);
    }

    void tearDown() {
        auto &buildInfo = pProgram->buildInfos[testedRootDeviceIndex];
        buildInfo.kernelInfoArray.clear();
        buildInfo.unpackedDeviceBinary.release();

        if (pMockMultiDeviceKernel) {
            delete pMockMultiDeviceKernel;
        }

        ApiFixture::tearDown();
    }

    struct PaddedStruct {
        uint32_t a;
        char b;
    };

    MockKernel *pMockKernel = nullptr;
    MultiDeviceKernel *pMockMultiDeviceKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char pSshLocal[64]{};
    char pCrossThreadData[64]{};
    std::string zeInfo;
    std::vector<uint8_t> elfStorage;
};

using clSetKernelArgImmediateWithTrailingPaddingFromZeInfoTests = Test<KernelArgImmediateWithTrailingPaddingFromZeInfoApiFixture<true>>;

TEST_F(clSetKernelArgImmediateWithTrailingPaddingFromZeInfoTests, GivenStructWithTrailingPaddingAndTypeSizeFromZeInfoWhenSettingKernelArgThenSuccessIsReturned) {
    PaddedStruct argValue;
    argValue.a = 0xaaaaaaaa;
    argValue.b = 'x';

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(PaddedStruct), &argValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pCrossthreadA = reinterpret_cast<uint32_t *>(pMockKernel->getCrossThreadData() + 0x10);
    EXPECT_EQ(argValue.a, *pCrossthreadA);

    auto pCrossthreadB = pMockKernel->getCrossThreadData() + 0x20;
    EXPECT_EQ(argValue.b, *pCrossthreadB);

    EXPECT_TRUE(pProgram->wasPopulateZebinExtendedArgsMetadataOnceCalled);
}

TEST_F(clSetKernelArgImmediateWithTrailingPaddingFromZeInfoTests, GivenStructWithTrailingPaddingAndTypeSizeFromZeInfoAndSizeTooSmallWhenSettingKernelArgThenInvalidArgSizeIsReturned) {
    PaddedStruct argValue;
    argValue.a = 0xaaaaaaaa;
    argValue.b = 'x';

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(uint32_t) + sizeof(char), &argValue);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
}

using clSetKernelArgImmediateWithTrailingPaddingFromZeInfoAbsentTests = Test<KernelArgImmediateWithTrailingPaddingFromZeInfoApiFixture<false>>;

TEST_F(clSetKernelArgImmediateWithTrailingPaddingFromZeInfoAbsentTests, GivenStructWithTrailingPaddingAndNoTypeSizeFromZeInfoWhenSettingKernelArgThenSuccessIsReturned) {
    PaddedStruct argValue;
    argValue.a = 0xaaaaaaaa;
    argValue.b = 'x';

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(PaddedStruct), &argValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pCrossthreadA = reinterpret_cast<uint32_t *>(pMockKernel->getCrossThreadData() + 0x10);
    EXPECT_EQ(argValue.a, *pCrossthreadA);

    auto pCrossthreadB = pMockKernel->getCrossThreadData() + 0x20;
    EXPECT_EQ(argValue.b, *pCrossthreadB);

    EXPECT_TRUE(pProgram->wasPopulateZebinExtendedArgsMetadataOnceCalled);
}

TEST_F(clSetKernelArgImmediateWithTrailingPaddingFromZeInfoAbsentTests, GivenStructWithTrailingPaddingAndNoTypeSizeFromZeInfoAndSizeTooSmallWhenSettingKernelArgThenSuccessIsReturned) {
    PaddedStruct argValue;
    argValue.a = 0xaaaaaaaa;
    argValue.b = 'x';

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(uint32_t) + sizeof(char), &argValue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
