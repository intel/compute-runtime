/*
 * Copyright (C) 2018-2021 Intel Corporation
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
#include "opencl/test/unit_test/fixtures/execution_model_fixture.h"
#include "opencl/test/unit_test/fixtures/execution_model_kernel_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_sampler.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "patch_list.h"

#include <algorithm>
#include <memory>

using namespace NEO;

struct KernelReflectionSurfaceTest : public ExecutionModelKernelFixture,
                                     public ::testing::WithParamInterface<std::tuple<const char *, const char *>> {

    void SetUp() override {

        ExecutionModelKernelFixture::SetUp(std::get<0>(GetParam()), std::get<1>(GetParam()));
    }
};
typedef ExecutionModelKernelTest KernelReflectionSurfaceWithQueueTest;

TEST_P(KernelReflectionSurfaceTest, WhenCreatingKernelThenKernelReflectionSurfaceIsNull) {
    EXPECT_EQ(nullptr, pKernel->getKernelReflectionSurface());
}

TEST_P(KernelReflectionSurfaceTest, GivenEmptyKernelInfoWhenPassedToGetCurbeParamsThenEmptyVectorIsReturned) {
    MockKernelInfo info;

    info.addArgImage(0, 32);
    info.addArgSampler(1, 32);
    info.addArgImmediate(2, 4, 32);

    std::vector<IGIL_KernelCurbeParams> curbeParamsForBlock;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParamsForBlock, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    // 3 params with Binding Table index of type 1024
    EXPECT_EQ(3u, curbeParamsForBlock.size());
    for (const auto &curbeParam : curbeParamsForBlock) {
        EXPECT_EQ(1024u, curbeParam.m_parameterType);
    }
    EXPECT_EQ(0u, firstSSHTokenIndex);
}

TEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithCorrectlyFilledImageArgumentWhenPassedToGetCurbeParamsThenImageCurbeParamsAreReturned) {
    MockKernelInfo info;

    const uint32_t offsetDataType = 4;
    const uint32_t offsetChannelOrder = 8;
    const uint32_t offsetHeap = 12;
    const uint32_t offsetDepth = 16;
    const uint32_t offsetWidth = 20;
    const uint32_t offsetHeight = 24;
    const uint32_t offsetObjectID = 28;
    const uint32_t offsetArraySize = 32;

    info.addArgImage(0, offsetHeap);
    auto &metaPayload = info.argAsImg(0).metadataPayload;
    metaPayload.channelDataType = offsetDataType;
    metaPayload.channelOrder = offsetChannelOrder;
    metaPayload.imgDepth = offsetDepth;
    metaPayload.imgWidth = offsetWidth;
    metaPayload.imgHeight = offsetHeight;
    metaPayload.arraySize = offsetArraySize;

    info.addExtendedDeviceSideEnqueueDescriptor(0, offsetObjectID);
    info.setAccessQualifier(0, KernelArgMetadata::AccessReadOnly);
    info.addExtendedMetadata(0, "img", "", "read_only");

    info.kernelDescriptor.kernelAttributes.gpuPointerSize = 8;

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    std::vector<uint32_t> supportedImageParamTypes = {iOpenCL::DATA_PARAMETER_IMAGE_WIDTH,
                                                      iOpenCL::DATA_PARAMETER_IMAGE_HEIGHT,
                                                      iOpenCL::DATA_PARAMETER_IMAGE_DEPTH,
                                                      iOpenCL::DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE,
                                                      iOpenCL::DATA_PARAMETER_IMAGE_CHANNEL_ORDER,
                                                      iOpenCL::DATA_PARAMETER_IMAGE_ARRAY_SIZE,
                                                      iOpenCL::DATA_PARAMETER_OBJECT_ID,
                                                      1024}; // type for Binding Table Index

    std::sort(supportedImageParamTypes.begin(), supportedImageParamTypes.end());

    size_t ParamCount = supportedImageParamTypes.size();

    EXPECT_EQ(ParamCount, curbeParams.size());

    for (size_t i = 0; i < std::min(ParamCount, curbeParams.size()); i++) {
        if (i < ParamCount - 1) {
            EXPECT_EQ(supportedImageParamTypes[i] + 50, curbeParams[i].m_parameterType);
            EXPECT_EQ(sizeof(uint32_t), curbeParams[i].m_parameterSize);
        } else {
            EXPECT_EQ(1024u, curbeParams[i].m_parameterType);
            EXPECT_EQ(8u, curbeParams[i].m_parameterSize);
        }

        switch (curbeParams[i].m_parameterType - 50) {
        case iOpenCL::DATA_PARAMETER_IMAGE_WIDTH:
            EXPECT_EQ(offsetWidth, curbeParams[i].m_patchOffset);
            break;
        case iOpenCL::DATA_PARAMETER_IMAGE_HEIGHT:
            EXPECT_EQ(offsetHeight, curbeParams[i].m_patchOffset);
            break;
        case iOpenCL::DATA_PARAMETER_IMAGE_DEPTH:
            EXPECT_EQ(offsetDepth, curbeParams[i].m_patchOffset);
            break;
        case iOpenCL::DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE:
            EXPECT_EQ(offsetDataType, curbeParams[i].m_patchOffset);
            break;
        case iOpenCL::DATA_PARAMETER_IMAGE_CHANNEL_ORDER:
            EXPECT_EQ(offsetChannelOrder, curbeParams[i].m_patchOffset);
            break;
        case iOpenCL::DATA_PARAMETER_IMAGE_ARRAY_SIZE:
            EXPECT_EQ(offsetArraySize, curbeParams[i].m_patchOffset);
            break;
        case iOpenCL::DATA_PARAMETER_OBJECT_ID:
            EXPECT_EQ(offsetObjectID, curbeParams[i].m_patchOffset);
            break;
        }
    }
    EXPECT_EQ(curbeParams.size() - 1, firstSSHTokenIndex);
}

HWTEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithSetBindingTableStateAndImageArgumentWhenPassedToGetCurbeParamsThenProperCurbeParamIsReturned) {
    typedef typename FamilyType::BINDING_TABLE_STATE BINDING_TABLE_STATE;

    MockKernelInfo info;
    uint32_t imageOffset = 32;
    uint32_t btIndex = 3;

    info.kernelDescriptor.kernelAttributes.gpuPointerSize = 8;

    info.addArgImage(0, imageOffset, iOpenCL::IMAGE_MEMORY_OBJECT_2D);

    info.setBindingTable(0, 4);

    BINDING_TABLE_STATE bindingTableState[4];

    memset(&bindingTableState, 0, 4 * sizeof(BINDING_TABLE_STATE));

    bindingTableState[btIndex].getRawData(0) = imageOffset;

    info.heapInfo.pSsh = reinterpret_cast<void *>(bindingTableState);

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    EXPECT_NE(0u, curbeParams.size());
    bool foundProperParam = false;

    for (const auto &curbeParam : curbeParams) {
        if (curbeParam.m_parameterType == 1024u) {
            EXPECT_EQ(btIndex, curbeParam.m_patchOffset);
            EXPECT_EQ(8u, curbeParam.m_parameterSize);
            EXPECT_EQ(0u, curbeParam.m_sourceOffset);
            foundProperParam = true;
            break;
        }
    }
    EXPECT_TRUE(foundProperParam);
}

HWTEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithBindingTableStateAndImageArgumentWhenCountIsZeroThenGetCurbeParamsReturnsMaxBTIndex) {
    typedef typename FamilyType::BINDING_TABLE_STATE BINDING_TABLE_STATE;

    MockKernelInfo info;
    uint32_t imageOffset = 32;
    uint32_t btIndex = 0;
    uint32_t maxBTIndex = 253;

    info.kernelDescriptor.kernelAttributes.gpuPointerSize = 8;

    info.addArgImage(0, imageOffset, iOpenCL::IMAGE_MEMORY_OBJECT_2D);

    info.setBindingTable(0, 0);

    BINDING_TABLE_STATE bindingTableState[1];

    memset(&bindingTableState, 0, 1 * sizeof(BINDING_TABLE_STATE));

    bindingTableState[btIndex].getRawData(0) = imageOffset;

    info.heapInfo.pSsh = reinterpret_cast<void *>(bindingTableState);

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    EXPECT_EQ(1u, curbeParams.size());
    bool foundProperParam = false;

    for (const auto &curbeParam : curbeParams) {
        if (curbeParam.m_parameterType == 1024u) {
            EXPECT_EQ(maxBTIndex, curbeParam.m_patchOffset);
            EXPECT_EQ(8u, curbeParam.m_parameterSize);
            EXPECT_EQ(0u, curbeParam.m_sourceOffset);
            foundProperParam = true;
            break;
        }
    }
    EXPECT_TRUE(foundProperParam);
}

TEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithCorrectlyFilledSamplerArgumentWhenPassedToGetCurbeParamsThenSamplerCurbeParamsAreReturned) {

    MockKernelInfo info;
    SPatchSamplerKernelArgument samplerMemObjKernelArg;
    samplerMemObjKernelArg.ArgumentNumber = 1;
    samplerMemObjKernelArg.Offset = 32;
    samplerMemObjKernelArg.Size = 4;
    samplerMemObjKernelArg.Type = iOpenCL::SAMPLER_OBJECT_TEXTURE;

    const uint32_t offsetSamplerAddressingMode = 4;
    const uint32_t offsetSamplerNormalizedCoords = 8;
    const uint32_t offsetSamplerSnapWa = 12;
    const uint32_t offsetObjectID = 28;

    info.addArgSampler(1, 32, offsetSamplerAddressingMode, offsetSamplerNormalizedCoords, offsetSamplerSnapWa);
    info.addExtendedMetadata(1, "smp");
    info.addExtendedDeviceSideEnqueueDescriptor(1, offsetObjectID);

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    std::vector<uint32_t> supportedSamplerParamTypes = {iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE,
                                                        iOpenCL::DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS,
                                                        iOpenCL::DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED,
                                                        iOpenCL::DATA_PARAMETER_OBJECT_ID};

    std::sort(supportedSamplerParamTypes.begin(), supportedSamplerParamTypes.end());

    size_t ParamCount = supportedSamplerParamTypes.size();

    EXPECT_EQ(ParamCount + 2, curbeParams.size()); // + 2 for 2 arguments' Binding Table Index params stored

    for (size_t i = 0; i < std::min(ParamCount, curbeParams.size()); i++) {
        EXPECT_EQ(supportedSamplerParamTypes[i] + 100, curbeParams[i].m_parameterType);

        EXPECT_EQ(sizeof(uint32_t), curbeParams[i].m_parameterSize);

        switch (curbeParams[i].m_parameterType - 100) {
        case iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE:
            EXPECT_EQ(offsetSamplerAddressingMode, curbeParams[i].m_patchOffset);
            break;
        case iOpenCL::DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS:
            EXPECT_EQ(offsetSamplerNormalizedCoords, curbeParams[i].m_patchOffset);
            break;
        case iOpenCL::DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED:
            EXPECT_EQ(offsetSamplerSnapWa, curbeParams[i].m_patchOffset);
            break;
        case iOpenCL::DATA_PARAMETER_OBJECT_ID:
            EXPECT_EQ(offsetObjectID, curbeParams[i].m_patchOffset);
            break;
        }
    }
}

TEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithBufferAndDataParameterBuffersTokensWhenPassedToGetCurbeParamsThenCorrectCurbeParamsWithProperSizesAreReturned) {

    MockKernelInfo info;
    info.addArgImmediate(0, 8, 40, 0, true);

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    EXPECT_LT(1u, curbeParams.size());
    bool kernelArgumentTokenFound = false;
    bool kernelArgumentSSHParamFound = false;
    for (const auto &curbeParam : curbeParams) {

        if (iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT == curbeParam.m_parameterType) {
            kernelArgumentTokenFound = true;
            EXPECT_EQ(0u, curbeParam.m_sourceOffset);
            EXPECT_EQ(8u, curbeParam.m_parameterSize);
            EXPECT_EQ(40u, curbeParam.m_patchOffset);
        }
        // kernel arg SSH param
        if (1024 == curbeParam.m_parameterType) {
            kernelArgumentSSHParamFound = true;
            EXPECT_EQ(0u, curbeParam.m_sourceOffset);
            EXPECT_EQ(0u, curbeParam.m_parameterSize);
            EXPECT_EQ(0u, curbeParam.m_patchOffset);
        }
    }
    EXPECT_TRUE(kernelArgumentTokenFound);
    EXPECT_TRUE((tokenMask & ((uint64_t)1 << iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT)) > 0);

    EXPECT_TRUE(kernelArgumentSSHParamFound);
}

TEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithBufferAndNoDataParameterBuffersTokenWhenPassedToGetCurbeParamsThenCurbeParamForDataKernelArgumentTokenIsNotReturned) {
    MockKernelInfo info;
    info.addArgImmediate(0, 8, 40);

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    bool kernelArgumentTokenFound = false;
    for (const auto &curbeParam : curbeParams) {
        if (iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT == curbeParam.m_parameterType) {
            kernelArgumentTokenFound = true;
        }
    }
    EXPECT_FALSE(kernelArgumentTokenFound);
    EXPECT_TRUE((tokenMask & ((uint64_t)1 << iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT)) == 0);
}

TEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithLocalMemoryParameterWhenPassedToGetCurbeParamsThenCurbeParamForLocalMemoryArgIsReturned) {
    MockKernelInfo info;

    const uint32_t crossThreadOffset = 10;
    const uint32_t slmAlignment = 80;

    info.addArgLocal(0, crossThreadOffset, slmAlignment);

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    bool localMemoryTokenFound = false;
    for (const auto &curbeParam : curbeParams) {
        if (iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES == curbeParam.m_parameterType) {
            localMemoryTokenFound = true;
            EXPECT_EQ(slmAlignment, curbeParam.m_sourceOffset);
            EXPECT_EQ(0u, curbeParam.m_parameterSize);
            EXPECT_EQ(crossThreadOffset, curbeParam.m_patchOffset);
        }
    }
    EXPECT_TRUE(localMemoryTokenFound);
    EXPECT_TRUE((tokenMask & ((uint64_t)1 << iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES)) > 0);
}

TEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithoutLocalMemoryParameterWhenPassedToGetCurbeParamsThenCurbeParamForLocalMemoryArgIsNotReturned) {

    MockKernelInfo info;

    const uint32_t crossThreadOffset = 10;
    const uint32_t slmAlignment = 0;

    info.addArgLocal(0, crossThreadOffset, slmAlignment);

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    bool localMemoryTokenFound = false;
    for (const auto &curbeParam : curbeParams) {
        if (iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES == curbeParam.m_parameterType) {
            localMemoryTokenFound = true;
        }
    }
    EXPECT_FALSE(localMemoryTokenFound);
    EXPECT_TRUE((tokenMask & ((uint64_t)1 << iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES)) == 0);
}

TEST_P(KernelReflectionSurfaceTest, WhenGettingCurbeParamsThenReturnedVectorIsSortedIncreasing) {
    EXPECT_TRUE(pKernel->isParentKernel);

    BlockKernelManager *blockManager = pProgram->getBlockKernelManager();
    size_t blockCount = blockManager->getCount();

    EXPECT_NE(0u, blockCount);

    std::vector<IGIL_KernelCurbeParams> curbeParamsForBlock;

    for (size_t i = 0; i < blockCount; i++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
        uint64_t tokenMask = 0;
        uint32_t firstSSHTokenIndex = 0;
        MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParamsForBlock, tokenMask, firstSSHTokenIndex, *pBlockInfo, pDevice->getHardwareInfo());

        if (pBlockInfo->kernelDescriptor.kernelMetadata.kernelName.find("simple_block_kernel") == std::string::npos) {
            EXPECT_LT(1u, curbeParamsForBlock.size());
        }

        for (size_t i = 1; i < curbeParamsForBlock.size(); i++) {
            EXPECT_LE(curbeParamsForBlock[i - 1].m_parameterType, curbeParamsForBlock[i].m_parameterType);
            if (curbeParamsForBlock[i - 1].m_parameterType == curbeParamsForBlock[i].m_parameterType) {

                if (curbeParamsForBlock[i - 1].m_parameterType == iOpenCL::DATA_PARAMETER_TOKEN::DATA_PARAMETER_LOCAL_WORK_SIZE) {
                    EXPECT_LE(curbeParamsForBlock[i - 1].m_patchOffset, curbeParamsForBlock[i].m_patchOffset);
                } else {
                    EXPECT_LE(curbeParamsForBlock[i - 1].m_sourceOffset, curbeParamsForBlock[i].m_sourceOffset);
                }
            }
        }
        EXPECT_EQ(curbeParamsForBlock.size() - pBlockInfo->getExplicitArgs().size(), firstSSHTokenIndex);
        curbeParamsForBlock.resize(0);
    }
}

TEST_P(KernelReflectionSurfaceTest, WhenGettingCurbeParamsThenReturnedVectorHasExpectedParamTypes) {
    EXPECT_TRUE(pKernel->isParentKernel);

    BlockKernelManager *blockManager = pProgram->getBlockKernelManager();
    size_t blockCount = blockManager->getCount();

    EXPECT_NE(0u, blockCount);

    std::vector<IGIL_KernelCurbeParams> curbeParamsForBlock;

    for (size_t i = 0; i < blockCount; i++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
        uint64_t tokenMask = 0;
        uint32_t firstSSHTokenIndex = 0;
        MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParamsForBlock, tokenMask, firstSSHTokenIndex, *pBlockInfo, pDevice->getHardwareInfo());

        const uint32_t bufferType = 49;
        const uint32_t imageType = iOpenCL::DATA_PARAMETER_TOKEN::DATA_PARAMETER_OBJECT_ID + 50;
        const uint32_t samplerType = iOpenCL::DATA_PARAMETER_TOKEN::DATA_PARAMETER_OBJECT_ID + 100;

        bool bufferFound = false;
        bool imageFound = false;
        bool samplerFound = false;

        if (pBlockInfo->kernelDescriptor.kernelMetadata.kernelName.find("kernel_reflection_dispatch_0") != std::string::npos) {
            EXPECT_LT(1u, curbeParamsForBlock.size());

            for (const auto &curbeParams : curbeParamsForBlock) {

                switch (curbeParams.m_parameterType) {
                case bufferType:
                    bufferFound = true;
                    break;
                case imageType:
                    imageFound = true;
                    break;
                case samplerType:
                    samplerFound = true;
                    break;
                }
            }

            EXPECT_TRUE(bufferFound);
            EXPECT_TRUE(imageFound);
            EXPECT_TRUE(samplerFound);
        }
        EXPECT_EQ(curbeParamsForBlock.size() - pBlockInfo->getExplicitArgs().size(), firstSSHTokenIndex);
        curbeParamsForBlock.resize(0);
    }
}

TEST_P(KernelReflectionSurfaceTest, WhenGettingCurbeParamsThenTokenMaskIsCorrect) {
    EXPECT_TRUE(pKernel->isParentKernel);

    BlockKernelManager *blockManager = pProgram->getBlockKernelManager();
    size_t blockCount = blockManager->getCount();

    EXPECT_NE(0u, blockCount);

    std::vector<IGIL_KernelCurbeParams> curbeParamsForBlock;

    for (size_t i = 0; i < blockCount; i++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);

        uint64_t tokenMask = 0;
        uint32_t firstSSHTokenIndex = 0;
        MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParamsForBlock, tokenMask, firstSSHTokenIndex, *pBlockInfo, pDevice->getHardwareInfo());

        if (pBlockInfo->kernelDescriptor.kernelMetadata.kernelName.find("kernel_reflection_dispatch_0") != std::string::npos) {
            EXPECT_LT(1u, curbeParamsForBlock.size());

            const uint64_t bufferToken = (uint64_t)1 << 63;
            const uint64_t imageToken = (uint64_t)1 << 50;
            const uint64_t samplerToken = (uint64_t)1 << 51;

            uint64_t expectedTokens = bufferToken | imageToken | samplerToken;
            EXPECT_NE(0u, tokenMask & expectedTokens);
        }

        curbeParamsForBlock.resize(0);
    }
}

TEST(KernelReflectionSurfaceTestSingle, GivenNonParentKernelWhenCreatingKernelReflectionSurfaceThenKernelReflectionSurfaceIsNotCreated) {
    MockClDevice device{new MockDevice};
    MockProgram program(toClDeviceVector(device));
    KernelInfo info;
    MockKernel kernel(&program, info, device);

    EXPECT_FALSE(kernel.isParentKernel);

    kernel.createReflectionSurface();

    auto reflectionSurface = kernel.getKernelReflectionSurface();

    EXPECT_EQ(nullptr, reflectionSurface);
}

TEST(KernelReflectionSurfaceTestSingle, GivenNonSchedulerKernelWithForcedSchedulerDispatchWhenCreatingKernelReflectionSurfaceThenKernelReflectionSurfaceIsNotCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceDispatchScheduler.set(true);

    MockClDevice device{new MockDevice};
    MockProgram program(toClDeviceVector(device));
    KernelInfo info;
    MockKernel kernel(&program, info, device);

    EXPECT_FALSE(kernel.isParentKernel);

    kernel.createReflectionSurface();

    auto reflectionSurface = kernel.getKernelReflectionSurface();

    EXPECT_EQ(nullptr, reflectionSurface);
}

TEST(KernelReflectionSurfaceTestSingle, GivenNoKernelArgsWhenObtainingKernelReflectionSurfaceThenParamsAreCorrect) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(defaultHwInfo);
    MockContext context;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockProgram program(toClDeviceVector(*device));
    MockKernelInfo *blockInfo = new MockKernelInfo;
    MockKernelInfo &info = *blockInfo;
    cl_queue_properties properties[1] = {0};
    DeviceQueue devQueue(&context, device.get(), properties[0]);

    info.kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;
    info.setCrossThreadDataSize(0);
    info.setBindingTable(0, 0);

    MockKernel kernel(&program, info, *device);

    EXPECT_TRUE(kernel.isParentKernel);

    program.blockKernelManager->addBlockKernelInfo(blockInfo);

    kernel.createReflectionSurface();
    auto reflectionSurface = kernel.getKernelReflectionSurface();
    EXPECT_NE(nullptr, reflectionSurface);

    kernel.patchReflectionSurface<true>(&devQueue, nullptr);

    uint64_t undefinedOffset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;

    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::defaultQueue.offset);
    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::devQueue.offset);
    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::eventPool.offset);

    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::defaultQueue.size);
    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::devQueue.size);
    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::eventPool.size);
}

TEST(KernelReflectionSurfaceTestSingle, GivenDeviceQueueKernelArgWhenObtainingKernelReflectionSurfaceThenParamsAreCorrect) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(defaultHwInfo);
    MockContext context;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockProgram program(toClDeviceVector(*device));

    MockKernelInfo *blockInfo = new MockKernelInfo;
    MockKernelInfo &info = *blockInfo;
    cl_queue_properties properties[1] = {0};
    DeviceQueue devQueue(&context, device.get(), properties[0]);

    uint32_t devQueueCurbeOffset = 16;
    uint32_t devQueueCurbeSize = 4;

    info.kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;
    info.setCrossThreadDataSize(0);
    info.setBindingTable(0, 0);
    info.addArgDevQueue(0, devQueueCurbeOffset, devQueueCurbeSize);

    MockKernel kernel(&program, info, *device);

    EXPECT_TRUE(kernel.isParentKernel);

    program.blockKernelManager->addBlockKernelInfo(blockInfo);

    kernel.createReflectionSurface();
    auto reflectionSurface = kernel.getKernelReflectionSurface();
    EXPECT_NE(nullptr, reflectionSurface);

    kernel.patchReflectionSurface<true>(&devQueue, nullptr);
    uint64_t undefinedOffset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;

    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::defaultQueue.offset);
    EXPECT_EQ(devQueueCurbeOffset, MockKernel::ReflectionSurfaceHelperPublic::devQueue.offset);
    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::eventPool.offset);

    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::defaultQueue.size);
    EXPECT_EQ(4u, MockKernel::ReflectionSurfaceHelperPublic::devQueue.size);
    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::eventPool.size);
}

TEST_P(KernelReflectionSurfaceTest, WhenCreatingKernelReflectionSurfaceThenKernelReflectionSurfaceIsCorrect) {
    EXPECT_TRUE(pKernel->isParentKernel);

    BlockKernelManager *blockManager = pProgram->getBlockKernelManager();
    size_t blockCount = blockManager->getCount();

    EXPECT_EQ(3u, blockCount);

    size_t maxConstantBufferSize = 0;
    size_t parentImageCount = 0;
    size_t parentSamplerCount = 0;

    if (pKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName == "kernel_reflection") {
        parentImageCount = 1;
        parentSamplerCount = 1;
    }

    size_t samplerStateArrayAndBorderColorTotalSize = 0;
    size_t totalCurbeParamsSize = 0;

    std::vector<size_t> blockCurbeParamCounts(blockCount);
    std::vector<size_t> samplerStateAndBorderColorSizes(blockCount);
    std::vector<IGIL_KernelCurbeParams> curbeParamsForBlock;

    for (size_t i = 0; i < blockCount; i++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
        uint64_t tokenMask = 0;
        uint32_t firstSSHTokenIndex = 0;
        MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParamsForBlock, tokenMask, firstSSHTokenIndex, *pBlockInfo, pDevice->getHardwareInfo());

        blockCurbeParamCounts[i] = curbeParamsForBlock.size();

        maxConstantBufferSize = std::max(maxConstantBufferSize, static_cast<size_t>(pBlockInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize));
        totalCurbeParamsSize += blockCurbeParamCounts[i];

        size_t samplerStateAndBorderColorSize = pBlockInfo->getSamplerStateArraySize(pDevice->getHardwareInfo());
        samplerStateAndBorderColorSize = alignUp(samplerStateAndBorderColorSize, Sampler::samplerStateArrayAlignment);
        samplerStateAndBorderColorSize += pBlockInfo->getBorderColorStateSize();
        samplerStateAndBorderColorSizes[i] = samplerStateAndBorderColorSize;

        samplerStateArrayAndBorderColorTotalSize += alignUp(samplerStateAndBorderColorSizes[i], sizeof(void *));
        curbeParamsForBlock.clear();
    }

    totalCurbeParamsSize *= sizeof(IGIL_KernelCurbeParams);

    size_t expectedReflectionSurfaceSize = alignUp(sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData) * blockCount, sizeof(void *));
    expectedReflectionSurfaceSize += alignUp(sizeof(IGIL_KernelData), sizeof(void *)) * blockCount;
    expectedReflectionSurfaceSize += (parentSamplerCount * sizeof(IGIL_SamplerParams) + maxConstantBufferSize) * blockCount +
                                     totalCurbeParamsSize +
                                     parentImageCount * sizeof(IGIL_ImageParamters) +
                                     parentSamplerCount * sizeof(IGIL_ParentSamplerParams) +
                                     samplerStateArrayAndBorderColorTotalSize;

    pKernel->createReflectionSurface();
    auto reflectionSurface = pKernel->getKernelReflectionSurface();

    ASSERT_NE(nullptr, reflectionSurface);
    EXPECT_EQ(expectedReflectionSurfaceSize, reflectionSurface->getUnderlyingBufferSize());

    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface->getUnderlyingBuffer());

    uint32_t parentImages = 0;
    uint32_t parentSamplers = 0;

    if (pKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName == "kernel_reflection") {
        parentImages = 1;
        parentSamplers = 1;
        EXPECT_LT(sizeof(IGIL_KernelDataHeader), pKernelHeader->m_ParentSamplerParamsOffset);
    }

    EXPECT_EQ(blockCount, pKernelHeader->m_numberOfKernels);
    EXPECT_EQ(parentImages, pKernelHeader->m_ParentKernelImageCount);
    EXPECT_LT(sizeof(IGIL_KernelDataHeader), pKernelHeader->m_ParentImageDataOffset);
    EXPECT_EQ(parentSamplers, pKernelHeader->m_ParentSamplerCount);
    EXPECT_NE(pKernelHeader->m_ParentImageDataOffset, pKernelHeader->m_ParentSamplerParamsOffset);

    // Curbe tokens
    EXPECT_NE(0u, totalCurbeParamsSize);

    for (uint32_t i = 0; i < pKernelHeader->m_numberOfKernels; i++) {
        IGIL_KernelAddressData *addressData = pKernelHeader->m_data;
        EXPECT_NE(0u, addressData->m_KernelDataOffset);
        EXPECT_NE(0u, addressData->m_BTSize);
        EXPECT_NE(0u, addressData->m_SSHTokensOffset);
        EXPECT_NE(0u, addressData->m_ConstantBufferOffset);
        EXPECT_NE(0u, addressData->m_BTSoffset);

        IGIL_KernelData *kernelData = reinterpret_cast<IGIL_KernelData *>(ptrOffset(pKernelHeader, (size_t)(addressData->m_KernelDataOffset)));

        EXPECT_NE_VAL(0u, kernelData->m_SIMDSize);
        EXPECT_NE_VAL(0u, kernelData->m_PatchTokensMask);
        EXPECT_NE_VAL(0u, kernelData->m_numberOfCurbeParams);
        EXPECT_NE_VAL(0u, kernelData->m_numberOfCurbeTokens);
        EXPECT_NE_VAL(0u, kernelData->m_sizeOfConstantBuffer);

        for (uint32_t j = 0; j < kernelData->m_numberOfCurbeParams; j++) {
            EXPECT_NE_VAL(0u, kernelData->m_data[j].m_parameterType);
        }
    }
}

TEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithArgsWhenPassedToGetCurbeParamsThenProperFirstSshTokenIndexIsReturned) {

    KernelInfo info;

    info.kernelDescriptor.payloadMappings.explicitArgs.resize(9);

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pDevice->getHardwareInfo());

    EXPECT_EQ(curbeParams.size() - 9, firstSSHTokenIndex);
}

TEST_P(KernelReflectionSurfaceTest, GivenKernelInfoWithExecutionParametersWhenPassedToGetCurbeParamsThenProperCurbeParamsAreReturned) {

    KernelInfo info;

    std::vector<uint32_t> supportedExecutionParamTypes = {iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE,
                                                          iOpenCL::DATA_PARAMETER_GLOBAL_WORK_SIZE,
                                                          iOpenCL::DATA_PARAMETER_NUM_WORK_GROUPS,
                                                          iOpenCL::DATA_PARAMETER_WORK_DIMENSIONS,
                                                          iOpenCL::DATA_PARAMETER_GLOBAL_WORK_OFFSET,
                                                          iOpenCL::DATA_PARAMETER_NUM_HARDWARE_THREADS,
                                                          iOpenCL::DATA_PARAMETER_PARENT_EVENT,
                                                          iOpenCL::DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE};

    std::sort(supportedExecutionParamTypes.begin(), supportedExecutionParamTypes.end());

    const uint32_t workDimOffset = 80;
    const uint32_t numHwThreads = 84;
    const uint32_t parentEventOffset = 88;

    const uint32_t lwsOffsets[3] = {4, 8, 12};
    const uint32_t lwsOffsets2[3] = {16, 20, 24};
    const uint32_t gwsOffsets[3] = {28, 32, 36};
    const uint32_t numOffsets[3] = {40, 44, 48};
    const uint32_t globalOffsetOffsets[3] = {52, 56, 60};
    const uint32_t enqueuedLocalWorkSizeOffsets[3] = {64, 68, 72};

    info.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] = lwsOffsets[0];
    info.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1] = lwsOffsets[1];
    info.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2] = lwsOffsets[2];

    info.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize2[0] = lwsOffsets2[0];
    info.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize2[1] = lwsOffsets2[1];
    info.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize2[2] = lwsOffsets2[2];

    info.kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = gwsOffsets[0];
    info.kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[1] = gwsOffsets[1];
    info.kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[2] = gwsOffsets[2];

    info.kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = numOffsets[0];
    info.kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[1] = numOffsets[1];
    info.kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[2] = numOffsets[2];

    info.kernelDescriptor.payloadMappings.dispatchTraits.globalWorkOffset[0] = globalOffsetOffsets[0];
    info.kernelDescriptor.payloadMappings.dispatchTraits.globalWorkOffset[1] = globalOffsetOffsets[1];
    info.kernelDescriptor.payloadMappings.dispatchTraits.globalWorkOffset[2] = globalOffsetOffsets[2];

    info.kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[0] = enqueuedLocalWorkSizeOffsets[0];
    info.kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[1] = enqueuedLocalWorkSizeOffsets[1];
    info.kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[2] = enqueuedLocalWorkSizeOffsets[2];

    info.kernelDescriptor.payloadMappings.dispatchTraits.workDim = workDimOffset;
    // NUM_HARDWARE_THREADS unsupported
    EXPECT_TRUE(numHwThreads > 0u);
    info.kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueParentEvent = parentEventOffset;

    std::vector<IGIL_KernelCurbeParams> curbeParams;
    uint64_t tokenMask = 0;
    uint32_t firstSSHTokenIndex = 0;
    MockKernel::ReflectionSurfaceHelperPublic::getCurbeParams(curbeParams, tokenMask, firstSSHTokenIndex, info, pPlatform->getClDevice(0)->getHardwareInfo());

    EXPECT_LE(supportedExecutionParamTypes.size(), curbeParams.size());

    for (auto paramType : supportedExecutionParamTypes) {
        auto foundParams = 0u;
        auto j = 0;
        for (const auto &curbeParam : curbeParams) {
            if (paramType == curbeParam.m_parameterType) {

                foundParams++;

                uint32_t index = curbeParam.m_sourceOffset / sizeof(uint32_t);

                switch (paramType) {
                case iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE:
                    if (j < 3) {
                        EXPECT_EQ(lwsOffsets[index], curbeParam.m_patchOffset);
                    } else {
                        EXPECT_EQ(lwsOffsets2[index], curbeParam.m_patchOffset);
                    }
                    break;
                case iOpenCL::DATA_PARAMETER_GLOBAL_WORK_SIZE:
                    EXPECT_EQ(gwsOffsets[index], curbeParam.m_patchOffset);
                    break;
                case iOpenCL::DATA_PARAMETER_NUM_WORK_GROUPS:
                    EXPECT_EQ(numOffsets[index], curbeParam.m_patchOffset);
                    break;
                case iOpenCL::DATA_PARAMETER_GLOBAL_WORK_OFFSET:
                    EXPECT_EQ(globalOffsetOffsets[index], curbeParam.m_patchOffset);
                    break;
                case iOpenCL::DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE:
                    EXPECT_EQ(enqueuedLocalWorkSizeOffsets[index], curbeParam.m_patchOffset);
                    break;
                }
            }
            j++;
        }
        switch (paramType) {
        case iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE:
            EXPECT_EQ(6u, foundParams) << "Parameter token: " << paramType;
            break;
        case iOpenCL::DATA_PARAMETER_GLOBAL_WORK_SIZE:
        case iOpenCL::DATA_PARAMETER_NUM_WORK_GROUPS:
        case iOpenCL::DATA_PARAMETER_GLOBAL_WORK_OFFSET:
        case iOpenCL::DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE:
            EXPECT_EQ(3u, foundParams) << "Parameter token: " << paramType;
            break;
        }
    }

    for (auto paramType : supportedExecutionParamTypes) {
        auto foundParams = 0u;

        for (const auto &curbeParam : curbeParams) {
            if (paramType == curbeParam.m_parameterType) {

                switch (paramType) {
                case iOpenCL::DATA_PARAMETER_PARENT_EVENT:
                    EXPECT_EQ(parentEventOffset, curbeParam.m_patchOffset);
                    foundParams++;
                    break;
                case iOpenCL::DATA_PARAMETER_WORK_DIMENSIONS:
                    EXPECT_EQ(workDimOffset, curbeParam.m_patchOffset);
                    foundParams++;
                    break;
                }
            }
        }

        switch (paramType) {
        case iOpenCL::DATA_PARAMETER_PARENT_EVENT:
        case iOpenCL::DATA_PARAMETER_WORK_DIMENSIONS:
            EXPECT_EQ(1u, foundParams);
            break;
        }
    }

    for (auto paramType : supportedExecutionParamTypes) {
        if (paramType != iOpenCL::DATA_PARAMETER_NUM_HARDWARE_THREADS) {
            auto expectedTokens = (uint64_t)1 << paramType;
            EXPECT_TRUE((tokenMask & expectedTokens) > 0) << "Parameter Token: " << paramType;
        }
    }
}

static const char *binaryFile = "simple_block_kernel";
static const char *KernelNames[] = {"kernel_reflection", "simple_block_kernel"};

INSTANTIATE_TEST_CASE_P(KernelReflectionSurfaceTest,
                        KernelReflectionSurfaceTest,
                        ::testing::Combine(
                            ::testing::Values(binaryFile),
                            ::testing::ValuesIn(KernelNames)));

HWCMDTEST_P(IGFX_GEN8_CORE, KernelReflectionSurfaceWithQueueTest, WhenObtainingKernelReflectionSurfacePatchesThenCurbeIsBlocked) {
    BlockKernelManager *blockManager = pProgram->getBlockKernelManager();
    size_t blockCount = blockManager->getCount();

    EXPECT_NE(0u, blockCount);

    std::vector<IGIL_KernelCurbeParams> curbeParamsForBlock;

    pKernel->createReflectionSurface();
    pKernel->patchReflectionSurface(pDevQueue, nullptr);

    auto *reflectionSurface = pKernel->getKernelReflectionSurface();
    ASSERT_NE(nullptr, reflectionSurface);
    void *reflectionSurfaceMemory = reflectionSurface->getUnderlyingBuffer();

    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface->getUnderlyingBuffer());

    EXPECT_EQ(blockCount, pKernelHeader->m_numberOfKernels);

    for (uint32_t i = 0; i < pKernelHeader->m_numberOfKernels; i++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);

        IGIL_KernelAddressData *addressData = pKernelHeader->m_data;

        EXPECT_NE(0u, addressData[i].m_ConstantBufferOffset);

        void *pCurbe = ptrOffset(reflectionSurfaceMemory, (size_t)(addressData[i].m_ConstantBufferOffset));

        const auto &eventPoolSurfaceAddress = pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress;
        if (isValidOffset(eventPoolSurfaceAddress.stateless)) {
            auto *patchedPointer = ptrOffset(pCurbe, eventPoolSurfaceAddress.stateless);
            if (eventPoolSurfaceAddress.pointerSize == sizeof(uint32_t)) {
                uint32_t *patchedValue = static_cast<uint32_t *>(patchedPointer);
                uint64_t patchedValue64 = *patchedValue;
                EXPECT_EQ(pDevQueue->getEventPoolBuffer()->getGpuAddress(), patchedValue64);
            } else if (eventPoolSurfaceAddress.pointerSize == sizeof(uint64_t)) {
                uint64_t *patchedValue = static_cast<uint64_t *>(patchedPointer);
                EXPECT_EQ(pDevQueue->getEventPoolBuffer()->getGpuAddress(), *patchedValue);
            }
        }

        const auto &defaultQueueSurfaceAddress = pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress;
        if (isValidOffset(defaultQueueSurfaceAddress.stateless)) {
            auto *patchedPointer = ptrOffset(pCurbe, defaultQueueSurfaceAddress.stateless);
            if (defaultQueueSurfaceAddress.pointerSize == sizeof(uint32_t)) {
                uint32_t *patchedValue = static_cast<uint32_t *>(patchedPointer);
                uint64_t patchedValue64 = *patchedValue;
                EXPECT_EQ(pDevQueue->getQueueBuffer()->getGpuAddress(), patchedValue64);
            } else if (defaultQueueSurfaceAddress.pointerSize == sizeof(uint64_t)) {
                uint64_t *patchedValue = static_cast<uint64_t *>(patchedPointer);
                EXPECT_EQ(pDevQueue->getQueueBuffer()->getGpuAddress(), *patchedValue);
            }
        }

        for (const auto &arg : pBlockInfo->getExplicitArgs()) {
            if (arg.getExtendedTypeInfo().isDeviceQueue) {
                auto &asPtr = arg.as<ArgDescPointer>();
                auto *patchedPointer = ptrOffset(pCurbe, asPtr.stateless);
                if (asPtr.pointerSize == sizeof(uint32_t)) {
                    uint32_t *patchedValue = static_cast<uint32_t *>(patchedPointer);
                    uint64_t patchedValue64 = *patchedValue;
                    EXPECT_EQ(pDevQueue->getQueueBuffer()->getGpuAddress(), patchedValue64);
                } else if (asPtr.pointerSize == sizeof(uint64_t)) {
                    uint64_t *patchedValue = static_cast<uint64_t *>(patchedPointer);
                    EXPECT_EQ(pDevQueue->getQueueBuffer()->getGpuAddress(), *patchedValue);
                }
            }
        }
    }
}

HWCMDTEST_P(IGFX_GEN8_CORE, KernelReflectionSurfaceWithQueueTest, WhenObtainingKernelReflectionSurfaceThenParentImageAndSamplersParamsAreSet) {
    BlockKernelManager *blockManager = pProgram->getBlockKernelManager();
    size_t blockCount = blockManager->getCount();

    EXPECT_NE(0u, blockCount);

    std::vector<IGIL_KernelCurbeParams> curbeParamsForBlock;

    std::unique_ptr<Image> image3d(ImageHelper<Image3dDefaults>::create(context));
    std::unique_ptr<Sampler> sampler(new MockSampler(context,
                                                     true,
                                                     (cl_addressing_mode)CL_ADDRESS_CLAMP_TO_EDGE,
                                                     (cl_filter_mode)CL_FILTER_LINEAR));

    cl_sampler samplerCl = sampler.get();
    cl_mem imageCl = image3d.get();

    if (pKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName == "kernel_reflection") {
        pKernel->setArgSampler(0, sizeof(cl_sampler), &samplerCl);
        pKernel->setArgImage(1, sizeof(cl_mem), &imageCl);
    }

    pKernel->createReflectionSurface();

    auto *reflectionSurface = pKernel->getKernelReflectionSurface();
    ASSERT_NE(nullptr, reflectionSurface);

    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface->getUnderlyingBuffer());
    IGIL_ImageParamters *pParentImageParams = reinterpret_cast<IGIL_ImageParamters *>(ptrOffset(pKernelHeader, (size_t)pKernelHeader->m_ParentImageDataOffset));
    IGIL_ParentSamplerParams *pParentSamplerParams = reinterpret_cast<IGIL_ParentSamplerParams *>(ptrOffset(pKernelHeader, (size_t)pKernelHeader->m_ParentSamplerParamsOffset));

    memset(pParentImageParams, 0xff, sizeof(IGIL_ImageParamters) * pKernelHeader->m_ParentKernelImageCount);
    memset(pParentSamplerParams, 0xff, sizeof(IGIL_ParentSamplerParams) * pKernelHeader->m_ParentSamplerCount);

    pKernel->patchReflectionSurface(pDevQueue, nullptr);

    EXPECT_EQ(blockCount, pKernelHeader->m_numberOfKernels);

    for (uint32_t i = 0; i < pKernelHeader->m_numberOfKernels; i++) {

        if (pKernelHeader->m_ParentKernelImageCount > 0) {
            uint32_t imageIndex = 0;
            for (const auto &arg : pKernel->getKernelInfo().getExplicitArgs()) {
                if (arg.is<ArgDescriptor::ArgTImage>()) {
                    const auto &asImg = arg.as<ArgDescImage>();
                    EXPECT_EQ(pParentImageParams[imageIndex].m_ObjectID, asImg.bindful);
                    imageIndex++;
                }
            }
        }

        if (pKernelHeader->m_ParentSamplerCount > 0) {
            uint32_t samplerIndex = 0;
            for (const auto &arg : pKernel->getKernelInfo().getExplicitArgs()) {
                if (arg.is<ArgDescriptor::ArgTSampler>()) {
                    const auto &asSmp = arg.as<ArgDescSampler>();
                    EXPECT_EQ(pParentSamplerParams[samplerIndex].m_ObjectID, static_cast<uint>(OCLRT_ARG_OFFSET_TO_SAMPLER_OBJECT_ID(asSmp.bindful)));
                    samplerIndex++;
                }
            }
        }
    }
}

INSTANTIATE_TEST_CASE_P(KernelReflectionSurfaceWithQueueTest,
                        KernelReflectionSurfaceWithQueueTest,
                        ::testing::Combine(
                            ::testing::Values(binaryFile),
                            ::testing::ValuesIn(KernelNames)));

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

typedef ParentKernelCommandQueueFixture ReflectionSurfaceTestForPrintfHandler;

TEST_F(ReflectionSurfaceTestForPrintfHandler, GivenPrintfHandlerWhenPatchingReflectionSurfaceThenPrintBufferIsPatched) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);
    MockContext context(device);
    cl_queue_properties properties[3] = {0};
    MockParentKernel::CreateParams createParams{};
    createParams.addPrintfForBlock = true;
    createParams.addPrintfForParent = true;
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context, createParams));

    DeviceQueue devQueue(&context, device, properties[0]);
    parentKernel->createReflectionSurface();

    context.setDefaultDeviceQueue(&devQueue);

    MockMultiDispatchInfo multiDispatchInfo(device, parentKernel.get());
    PrintfHandler *printfHandler = PrintfHandler::create(multiDispatchInfo, *device);
    printfHandler->prepareDispatch(multiDispatchInfo);

    parentKernel->patchReflectionSurface<true>(&devQueue, printfHandler);

    const auto &printfSurfaceArg = parentKernel->getProgram()->getBlockKernelManager()->getBlockKernelInfo(0)->kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress;
    EXPECT_EQ(printfSurfaceArg.stateless, MockKernel::ReflectionSurfaceHelperPublic::printfBuffer.offset);
    EXPECT_EQ(printfHandler->getSurface()->getGpuAddress(), MockKernel::ReflectionSurfaceHelperPublic::printfBuffer.address);
    EXPECT_EQ(printfSurfaceArg.pointerSize, MockKernel::ReflectionSurfaceHelperPublic::printfBuffer.size);

    delete printfHandler;
}

TEST_F(ReflectionSurfaceTestForPrintfHandler, GivenNoPrintfSurfaceWhenPatchingReflectionSurfaceThenPrintBufferIsNotPatched) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);
    MockContext context(device);
    cl_queue_properties properties[3] = {0};
    MockParentKernel::CreateParams createParams{};
    createParams.addPrintfForBlock = true;
    createParams.addPrintfForParent = true;
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context, createParams));

    DeviceQueue devQueue(&context, device, properties[0]);
    parentKernel->createReflectionSurface();

    context.setDefaultDeviceQueue(&devQueue);

    MockMultiDispatchInfo multiDispatchInfo(device, parentKernel.get());
    PrintfHandler *printfHandler = PrintfHandler::create(multiDispatchInfo, *device);

    parentKernel->patchReflectionSurface<true>(&devQueue, printfHandler);

    const auto &printfSurfaceArg = parentKernel->getProgram()->getBlockKernelManager()->getBlockKernelInfo(0)->kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress;
    EXPECT_EQ(printfSurfaceArg.stateless, MockKernel::ReflectionSurfaceHelperPublic::printfBuffer.offset);
    EXPECT_EQ(0U, MockKernel::ReflectionSurfaceHelperPublic::printfBuffer.address);
    EXPECT_EQ(printfSurfaceArg.pointerSize, MockKernel::ReflectionSurfaceHelperPublic::printfBuffer.size);

    delete printfHandler;
}

class ReflectionSurfaceConstantValuesPatchingTest : public ClDeviceFixture,
                                                    public ::testing::Test {
  public:
    void SetUp() override {
        ClDeviceFixture::SetUp();
    }
    void TearDown() override {
        ClDeviceFixture::TearDown();
    }
};

TEST_F(ReflectionSurfaceConstantValuesPatchingTest, GivenBlockWithGlobalMemoryWhenReflectionSurfaceIsPatchedWithConstantValuesThenProgramGlobalMemoryAddressIsPatched) {

    MockContext context(pClDevice);
    MockParentKernel::CreateParams createParams{};
    createParams.addChildGlobalMemory = true;
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context, createParams));

    // graphicsMemory is released by Program
    GraphicsAllocation *globalMemory = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    parentKernel->mockProgram->setGlobalSurface(globalMemory);

    // Allocte reflectionSurface, 2 * 4096 should be enough
    GraphicsAllocation *reflectionSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), 2 * MemoryConstants::pageSize});
    parentKernel->setReflectionSurface(reflectionSurface);

    memset(reflectionSurface->getUnderlyingBuffer(), 0, reflectionSurface->getUnderlyingBufferSize());

    const uint32_t constBufferOffset = (uint32_t)alignUp(sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData) + sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams), sizeof(uint64_t));
    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface->getUnderlyingBuffer());
    pKernelHeader->m_numberOfKernels = 1;
    pKernelHeader->m_data[0].m_ConstantBufferOffset = constBufferOffset;

    parentKernel->patchBlocksCurbeWithConstantValues();

    auto *blockInfo = parentKernel->mockProgram->blockKernelManager->getBlockKernelInfo(0);

    uint32_t blockPatchOffset = blockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless;

    uint64_t *pCurbe = (uint64_t *)ptrOffset(reflectionSurface->getUnderlyingBuffer(), constBufferOffset + blockPatchOffset);

    EXPECT_EQ(globalMemory->getGpuAddressToPatch(), *pCurbe);
}

TEST_F(ReflectionSurfaceConstantValuesPatchingTest, GivenBlockWithGlobalMemoryAndProgramWithoutGlobalMemortWhenReflectionSurfaceIsPatchedWithConstantValuesThenZeroAddressIsPatched) {

    MockContext context(pClDevice);
    MockParentKernel::CreateParams createParams{};
    createParams.addChildGlobalMemory = true;
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context, createParams));

    if (parentKernel->mockProgram->getGlobalSurface(pClDevice->getRootDeviceIndex())) {
        pDevice->getMemoryManager()->freeGraphicsMemory(parentKernel->mockProgram->getGlobalSurface(pClDevice->getRootDeviceIndex()));
        parentKernel->mockProgram->setGlobalSurface(nullptr);
    }

    // Allocte reflectionSurface, 2 * 4096 should be enough
    GraphicsAllocation *reflectionSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), 2 * MemoryConstants::pageSize});
    parentKernel->setReflectionSurface(reflectionSurface);

    memset(reflectionSurface->getUnderlyingBuffer(), 0, reflectionSurface->getUnderlyingBufferSize());

    const uint32_t constBufferOffset = (uint32_t)alignUp(sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData) + sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams), sizeof(uint64_t));
    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface->getUnderlyingBuffer());
    pKernelHeader->m_numberOfKernels = 1;
    pKernelHeader->m_data[0].m_ConstantBufferOffset = constBufferOffset;

    parentKernel->patchBlocksCurbeWithConstantValues();

    auto *blockInfo = parentKernel->mockProgram->blockKernelManager->getBlockKernelInfo(0);

    uint32_t blockPatchOffset = blockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless;
    uint64_t *pCurbe = (uint64_t *)ptrOffset(reflectionSurface->getUnderlyingBuffer(), constBufferOffset + blockPatchOffset);

    EXPECT_EQ(0u, *pCurbe);
}

TEST_F(ReflectionSurfaceConstantValuesPatchingTest, GivenBlockWithConstantMemoryWhenReflectionSurfaceIsPatchedWithConstantValuesThenProgramConstantMemoryAddressIsPatched) {

    MockContext context(pClDevice);
    MockParentKernel::CreateParams createParams{};
    createParams.addChildConstantMemory = true;
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context, createParams));

    // graphicsMemory is released by Program
    GraphicsAllocation *constantMemory = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    parentKernel->mockProgram->setConstantSurface(constantMemory);

    // Allocte reflectionSurface, 2 * 4096 should be enough
    GraphicsAllocation *reflectionSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), 2 * MemoryConstants::pageSize});
    parentKernel->setReflectionSurface(reflectionSurface);

    memset(reflectionSurface->getUnderlyingBuffer(), 0, reflectionSurface->getUnderlyingBufferSize());

    const uint32_t constBufferOffset = (uint32_t)alignUp(sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData) + sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams), sizeof(uint64_t));
    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface->getUnderlyingBuffer());
    pKernelHeader->m_numberOfKernels = 1;
    pKernelHeader->m_data[0].m_ConstantBufferOffset = constBufferOffset;

    parentKernel->patchBlocksCurbeWithConstantValues();

    auto *blockInfo = parentKernel->mockProgram->blockKernelManager->getBlockKernelInfo(0);

    uint32_t blockPatchOffset = blockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless;

    uint64_t *pCurbe = (uint64_t *)ptrOffset(reflectionSurface->getUnderlyingBuffer(), constBufferOffset);
    uint64_t *pCurbeToPatch = (uint64_t *)ptrOffset(reflectionSurface->getUnderlyingBuffer(), constBufferOffset + blockPatchOffset);

    EXPECT_EQ(constantMemory->getGpuAddressToPatch(), *pCurbeToPatch);

    std::unique_ptr<char> zeroMemory = std::unique_ptr<char>(new char[4096]);
    memset(zeroMemory.get(), 0, 4096);
    // memory before is not written
    EXPECT_THAT(zeroMemory.get(), MemCompare(pCurbe, std::min(4096u, blockPatchOffset)));

    //memory after is not written
    EXPECT_THAT(zeroMemory.get(), MemCompare(pCurbeToPatch + 1, std::min(4096u, 8192u - constBufferOffset - blockPatchOffset - (uint32_t)sizeof(uint64_t))));
}

TEST_F(ReflectionSurfaceConstantValuesPatchingTest, GivenBlockWithConstantMemoryAndProgramWithoutConstantMemortWhenReflectionSurfaceIsPatchedWithConstantValuesThenZeroAddressIsPatched) {

    MockContext context(pClDevice);
    MockParentKernel::CreateParams createParams{};
    createParams.addChildConstantMemory = true;
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context, createParams));

    if (parentKernel->mockProgram->getConstantSurface(pClDevice->getRootDeviceIndex())) {
        pDevice->getMemoryManager()->freeGraphicsMemory(parentKernel->mockProgram->getConstantSurface(pClDevice->getRootDeviceIndex()));
        parentKernel->mockProgram->setConstantSurface(nullptr);
    }

    // Allocte reflectionSurface, 2 * 4096 should be enough
    GraphicsAllocation *reflectionSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), 2 * MemoryConstants::pageSize});
    parentKernel->setReflectionSurface(reflectionSurface);

    memset(reflectionSurface->getUnderlyingBuffer(), 0, reflectionSurface->getUnderlyingBufferSize());

    const uint32_t constBufferOffset = (uint32_t)alignUp(sizeof(IGIL_KernelDataHeader) + sizeof(IGIL_KernelAddressData) + sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams), sizeof(uint64_t));
    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface->getUnderlyingBuffer());
    pKernelHeader->m_numberOfKernels = 1;
    pKernelHeader->m_data[0].m_ConstantBufferOffset = constBufferOffset;

    parentKernel->patchBlocksCurbeWithConstantValues();

    auto *blockInfo = parentKernel->mockProgram->blockKernelManager->getBlockKernelInfo(0);

    uint32_t blockPatchOffset = blockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless;

    uint64_t *pCurbe = (uint64_t *)ptrOffset(reflectionSurface->getUnderlyingBuffer(), constBufferOffset);
    uint64_t *pCurbeToPatch = (uint64_t *)ptrOffset(reflectionSurface->getUnderlyingBuffer(), constBufferOffset + blockPatchOffset);

    EXPECT_EQ(0u, *pCurbeToPatch);

    std::unique_ptr<char> zeroMemory = std::unique_ptr<char>(new char[4096]);
    memset(zeroMemory.get(), 0, 4096);

    // memory before is not written
    EXPECT_THAT(zeroMemory.get(), MemCompare(pCurbe, std::min(4096u, blockPatchOffset)));

    //memory after is not written
    EXPECT_THAT(zeroMemory.get(), MemCompare(pCurbeToPatch + 1, std::min(4096u, 8192u - constBufferOffset - blockPatchOffset - (uint32_t)sizeof(uint64_t))));
}

using KernelReflectionMultiDeviceTest = MultiRootDeviceFixture;

TEST_F(KernelReflectionMultiDeviceTest, GivenNoKernelArgsWhenObtainingKernelReflectionSurfaceThenParamsAreCorrect) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device1);

    MockProgram program(context.get(), false, toClDeviceVector(*device1));
    MockKernelInfo *blockInfo = new MockKernelInfo;
    MockKernelInfo &info = *blockInfo;
    cl_queue_properties properties[1] = {0};
    DeviceQueue devQueue(context.get(), device1, properties[0]);

    info.kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;
    info.setCrossThreadDataSize(0);
    info.setBindingTable(0, 0);

    MockKernel kernel(&program, info, *device1);

    EXPECT_TRUE(kernel.isParentKernel);

    program.blockKernelManager->addBlockKernelInfo(blockInfo);

    kernel.createReflectionSurface();
    auto reflectionSurface = kernel.getKernelReflectionSurface();
    ASSERT_NE(nullptr, reflectionSurface);
    EXPECT_EQ(expectedRootDeviceIndex, reflectionSurface->getRootDeviceIndex());

    kernel.patchReflectionSurface<true>(&devQueue, nullptr);

    uint64_t undefinedOffset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;

    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::defaultQueue.offset);
    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::devQueue.offset);
    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::eventPool.offset);

    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::defaultQueue.size);
    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::devQueue.size);
    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::eventPool.size);
}

TEST_F(KernelReflectionMultiDeviceTest, GivenDeviceQueueKernelArgWhenObtainingKernelReflectionSurfaceThenParamsAreCorrect) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device1);

    MockProgram program(context.get(), false, toClDeviceVector(*device1));

    MockKernelInfo *blockInfo = new MockKernelInfo;
    MockKernelInfo &info = *blockInfo;
    cl_queue_properties properties[1] = {0};
    DeviceQueue devQueue(context.get(), device1, properties[0]);

    uint32_t devQueueCurbeOffset = 16;
    uint32_t devQueueCurbeSize = 4;

    info.kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;
    info.setCrossThreadDataSize(0);
    info.setBindingTable(0, 0);
    info.addArgDevQueue(0, devQueueCurbeOffset, devQueueCurbeSize);

    MockKernel kernel(&program, info, *device1);

    EXPECT_TRUE(kernel.isParentKernel);

    program.blockKernelManager->addBlockKernelInfo(blockInfo);

    kernel.createReflectionSurface();
    auto reflectionSurface = kernel.getKernelReflectionSurface();
    ASSERT_NE(nullptr, reflectionSurface);
    EXPECT_EQ(expectedRootDeviceIndex, reflectionSurface->getRootDeviceIndex());

    kernel.patchReflectionSurface<true>(&devQueue, nullptr);
    uint64_t undefinedOffset = MockKernel::ReflectionSurfaceHelperPublic::undefinedOffset;

    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::defaultQueue.offset);
    EXPECT_EQ(devQueueCurbeOffset, MockKernel::ReflectionSurfaceHelperPublic::devQueue.offset);
    EXPECT_EQ(undefinedOffset, MockKernel::ReflectionSurfaceHelperPublic::eventPool.offset);

    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::defaultQueue.size);
    EXPECT_EQ(4u, MockKernel::ReflectionSurfaceHelperPublic::devQueue.size);
    EXPECT_EQ(0u, MockKernel::ReflectionSurfaceHelperPublic::eventPool.size);
}
