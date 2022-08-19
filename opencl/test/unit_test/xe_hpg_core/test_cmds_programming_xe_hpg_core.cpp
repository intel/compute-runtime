/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;
using CmdsProgrammingTestsXeHpgCore = UltCommandStreamReceiverTest;

XE_HPG_CORETEST_F(CmdsProgrammingTestsXeHpgCore, givenL3ToL1DebugFlagWhenStatelessMocsIsProgrammedThenItHasL1CachingOn) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(1u);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);

    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);

    auto actualL1CachePolocy = static_cast<uint8_t>(stateBaseAddress->getL1CachePolicyL1CacheControl());

    const uint8_t expectedL1CachePolicy = 0;
    EXPECT_EQ(expectedL1CachePolicy, actualL1CachePolocy);
}

XE_HPG_CORETEST_F(CmdsProgrammingTestsXeHpgCore, whenAppendingRssThenProgramWBPL1CachePolicy) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto rssCmd = FamilyType::cmdInitRenderSurfaceState;

    MockContext context(pClDevice);
    auto multiGraphicsAllocation = MultiGraphicsAllocation(pClDevice->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(allocation);

    std::unique_ptr<BufferHw<FamilyType>> buffer(static_cast<BufferHw<FamilyType> *>(
        BufferHw<FamilyType>::create(&context, {}, 0, 0, allocationSize, nullptr, nullptr, std::move(multiGraphicsAllocation), false, false, false)));

    NEO::EncodeSurfaceStateArgs args;
    args.outMemory = &rssCmd;
    args.graphicsAddress = allocation->getGpuAddress();
    args.size = allocation->getUnderlyingBufferSize();
    args.mocs = buffer->getMocsValue(false, false, pClDevice->getRootDeviceIndex());
    args.numAvailableDevices = pClDevice->getNumGenericSubDevices();
    args.allocation = allocation;
    args.gmmHelper = pClDevice->getGmmHelper();
    args.areMultipleSubDevicesInContext = true;

    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WBP, rssCmd.getL1CachePolicyL1CacheControl());
}

XE_HPG_CORETEST_F(CmdsProgrammingTestsXeHpgCore, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferConstPolicy) {
    MockContext context;
    const auto size = MemoryConstants::pageSize;
    const auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
    const auto flags = CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY;

    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        flags,
        size,
        ptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    const auto expectedMocs = context.getDevice(0)->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    auto actualL1CachePolocy = static_cast<uint8_t>(surfaceState.getL1CachePolicyL1CacheControl());

    const uint8_t expectedL1CachePolicy = 0;
    EXPECT_EQ(expectedL1CachePolicy, actualL1CachePolocy);

    alignedFree(ptr);
}

XE_HPG_CORETEST_F(CmdsProgrammingTestsXeHpgCore, givenDecompressInL3ForImage2dFromBufferEnabledWhenProgrammingStateForImage2dFrom3dCompressedBufferThenCorrectFlagsAreSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    MockContext context;
    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    imageDesc.mem_object = clCreateBuffer(&context, CL_MEM_READ_WRITE, imageDesc.image_height * imageDesc.image_width, nullptr, &retVal);

    auto gmm = new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gmm->isCompressionEnabled = true;

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);
    buffer->getGraphicsAllocation(0)->setGmm(gmm, 0);

    auto gmmResourceInfo = buffer->getMultiGraphicsAllocation().getDefaultGraphicsAllocation()->getDefaultGmm()->gmmResourceInfo.get();
    auto bufferCompressionFormat = context.getDevice(0)->getGmmClientContext()->getSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());

    auto surfaceFormat = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
                                                      CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    {
        DebugManagerStateRestore restorer;
        DebugManager.flags.DecompressInL3ForImage2dFromBuffer.set(-1);
        uint32_t forcedCompressionFormat = 3;
        DebugManager.flags.ForceBufferCompressionFormat.set(forcedCompressionFormat);
        auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
        imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex(), false);
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, surfaceState.getAuxiliarySurfaceMode());
        EXPECT_EQ(1u, surfaceState.getDecompressInL3());
        EXPECT_EQ(1u, surfaceState.getMemoryCompressionEnable());
        EXPECT_EQ(RENDER_SURFACE_STATE::MEMORY_COMPRESSION_TYPE::MEMORY_COMPRESSION_TYPE_3D_COMPRESSION, surfaceState.getMemoryCompressionType());
        EXPECT_EQ(forcedCompressionFormat, surfaceState.getCompressionFormat());
    }

    {
        DebugManagerStateRestore restorer;
        DebugManager.flags.DecompressInL3ForImage2dFromBuffer.set(1);

        auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
        imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex(), false);
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, surfaceState.getAuxiliarySurfaceMode());
        EXPECT_EQ(1u, surfaceState.getDecompressInL3());
        EXPECT_EQ(1u, surfaceState.getMemoryCompressionEnable());
        EXPECT_EQ(RENDER_SURFACE_STATE::MEMORY_COMPRESSION_TYPE::MEMORY_COMPRESSION_TYPE_3D_COMPRESSION, surfaceState.getMemoryCompressionType());
        EXPECT_EQ(bufferCompressionFormat, surfaceState.getCompressionFormat());
    }

    {
        DebugManagerStateRestore restorer;
        DebugManager.flags.DecompressInL3ForImage2dFromBuffer.set(0);

        auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
        imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex(), false);
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E, surfaceState.getAuxiliarySurfaceMode());
        EXPECT_EQ(0u, surfaceState.getDecompressInL3());
        EXPECT_EQ(0u, surfaceState.getMemoryCompressionEnable());
        EXPECT_EQ(RENDER_SURFACE_STATE::MEMORY_COMPRESSION_TYPE::MEMORY_COMPRESSION_TYPE_MEDIA_COMPRESSION, surfaceState.getMemoryCompressionType());
        EXPECT_EQ(bufferCompressionFormat, surfaceState.getCompressionFormat());
    }

    clReleaseMemObject(imageDesc.mem_object);
}

XE_HPG_CORETEST_F(CmdsProgrammingTestsXeHpgCore, givenDecompressInL3ForImage2dFromBufferEnabledWhenProgrammingStateForImage1dFromCompressedBufferThenCorrectFlagsAreSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    MockContext context;
    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    imageDesc.image_height = 1;
    imageDesc.image_width = 256;

    imageDesc.mem_object = clCreateBuffer(&context, CL_MEM_READ_WRITE, imageDesc.image_height * imageDesc.image_width, nullptr, &retVal);

    auto gmm = new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gmm->isCompressionEnabled = true;

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);
    buffer->getGraphicsAllocation(0)->setGmm(gmm, 0);

    auto surfaceFormat = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
                                                      CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    for (auto &decompressInL3 : ::testing::Bool()) {
        DebugManagerStateRestore restorer;
        DebugManager.flags.DecompressInL3ForImage2dFromBuffer.set(decompressInL3);

        auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
        imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex(), false);
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E, surfaceState.getAuxiliarySurfaceMode());
        EXPECT_EQ(0u, surfaceState.getDecompressInL3());
        EXPECT_EQ(0u, surfaceState.getMemoryCompressionEnable());
        EXPECT_EQ(RENDER_SURFACE_STATE::MEMORY_COMPRESSION_TYPE::MEMORY_COMPRESSION_TYPE_MEDIA_COMPRESSION, surfaceState.getMemoryCompressionType());
    }

    clReleaseMemObject(imageDesc.mem_object);
}

XE_HPG_CORETEST_F(CmdsProgrammingTestsXeHpgCore, givenDecompressInL3ForImage2dFromBufferEnabledWhenProgrammingStateForImage2dFromNotCompressedBufferThenCorrectFlagsAreSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    MockContext context;
    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    imageDesc.mem_object = clCreateBuffer(&context, CL_MEM_READ_WRITE, imageDesc.image_height * imageDesc.image_width, nullptr, &retVal);

    auto gmm = new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gmm->isCompressionEnabled = false;

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);
    buffer->getGraphicsAllocation(0)->setGmm(gmm, 0);

    auto surfaceFormat = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
                                                      CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    for (auto &decompressInL3 : ::testing::Bool()) {
        DebugManagerStateRestore restorer;
        DebugManager.flags.DecompressInL3ForImage2dFromBuffer.set(decompressInL3);

        auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
        imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex(), false);
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, surfaceState.getAuxiliarySurfaceMode());
        EXPECT_EQ(0u, surfaceState.getDecompressInL3());
        EXPECT_EQ(0u, surfaceState.getMemoryCompressionEnable());
        EXPECT_EQ(RENDER_SURFACE_STATE::MEMORY_COMPRESSION_TYPE::MEMORY_COMPRESSION_TYPE_MEDIA_COMPRESSION, surfaceState.getMemoryCompressionType());
    }

    clReleaseMemObject(imageDesc.mem_object);
}

XE_HPG_CORETEST_F(CmdsProgrammingTestsXeHpgCore, givenDecompressInL3ForImage2dFromBufferEnabledWhenProgrammingStateForImage2dFromMediaCompressedBufferThenCorrectFlagsAreSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    MockContext context;
    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    imageDesc.mem_object = clCreateBuffer(&context, CL_MEM_READ_WRITE, imageDesc.image_height * imageDesc.image_width, nullptr, &retVal);

    auto gmm = new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gmm->isCompressionEnabled = true;
    gmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);
    buffer->getGraphicsAllocation(0)->setGmm(gmm, 0);

    auto surfaceFormat = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
                                                      CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    for (auto &decompressInL3 : ::testing::Bool()) {
        DebugManagerStateRestore restorer;
        DebugManager.flags.DecompressInL3ForImage2dFromBuffer.set(decompressInL3);

        auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
        imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex(), false);
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, surfaceState.getAuxiliarySurfaceMode());
        EXPECT_EQ(0u, surfaceState.getDecompressInL3());
        EXPECT_EQ(1u, surfaceState.getMemoryCompressionEnable());
        EXPECT_EQ(RENDER_SURFACE_STATE::MEMORY_COMPRESSION_TYPE::MEMORY_COMPRESSION_TYPE_MEDIA_COMPRESSION, surfaceState.getMemoryCompressionType());
    }

    clReleaseMemObject(imageDesc.mem_object);
}

using PreambleCfeState = PreambleFixture;

HWTEST2_F(PreambleCfeState, givenXehpAndDisabledFusedEuWhenCfeStateProgrammedThenFusedEuDispatchSetToTrue, IsXeHpgCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.fusedEuEnabled = false;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, hwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(false, false, false, false, hwInfo);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, hwInfo, 0u, 0, 0, streamProperties, nullptr);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_TRUE(cfeState->getFusedEuDispatch());
}

HWTEST2_F(PreambleCfeState, givenXehpEnabledFusedEuAndDisableFusedDispatchFromKernelWhenCfeStateProgrammedThenFusedEuDispatchSetToFalse, IsXeHpgCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.CFEFusedEUDispatch.set(0);

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.fusedEuEnabled = true;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, hwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(false, true, false, false, hwInfo);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, hwInfo, 0u, 0, 0, streamProperties, nullptr);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_FALSE(cfeState->getFusedEuDispatch());
}

HWTEST2_F(PreambleCfeState, givenXehpAndEnabledFusedEuWhenCfeStateProgrammedThenFusedEuDispatchSetToFalse, IsXeHpgCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.fusedEuEnabled = true;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, hwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(false, false, false, false, hwInfo);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, hwInfo, 0u, 0, 0, streamProperties, nullptr);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_FALSE(cfeState->getFusedEuDispatch());
}
