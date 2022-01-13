/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/matchers.h"

#include "opencl/source/execution_model/device_enqueue.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/program/printf_handler.h"
#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_sampler.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "patch_list.h"

#include <algorithm>
#include <memory>

using namespace NEO;

class ReflectionSurfaceHelperTest : public testing::TestWithParam<std::tuple<const IGIL_KernelCurbeParams, const IGIL_KernelCurbeParams, bool>> {

  protected:
    ReflectionSurfaceHelperTest() {
    }

    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_P(ReflectionSurfaceHelperTest, WhenComparingThenResultIsCorrect) {
    IGIL_KernelCurbeParams curbeParamFirst, curbeParamSecond;
    bool result;
    std::tie(curbeParamFirst, curbeParamSecond, result) = GetParam();

    EXPECT_EQ(result, MockKernel::ReflectionSurfaceHelperPublic::compareFunction(curbeParamFirst, curbeParamSecond));
}

// arg first, arg second, expected result
static std::tuple<const IGIL_KernelCurbeParams, const IGIL_KernelCurbeParams, bool> Inputs[] = {std::make_tuple(IGIL_KernelCurbeParams{1, 0, 0, 0}, IGIL_KernelCurbeParams{1, 0, 0, 100}, true),
                                                                                                std::make_tuple(IGIL_KernelCurbeParams{1, 0, 0, 100}, IGIL_KernelCurbeParams{1, 0, 0, 0}, false),
                                                                                                std::make_tuple(IGIL_KernelCurbeParams{1, 0, 0, 100}, IGIL_KernelCurbeParams{100, 0, 0, 0}, true),
                                                                                                std::make_tuple(IGIL_KernelCurbeParams{iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE, 0, 4, 100}, IGIL_KernelCurbeParams{iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE, 0, 8, 0}, true),
                                                                                                std::make_tuple(IGIL_KernelCurbeParams{iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE, 0, 4, 0}, IGIL_KernelCurbeParams{iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE, 0, 8, 100}, true),
                                                                                                std::make_tuple(IGIL_KernelCurbeParams{iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE, 0, 8, 100}, IGIL_KernelCurbeParams{iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE, 0, 4, 0}, false),
                                                                                                std::make_tuple(IGIL_KernelCurbeParams{iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE, 0, 8, 0}, IGIL_KernelCurbeParams{iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE, 0, 4, 100}, false)};

INSTANTIATE_TEST_CASE_P(ReflectionSurfaceHelperTest,
                        ReflectionSurfaceHelperTest,
                        ::testing::ValuesIn(Inputs));

struct LocalIDPresent {
    bool x;
    bool y;
    bool z;
    bool flattend;
};

class ReflectionSurfaceHelperFixture : public PlatformFixture, public ::testing::Test {

  protected:
    ReflectionSurfaceHelperFixture() {
    }

    void SetUp() override {
        PlatformFixture::SetUp();
    }

    void TearDown() override {
        PlatformFixture::TearDown();
    }
};

class ReflectionSurfaceHelperSetKernelDataTest : public testing::TestWithParam<std::tuple<LocalIDPresent, uint32_t>>, // LocalIDPresent, private surface size,
                                                 public PlatformFixture {

  protected:
    ReflectionSurfaceHelperSetKernelDataTest() {
    }

    void SetUp() override {
        PlatformFixture::SetUp();

        info.setSamplerTable(3, 1, 5);

        info.setCrossThreadDataSize(crossThreadDataSize);
        info.kernelDescriptor.kernelAttributes.simdSize = 16;
        info.kernelDescriptor.kernelAttributes.barrierCount = 1;

        info.setLocalIds({0, 0, 0});

        info.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = 4;
        info.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = 8;
        info.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = 2;

        info.kernelDescriptor.kernelAttributes.slmInlineSize = 1652;

        IGIL_KernelCurbeParams testParams[3] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
        curbeParams.push_back(testParams[0]);
        curbeParams.push_back(testParams[1]);
        curbeParams.push_back(testParams[2]);
    }

    void TearDown() override {
        PlatformFixture::TearDown();
    }

    MockKernelInfo info;
    const uint16_t crossThreadDataSize = 16;

    std::vector<IGIL_KernelCurbeParams> curbeParams;
};

// arg first, arg second, expected result
static std::tuple<LocalIDPresent, uint32_t> InputsSetKernelData[] = {std::make_tuple(LocalIDPresent{1, 0, 0, 0}, 0),
                                                                     std::make_tuple(LocalIDPresent{0, 1, 0, 0}, 0),
                                                                     std::make_tuple(LocalIDPresent{0, 0, 1, 0}, 32),
                                                                     std::make_tuple(LocalIDPresent{0, 0, 0, 1}, 0),
                                                                     std::make_tuple(LocalIDPresent{0, 0, 0, 0}, 32)};

INSTANTIATE_TEST_CASE_P(ReflectionSurfaceHelperSetKernelDataTest,
                        ReflectionSurfaceHelperSetKernelDataTest,
                        ::testing::ValuesIn(InputsSetKernelData));

TEST_P(ReflectionSurfaceHelperSetKernelDataTest, WhenSettingKernelDataThenDataAndOffsetsAreCorrect) {

    LocalIDPresent localIDPresent;
    uint32_t privateSurfaceSize;

    std::tie(localIDPresent, privateSurfaceSize) = GetParam();

    info.setLocalIds({localIDPresent.x, localIDPresent.y, localIDPresent.z});
    info.kernelDescriptor.kernelAttributes.flags.usesFlattenedLocalIds = localIDPresent.flattend;
    info.setPrivateMemory(privateSurfaceSize, false, 0, 0, 0);

    std::unique_ptr<char> kernelDataMemory(new char[4096]);

    uint64_t tokenMask = 1 | 2 | 4;

    size_t maxConstantBufferSize = 32;
    size_t samplerCount = 1;
    size_t samplerHeapSize = alignUp(info.getSamplerStateArraySize(pPlatform->getClDevice(0)->getHardwareInfo()), Sampler::samplerStateArrayAlignment) + info.getBorderColorStateSize();

    uint32_t offsetInKernelDataMemory = 12;
    uint32_t offset = MockKernel::ReflectionSurfaceHelperPublic::setKernelData(kernelDataMemory.get(), offsetInKernelDataMemory,
                                                                               curbeParams, tokenMask, maxConstantBufferSize, samplerCount,
                                                                               info, pPlatform->getClDevice(0)->getHardwareInfo());

    IGIL_KernelData *kernelData = reinterpret_cast<IGIL_KernelData *>(kernelDataMemory.get() + offsetInKernelDataMemory);

    const auto &samplerTable = info.kernelDescriptor.payloadMappings.samplerTable;
    EXPECT_EQ(3u, kernelData->m_numberOfCurbeParams);
    EXPECT_EQ(3u, kernelData->m_numberOfCurbeTokens);
    EXPECT_EQ(samplerTable.numSamplers, kernelData->m_numberOfSamplerStates);
    EXPECT_EQ(Sampler::samplerStateArrayAlignment + samplerTable.tableOffset - samplerTable.borderColor, kernelData->m_SizeOfSamplerHeap);
    EXPECT_EQ(samplerTable.borderColor, kernelData->m_SamplerBorderColorStateOffsetOnDSH);
    EXPECT_EQ(samplerTable.tableOffset, kernelData->m_SamplerStateArrayOffsetOnDSH);
    EXPECT_EQ(crossThreadDataSize, kernelData->m_sizeOfConstantBuffer);
    EXPECT_EQ(tokenMask, kernelData->m_PatchTokensMask);
    EXPECT_EQ(0u, kernelData->m_ScratchSpacePatchValue);
    EXPECT_EQ(info.kernelDescriptor.kernelAttributes.simdSize, kernelData->m_SIMDSize);
    EXPECT_EQ(info.kernelDescriptor.kernelAttributes.barrierCount, kernelData->m_HasBarriers);
    EXPECT_EQ(info.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0], kernelData->m_RequiredWkgSizes[0]);
    EXPECT_EQ(info.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1], kernelData->m_RequiredWkgSizes[1]);
    EXPECT_EQ(info.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2], kernelData->m_RequiredWkgSizes[2]);
    EXPECT_EQ(info.kernelDescriptor.kernelAttributes.slmInlineSize, kernelData->m_InilineSLMSize);

    if (localIDPresent.flattend || localIDPresent.x || localIDPresent.y || localIDPresent.z)
        EXPECT_EQ(1u, kernelData->m_NeedLocalIDS);
    else
        EXPECT_EQ(0u, kernelData->m_NeedLocalIDS);

    EXPECT_EQ(0u, kernelData->m_DisablePreemption);

    if (privateSurfaceSize == 0)
        EXPECT_EQ(1u, kernelData->m_CanRunConcurently);
    else
        EXPECT_EQ(0u, kernelData->m_CanRunConcurently);

    size_t expectedOffset = offsetInKernelDataMemory;
    expectedOffset += alignUp(sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams) * curbeParams.size(), sizeof(void *));
    expectedOffset += maxConstantBufferSize + alignUp(samplerHeapSize, sizeof(void *)) + samplerCount * sizeof(IGIL_SamplerParams);

    EXPECT_EQ(expectedOffset, offset);
}

TEST_F(ReflectionSurfaceHelperSetKernelDataTest, GivenNullThreadPayloadWhenSettingKernelDataThenDataAndOffsetsAreCorrect) {
    std::unique_ptr<char> kernelDataMemory(new char[4096]);

    std::vector<IGIL_KernelCurbeParams> curbeParams;

    uint64_t tokenMask = 1 | 2 | 4;

    size_t maxConstantBufferSize = 32;
    size_t samplerCount = 1;
    size_t samplerHeapSize = alignUp(info.getSamplerStateArraySize(pPlatform->getClDevice(0)->getHardwareInfo()), Sampler::samplerStateArrayAlignment) + info.getBorderColorStateSize();

    uint32_t offsetInKernelDataMemory = 0;
    uint32_t offset = MockKernel::ReflectionSurfaceHelperPublic::setKernelData(kernelDataMemory.get(), offsetInKernelDataMemory,
                                                                               curbeParams, tokenMask, maxConstantBufferSize, samplerCount,
                                                                               info, pPlatform->getClDevice(0)->getHardwareInfo());

    IGIL_KernelData *kernelData = reinterpret_cast<IGIL_KernelData *>(kernelDataMemory.get() + offsetInKernelDataMemory);

    EXPECT_EQ(0u, kernelData->m_NeedLocalIDS);

    size_t expectedOffset = offsetInKernelDataMemory;
    expectedOffset += alignUp(sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams) * curbeParams.size(), sizeof(void *));
    expectedOffset += maxConstantBufferSize + alignUp(samplerHeapSize, sizeof(void *)) + samplerCount * sizeof(IGIL_SamplerParams);

    EXPECT_EQ(expectedOffset, offset);
}

TEST_F(ReflectionSurfaceHelperSetKernelDataTest, GivenNullPrivateSurfaceWhenSettingKernelDataThenDataAndOffsetsAreCorrect) {
    std::unique_ptr<char> kernelDataMemory(new char[4096]);

    std::vector<IGIL_KernelCurbeParams> curbeParams;

    uint64_t tokenMask = 1 | 2 | 4;

    size_t maxConstantBufferSize = 32;
    size_t samplerCount = 1;
    size_t samplerHeapSize = alignUp(info.getSamplerStateArraySize(pPlatform->getClDevice(0)->getHardwareInfo()), Sampler::samplerStateArrayAlignment) + info.getBorderColorStateSize();

    uint32_t offsetInKernelDataMemory = 0;
    uint32_t offset = MockKernel::ReflectionSurfaceHelperPublic::setKernelData(kernelDataMemory.get(), offsetInKernelDataMemory, curbeParams,
                                                                               tokenMask, maxConstantBufferSize, samplerCount,
                                                                               info, pPlatform->getClDevice(0)->getHardwareInfo());

    IGIL_KernelData *kernelData = reinterpret_cast<IGIL_KernelData *>(kernelDataMemory.get() + offsetInKernelDataMemory);

    EXPECT_EQ(1u, kernelData->m_CanRunConcurently);

    size_t expectedOffset = offsetInKernelDataMemory;
    expectedOffset += alignUp(sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams) * curbeParams.size(), sizeof(void *));
    expectedOffset += maxConstantBufferSize + alignUp(samplerHeapSize, sizeof(void *)) + samplerCount * sizeof(IGIL_SamplerParams);

    EXPECT_EQ(expectedOffset, offset);
}

TEST_F(ReflectionSurfaceHelperSetKernelDataTest, GivenNullSamplerStateWhenSettingKernelDataThenDataAndOffsetsAreCorrect) {
    info.setSamplerTable(0, 0, 0);

    std::unique_ptr<char> kernelDataMemory(new char[4096]);

    std::vector<IGIL_KernelCurbeParams> curbeParams;

    uint64_t tokenMask = 1 | 2 | 4;

    size_t maxConstantBufferSize = 32;
    size_t samplerCount = 1;
    size_t samplerHeapSize = alignUp(info.getSamplerStateArraySize(pPlatform->getClDevice(0)->getHardwareInfo()), Sampler::samplerStateArrayAlignment) + info.getBorderColorStateSize();

    uint32_t offsetInKernelDataMemory = 0;
    uint32_t offset = MockKernel::ReflectionSurfaceHelperPublic::setKernelData(kernelDataMemory.get(), offsetInKernelDataMemory, curbeParams,
                                                                               tokenMask, maxConstantBufferSize, samplerCount,
                                                                               info, pPlatform->getClDevice(0)->getHardwareInfo());

    IGIL_KernelData *kernelData = reinterpret_cast<IGIL_KernelData *>(kernelDataMemory.get() + offsetInKernelDataMemory);

    size_t expectedOffset = offsetInKernelDataMemory;
    expectedOffset += alignUp(sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams) * curbeParams.size(), sizeof(void *));
    expectedOffset += maxConstantBufferSize + alignUp(samplerHeapSize, sizeof(void *)) + samplerCount * sizeof(IGIL_SamplerParams);

    EXPECT_EQ(0u, kernelData->m_numberOfSamplerStates);
    EXPECT_EQ(0u, kernelData->m_SizeOfSamplerHeap);
    EXPECT_EQ(expectedOffset, offset);
}

TEST_F(ReflectionSurfaceHelperSetKernelDataTest, GivenDisabledConcurrentExecutionDebugFlagWhenSettingKernelDataThenCanRunConCurrentlyFlagIsZero) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.DisableConcurrentBlockExecution.set(true);

    std::unique_ptr<char> kernelDataMemory(new char[4096]);

    std::vector<IGIL_KernelCurbeParams> curbeParams;

    uint64_t tokenMask = 0;

    size_t maxConstantBufferSize = 0;
    size_t samplerCount = 0;

    uint32_t offsetInKernelDataMemory = 0;
    MockKernel::ReflectionSurfaceHelperPublic::setKernelData(kernelDataMemory.get(), offsetInKernelDataMemory, curbeParams,
                                                             tokenMask, maxConstantBufferSize, samplerCount,
                                                             info, pPlatform->getClDevice(0)->getHardwareInfo());

    IGIL_KernelData *kernelData = reinterpret_cast<IGIL_KernelData *>(kernelDataMemory.get() + offsetInKernelDataMemory);
    EXPECT_EQ(0u, kernelData->m_CanRunConcurently);
}

TEST_F(ReflectionSurfaceHelperFixture, GivenNullBindingTableWhenSettingKernelDataThenDataIsCorrectlySet) {
    KernelInfo info;

    std::unique_ptr<char> kernelDataMemory(new char[200]);
    IGIL_KernelAddressData *kernalAddressData = reinterpret_cast<IGIL_KernelAddressData *>(kernelDataMemory.get());
    MockKernel::ReflectionSurfaceHelperPublic::setKernelAddressData(kernelDataMemory.get(), 0, 1, 2, 3, 4, 5, 6, info, pPlatform->getClDevice(0)->getHardwareInfo());

    EXPECT_EQ(1u, kernalAddressData->m_KernelDataOffset);
    EXPECT_EQ(2u, kernalAddressData->m_SamplerHeapOffset);
    EXPECT_EQ(4u, kernalAddressData->m_SamplerParamsOffset);
    EXPECT_EQ(3u, kernalAddressData->m_ConstantBufferOffset);
    EXPECT_EQ(5u, kernalAddressData->m_SSHTokensOffset);
    EXPECT_EQ(6u, kernalAddressData->m_BTSoffset);
    EXPECT_EQ(0u, kernalAddressData->m_BTSize);
}

TEST_F(ReflectionSurfaceHelperFixture, GivenSetBindingTableWhenSettingKernelDataThenDataIsCorrectlySet) {
    MockKernelInfo info;
    info.setBindingTable(0, 4);

    std::unique_ptr<char> kernelDataMemory(new char[200]);
    IGIL_KernelAddressData *kernalAddressData = reinterpret_cast<IGIL_KernelAddressData *>(kernelDataMemory.get());
    MockKernel::ReflectionSurfaceHelperPublic::setKernelAddressData(kernelDataMemory.get(), 0, 1, 2, 3, 4, 5, 6, info, pPlatform->getClDevice(0)->getHardwareInfo());

    EXPECT_EQ(1u, kernalAddressData->m_KernelDataOffset);
    EXPECT_EQ(2u, kernalAddressData->m_SamplerHeapOffset);
    EXPECT_EQ(4u, kernalAddressData->m_SamplerParamsOffset);
    EXPECT_EQ(3u, kernalAddressData->m_ConstantBufferOffset);
    EXPECT_EQ(5u, kernalAddressData->m_SSHTokensOffset);
    EXPECT_EQ(6u, kernalAddressData->m_BTSoffset);
    EXPECT_NE(0u, kernalAddressData->m_BTSize);
}

TEST_F(ReflectionSurfaceHelperFixture, WhenPatchingBlocksCurbeThenAddressesAreSetCorrectly) {
    KernelInfo info;

    std::unique_ptr<char> refletionSurfaceMemory(new char[4096]);
    IGIL_KernelDataHeader *header = reinterpret_cast<IGIL_KernelDataHeader *>(refletionSurfaceMemory.get());
    header->m_numberOfKernels = 2;

    IGIL_KernelAddressData *kernalAddressData = header->m_data;

    uint32_t offset = static_cast<uint32_t>(alignUp(sizeof(IGIL_KernelDataHeader) + 2 * sizeof(IGIL_KernelAddressData) + 2 * sizeof(IGIL_KernelData), MemoryConstants::cacheLineSize));

    for (uint32_t i = 0; i < 2; i++) {
        assert(offset < 4000);
        kernalAddressData[i].m_ConstantBufferOffset = offset;
        offset += 128;
    }

    uint64_t defaultQueueOffset = 8;
    uint64_t deviceQueueOffset = 16;
    uint64_t eventPoolOffset = 24;
    uint64_t printfOffset = 32;
    uint64_t privateSurfaceOffset = 40;

    uint64_t deviceQueueAddress = 0x12345678;
    uint64_t eventPoolAddress = 0x87654321;
    uint64_t printfAddress = 0x55667788;
    uint64_t privateSurfaceAddress = 0x19283746;

    for (uint32_t i = 0; i < 2; i++) {
        MockKernel::ReflectionSurfaceHelperPublic::patchBlocksCurbe(refletionSurfaceMemory.get(), i,
                                                                    defaultQueueOffset, sizeof(uint64_t), deviceQueueAddress,
                                                                    eventPoolOffset, sizeof(uint64_t), eventPoolAddress,
                                                                    deviceQueueOffset, sizeof(uint64_t), deviceQueueAddress,
                                                                    printfOffset, sizeof(uint64_t), printfAddress,
                                                                    privateSurfaceOffset, sizeof(uint64_t), privateSurfaceAddress);

        void *pCurbe = ptrOffset(refletionSurfaceMemory.get(), (size_t)(kernalAddressData[i].m_ConstantBufferOffset));

        EXPECT_EQ(deviceQueueAddress, *static_cast<uint64_t *>(ptrOffset(pCurbe, (size_t)defaultQueueOffset)));
        EXPECT_EQ(eventPoolAddress, *static_cast<uint64_t *>(ptrOffset(pCurbe, (size_t)eventPoolOffset)));
        EXPECT_EQ(deviceQueueAddress, *static_cast<uint64_t *>(ptrOffset(pCurbe, (size_t)deviceQueueOffset)));
        EXPECT_EQ(printfAddress, *static_cast<uint64_t *>(ptrOffset(pCurbe, (size_t)printfOffset)));
        EXPECT_EQ(privateSurfaceAddress, *static_cast<uint64_t *>(ptrOffset(pCurbe, (size_t)privateSurfaceOffset)));
    }
}

TEST_F(ReflectionSurfaceHelperFixture, GivenUndefinedOffsetsWhenPatchingBlocksCurbeThenAddressesAreSetCorrectly) {
    KernelInfo info;

    std::unique_ptr<char> refletionSurfaceMemory(new char[4096]);
    IGIL_KernelDataHeader *header = reinterpret_cast<IGIL_KernelDataHeader *>(refletionSurfaceMemory.get());
    header->m_numberOfKernels = 2;

    IGIL_KernelAddressData *kernalAddressData = header->m_data;

    uint32_t offset = sizeof(IGIL_KernelDataHeader) + 2 * sizeof(IGIL_KernelAddressData) + 2 * sizeof(IGIL_KernelData);

    uint8_t pattern[100] = {0};
    memset(pattern, 0, 100);
    memset(ptrOffset(refletionSurfaceMemory.get(), offset), 0, 200);

    for (uint32_t i = 0; i < 2; i++) {
        assert(offset < 4000);
        kernalAddressData[i].m_ConstantBufferOffset = offset;
        offset += 100;
    }

    uint64_t defaultQueueOffset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;
    uint64_t deviceQueueOffset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;
    uint64_t eventPoolOffset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;
    uint64_t printfOffset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;
    uint64_t privateSurfaceOffset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;

    uint64_t deviceQueueAddress = 0x12345678;
    uint64_t eventPoolAddress = 0x87654321;
    uint64_t printfAddress = 0x55667788;
    uint64_t privateSurfaceGpuAddress = 0x19283746;

    uint32_t privateSurfaceSize = 128;

    for (uint32_t i = 0; i < 2; i++) {
        MockKernel::ReflectionSurfaceHelperPublic::patchBlocksCurbe(refletionSurfaceMemory.get(), i,
                                                                    defaultQueueOffset, sizeof(uint64_t), deviceQueueAddress,
                                                                    eventPoolOffset, sizeof(uint64_t), eventPoolAddress,
                                                                    deviceQueueOffset, sizeof(uint64_t), deviceQueueAddress,
                                                                    printfOffset, sizeof(uint64_t), printfAddress,
                                                                    privateSurfaceOffset, privateSurfaceSize, privateSurfaceGpuAddress);

        void *pCurbe = ptrOffset(refletionSurfaceMemory.get(), (size_t)(kernalAddressData[i].m_ConstantBufferOffset));
        // constant buffer should be intact
        EXPECT_EQ(0, memcmp(pattern, pCurbe, 100));
    }
}

TEST_F(ReflectionSurfaceHelperFixture, WhenSettingParentImageParamsThenParamsAreSetCorrectly) {
    MockContext context;
    MockKernelInfo info;
    std::vector<Kernel::SimpleKernelArgInfo> kernelArguments;

    std::unique_ptr<Image> image2d(ImageHelper<Image2dDefaults>::create(&context));
    std::unique_ptr<Image> image1d(ImageHelper<Image1dDefaults>::create(&context));

    Kernel::SimpleKernelArgInfo imgInfo;

    uint32_t imageID[4] = {32, 64, 0, 0};

    // Buffer Object should never be dereferenced by setParentImageParams
    imgInfo.type = Kernel::kernelArgType::BUFFER_OBJ;
    imgInfo.object = reinterpret_cast<void *>(0x0);
    kernelArguments.push_back(imgInfo);
    info.addArgBuffer(0, undefined<CrossThreadDataOffset>, 0, 0);

    imgInfo.type = Kernel::kernelArgType::IMAGE_OBJ;
    imgInfo.object = (cl_mem)image2d.get();
    kernelArguments.push_back(imgInfo);
    info.addArgImage(1, imageID[0]);

    // Buffer Object should never be dereferenced by setParentImageParams
    imgInfo.type = Kernel::kernelArgType::BUFFER_OBJ;
    imgInfo.object = reinterpret_cast<void *>(0x0);
    kernelArguments.push_back(imgInfo);
    info.addArgBuffer(2, undefined<CrossThreadDataOffset>, 0, 0);

    imgInfo.type = Kernel::kernelArgType::IMAGE_OBJ;
    imgInfo.object = (cl_mem)image1d.get();
    kernelArguments.push_back(imgInfo);
    info.addArgImage(3, imageID[1]);

    std::unique_ptr<char> reflectionSurfaceMemory(new char[4096]);

    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurfaceMemory.get());

    pKernelHeader->m_ParentImageDataOffset = 16;
    pKernelHeader->m_ParentKernelImageCount = 2;

    IGIL_ImageParamters *pImageParameters = reinterpret_cast<IGIL_ImageParamters *>(ptrOffset(pKernelHeader, (size_t)pKernelHeader->m_ParentImageDataOffset));

    MockKernel::ReflectionSurfaceHelperPublic::setParentImageParams(reflectionSurfaceMemory.get(), kernelArguments, info);

    auto numArgs = kernelArguments.size();
    uint32_t imageIndex = 0;

    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].type == Kernel::kernelArgType::IMAGE_OBJ) {
            const Image *image = const_cast<const Image *>(castToObject<Image>((cl_mem)kernelArguments[argIndex].object));
            EXPECT_EQ(image->getImageDesc().image_array_size, pImageParameters->m_ArraySize);
            EXPECT_EQ(image->getImageDesc().image_depth, pImageParameters->m_Depth);
            EXPECT_EQ(image->getImageDesc().image_height, pImageParameters->m_Height);
            EXPECT_EQ(image->getImageDesc().image_width, pImageParameters->m_Width);
            EXPECT_EQ(image->getImageDesc().num_mip_levels, pImageParameters->m_NumMipLevels);
            EXPECT_EQ(image->getImageDesc().num_samples, pImageParameters->m_NumSamples);

            EXPECT_EQ(image->getImageFormat().image_channel_data_type, pImageParameters->m_ChannelDataType);
            EXPECT_EQ(image->getImageFormat().image_channel_data_type, pImageParameters->m_ChannelOrder);
            EXPECT_EQ(imageID[imageIndex], pImageParameters->m_ObjectID);
            pImageParameters++;
            imageIndex++;
        }
    }
}

TEST_F(ReflectionSurfaceHelperFixture, WhenSettingParentSamplerParamsThenParamsAreSetCorrectly) {
    MockContext context;
    MockKernelInfo info;
    std::vector<Kernel::SimpleKernelArgInfo> kernelArguments;

    std::unique_ptr<MockSampler> sampler1(new MockSampler(&context,
                                                          true,
                                                          (cl_addressing_mode)CL_ADDRESS_CLAMP_TO_EDGE,
                                                          (cl_filter_mode)CL_FILTER_LINEAR));

    std::unique_ptr<MockSampler> sampler2(new MockSampler(&context,
                                                          false,
                                                          (cl_addressing_mode)CL_ADDRESS_CLAMP,
                                                          (cl_filter_mode)CL_FILTER_NEAREST));

    Kernel::SimpleKernelArgInfo imgInfo;

    uint32_t samplerID[4] = {32, 64, 0, 0};

    // Buffer Object should never be dereferenced by setParentImageParams
    imgInfo.type = Kernel::kernelArgType::BUFFER_OBJ;
    imgInfo.object = reinterpret_cast<void *>(0x0);
    kernelArguments.push_back(std::move(imgInfo));
    info.addArgBuffer(0, undefined<CrossThreadDataOffset>, 0, 0);

    imgInfo = {};
    imgInfo.type = Kernel::kernelArgType::SAMPLER_OBJ;
    imgInfo.object = (cl_sampler)sampler1.get();
    kernelArguments.push_back(std::move(imgInfo));
    info.addArgSampler(1, samplerID[0]);

    // Buffer Object should never be dereferenced by setParentImageParams
    imgInfo = {};
    imgInfo.type = Kernel::kernelArgType::BUFFER_OBJ;
    imgInfo.object = reinterpret_cast<void *>(0x0);
    kernelArguments.push_back(std::move(imgInfo));
    info.addArgBuffer(2, undefined<CrossThreadDataOffset>, 0, 0);

    imgInfo = {};
    imgInfo.type = Kernel::kernelArgType::SAMPLER_OBJ;
    imgInfo.object = (cl_sampler)sampler2.get();
    kernelArguments.push_back(std::move(imgInfo));
    info.addArgSampler(3, samplerID[1]);

    std::unique_ptr<char> reflectionSurfaceMemory(new char[4096]);

    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurfaceMemory.get());

    pKernelHeader->m_ParentSamplerParamsOffset = 16;
    pKernelHeader->m_ParentSamplerCount = 2;

    IGIL_ParentSamplerParams *pParentSamplerParams = reinterpret_cast<IGIL_ParentSamplerParams *>(ptrOffset(pKernelHeader, (size_t)pKernelHeader->m_ParentSamplerParamsOffset));

    MockKernel::ReflectionSurfaceHelperPublic::setParentSamplerParams(reflectionSurfaceMemory.get(), kernelArguments, info);

    auto numArgs = kernelArguments.size();
    uint32_t samplerIndex = 0;

    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].type == Kernel::kernelArgType::SAMPLER_OBJ) {
            MockSampler *sampler = reinterpret_cast<MockSampler *>(castToObject<Sampler>((cl_sampler)kernelArguments[argIndex].object));
            EXPECT_EQ((uint32_t)sampler->getAddressingMode(), pParentSamplerParams->m_AddressingMode);
            EXPECT_EQ((uint32_t)sampler->getNormalizedCoordinates(), pParentSamplerParams->NormalizedCoords);
            EXPECT_EQ(sampler->getSnapWaValue(), pParentSamplerParams->CoordinateSnapRequired);

            EXPECT_EQ(OCLRT_ARG_OFFSET_TO_SAMPLER_OBJECT_ID(samplerID[samplerIndex]), pParentSamplerParams->m_ObjectID);
            pParentSamplerParams++;
            samplerIndex++;
        }
    }
}

TEST_F(ReflectionSurfaceHelperFixture, GivenDefinedOffsetsWhenPatchingBlocksCurbeWithConstantValuesThenCurbeOnReflectionSurfaceIsPatched) {

    IGIL_KernelDataHeader reflectionSurface[10];
    IGIL_KernelDataHeader referenceHeader = {0};

    memset(reflectionSurface, 0, sizeof(IGIL_KernelDataHeader) * 10);

    const uint32_t constBufferOffset = (uint32_t)alignUp(sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData) + sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams), sizeof(uint64_t));
    reflectionSurface[0].m_numberOfKernels = 1;
    reflectionSurface[0].m_data[0].m_ConstantBufferOffset = constBufferOffset;
    reflectionSurface[0].m_data[0].m_KernelDataOffset = sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData);

    referenceHeader = reflectionSurface[0];

    uint64_t inititalOffset = 8;
    uint64_t offset = inititalOffset;

    uint64_t globalMemoryCurbeOffset = offset;
    uint32_t globalMemoryPatchSize = 8;
    uint64_t globalMemoryGpuAddress = 0x12345678;

    offset += 8;

    uint64_t constantMemoryCurbeOffset = offset;
    uint32_t constantMemoryPatchSize = 8;
    uint64_t constantMemoryGpuAddress = 0x87654321;

    offset += 8;

    uint64_t privateMemoryCurbeOffset = offset;
    uint32_t privateMemoryPatchSize = 8;
    uint64_t privateMemoryGpuAddress = 0x22446688;

    MockKernel::ReflectionSurfaceHelperPublic::patchBlocksCurbeWithConstantValues((void *)reflectionSurface, 0,
                                                                                  globalMemoryCurbeOffset, globalMemoryPatchSize, globalMemoryGpuAddress,
                                                                                  constantMemoryCurbeOffset, constantMemoryPatchSize, constantMemoryGpuAddress,
                                                                                  privateMemoryCurbeOffset, privateMemoryPatchSize, privateMemoryGpuAddress);

    uint64_t *patchedValues = reinterpret_cast<uint64_t *>(reinterpret_cast<char *>(reflectionSurface) + constBufferOffset + inititalOffset);

    EXPECT_EQ(patchedValues[0], globalMemoryGpuAddress);
    EXPECT_EQ(patchedValues[1], constantMemoryGpuAddress);
    EXPECT_EQ(patchedValues[2], privateMemoryGpuAddress);

    EXPECT_THAT(&referenceHeader, MemCompare(&reflectionSurface[0], sizeof(IGIL_KernelDataHeader)));

    IGIL_KernelData *kernelData = (IGIL_KernelData *)ptrOffset((char *)&reflectionSurface[0], sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData));
    IGIL_KernelData referenceKerneldData = {0};
    EXPECT_THAT(&referenceKerneldData, MemCompare(kernelData, sizeof(IGIL_KernelData)));
}

TEST_F(ReflectionSurfaceHelperFixture, GivenUndefinedOffsetsWhenPatchingBlocksCurbeWithConstantValuesThenCurbeOnReflectionSurfaceIsNotPatched) {

    IGIL_KernelDataHeader reflectionSurface[10];
    IGIL_KernelDataHeader referenceHeader = {0};

    memset(reflectionSurface, 0, sizeof(IGIL_KernelDataHeader) * 10);

    const uint32_t constBufferOffset = (uint32_t)alignUp(sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData) + sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams), sizeof(uint64_t));
    reflectionSurface[0].m_numberOfKernels = 1;
    reflectionSurface[0].m_data[0].m_ConstantBufferOffset = constBufferOffset;
    reflectionSurface[0].m_data[0].m_KernelDataOffset = sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData);

    referenceHeader = reflectionSurface[0];

    uint64_t offset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;

    uint64_t globalMemoryCurbeOffset = offset;
    uint32_t globalMemoryPatchSize = 8;
    uint64_t globalMemoryGpuAddress = 0x12345678;

    uint64_t constantMemoryCurbeOffset = offset;
    uint32_t constantMemoryPatchSize = 8;
    uint64_t constantMemoryGpuAddress = 0x87654321;

    uint64_t privateMemoryCurbeOffset = offset;
    uint32_t privateMemoryPatchSize = 8;
    uint64_t privateMemoryGpuAddress = 0x22446688;

    MockKernel::ReflectionSurfaceHelperPublic::patchBlocksCurbeWithConstantValues((void *)reflectionSurface, 0,
                                                                                  globalMemoryCurbeOffset, globalMemoryPatchSize, globalMemoryGpuAddress,
                                                                                  constantMemoryCurbeOffset, constantMemoryPatchSize, constantMemoryGpuAddress,
                                                                                  privateMemoryCurbeOffset, privateMemoryPatchSize, privateMemoryGpuAddress);

    uint64_t *patchedValues = reinterpret_cast<uint64_t *>(reinterpret_cast<char *>(reflectionSurface) + constBufferOffset);

    std::unique_ptr<char> reference = std::unique_ptr<char>(new char[10 * sizeof(IGIL_KernelDataHeader)]);
    memset(reference.get(), 0, 10 * sizeof(IGIL_KernelDataHeader));

    EXPECT_THAT(patchedValues, MemCompare(reference.get(), 10 * sizeof(IGIL_KernelDataHeader) - constBufferOffset));
}
