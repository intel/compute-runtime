/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/timestamp_packet_container.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
using namespace NEO;

struct BlitXeHpgCoreTests : public ::testing::Test {
    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }

        debugManager.flags.RenderCompressedBuffersEnabled.set(true);
        debugManager.flags.EnableLocalMemory.set(true);
        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = true;

        clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    }

    std::optional<TaskCountType> flushBcsTask(CommandStreamReceiver *csr, const BlitProperties &blitProperties, bool blocking, Device &device) {
        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);

        return csr->flushBcsTask(blitPropertiesContainer, blocking, device);
    }

    std::unique_ptr<MockClDevice> clDevice;
    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    DebugManagerStateRestore debugRestorer;
};

XE_HPG_CORETEST_F(BlitXeHpgCoreTests, givenBufferWhenProgrammingBltCommandThenSetMocs) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForCopy(buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     0, 0, {1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

    flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
    EXPECT_NE(nullptr, bltCmd);

    auto mocs = clDevice->getRootDeviceEnvironment().getGmmHelper()->getUncachedMOCS();

    EXPECT_EQ(mocs, bltCmd->getDestinationMOCS());
    EXPECT_EQ(mocs, bltCmd->getSourceMOCS());
}

XE_HPG_CORETEST_F(BlitXeHpgCoreTests, givenBufferWhenProgrammingBltCommandThenSetMocsToValueOfDebugKey) {
    DebugManagerStateRestore restorer;
    uint32_t expectedMocs = 0;
    debugManager.flags.OverrideBlitterMocs.set(expectedMocs);
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForCopy(buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     0, 0, {1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

    flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
    EXPECT_NE(nullptr, bltCmd);

    EXPECT_EQ(expectedMocs, bltCmd->getDestinationMOCS());
    EXPECT_EQ(expectedMocs, bltCmd->getSourceMOCS());
}

XE_HPG_CORETEST_F(BlitXeHpgCoreTests, given2dBlitCommandWhenDispatchingThenSetValidSurfaceType) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto allocation = buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex());
    MockGraphicsAllocation clearColorAlloc;

    size_t offset = 0;
    {
        // 1D
        auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, allocation,
                                                                         0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
        flushBcsTask(csr, blitProperties, false, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D, bltCmd->getDestinationSurfaceType());
        EXPECT_EQ(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D, bltCmd->getSourceSurfaceType());

        EXPECT_EQ(bltCmd->getSourceSurfaceWidth(), bltCmd->getDestinationX2CoordinateRight());
        EXPECT_EQ(bltCmd->getSourceSurfaceHeight(), bltCmd->getDestinationY2CoordinateBottom());

        EXPECT_EQ(bltCmd->getDestinationSurfaceWidth(), bltCmd->getDestinationX2CoordinateRight());
        EXPECT_EQ(bltCmd->getDestinationSurfaceHeight(), bltCmd->getDestinationY2CoordinateBottom());

        offset = csr->commandStream.getUsed();
    }

    {
        // 2D
        auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, allocation,
                                                                         0, 0, {(2 * BlitterConstants::maxBlitWidth) + 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
        flushBcsTask(csr, blitProperties, false, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, bltCmd->getDestinationSurfaceType());
        EXPECT_EQ(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, bltCmd->getSourceSurfaceType());

        EXPECT_EQ(bltCmd->getSourceSurfaceWidth(), bltCmd->getDestinationX2CoordinateRight());
        EXPECT_EQ(bltCmd->getSourceSurfaceHeight(), bltCmd->getDestinationY2CoordinateBottom());

        EXPECT_EQ(bltCmd->getDestinationSurfaceWidth(), bltCmd->getDestinationX2CoordinateRight());
        EXPECT_EQ(bltCmd->getDestinationSurfaceHeight(), bltCmd->getDestinationY2CoordinateBottom());
    }
}

XE_HPG_CORETEST_F(BlitXeHpgCoreTests, givenBufferWhenProgrammingBltCommandThenSetTargetMemory) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto bufferInSystemPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_FORCE_HOST_MEMORY_INTEL, 2048, nullptr, retVal));
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));
    auto bufferInLocalPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));

    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    }

    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    }
}

XE_HPG_CORETEST_F(BlitXeHpgCoreTests, givenBufferWhenProgrammingBltCommandThenSetTargetMemoryToSystemWhenDebugKeyPresent) {
    DebugManagerStateRestore restorer;

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto bufferInSystemPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_FORCE_HOST_MEMORY_INTEL, 2048, nullptr, retVal));
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));
    auto bufferInLocalPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));

    debugManager.flags.OverrideBlitterTargetMemory.set(0u);
    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
    debugManager.flags.OverrideBlitterTargetMemory.set(1u);
    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    }

    debugManager.flags.OverrideBlitterTargetMemory.set(2u);
    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    }
}

HWTEST2_F(BlitXeHpgCoreTests, givenBufferWhenProgrammingBltCommandAndRevisionB0ThenSetTargetMemory, IsDG2) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    HardwareInfo *hwInfo = clDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &productHelper = clDevice->getProductHelper();
    hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto bufferInSystemPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_FORCE_HOST_MEMORY_INTEL, 2048, nullptr, retVal));
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));
    auto bufferInLocalPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));

    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    }

    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    }
}

XE_HPG_CORETEST_F(BlitXeHpgCoreTests, givenCompressedBufferWhenResolveBlitIsCalledThenProgramSpecialOperationMode) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL, 2048, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection::auxToNonAux,
                                                                               buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                               &clearColorAlloc);

    flushBcsTask(csr, blitProperties, false, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
    EXPECT_NE(nullptr, bltCmd);

    EXPECT_EQ(XY_COPY_BLT::SPECIAL_MODE_OF_OPERATION::SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE, bltCmd->getSpecialModeOfOperation());
}

XE_HPG_CORETEST_F(BlitXeHpgCoreTests, givenCompressedBufferWhenNonAuxToAuxBlitIsCalledThenDontProgramSourceCompression) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL, 2048, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection::nonAuxToAux,
                                                                               buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                               &clearColorAlloc);

    flushBcsTask(csr, blitProperties, false, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
    EXPECT_NE(nullptr, bltCmd);

    EXPECT_EQ(XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE, bltCmd->getSourceCompressionEnable());
}

XE_HPG_CORETEST_F(BlitXeHpgCoreTests, givenCompressedBufferWhenProgrammingBltCommandThenSetCompressionFields) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto bufferCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL, 2048, nullptr, retVal));
    bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm()->setCompressionEnabled(true);
    auto bufferNotCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_UNCOMPRESSED_HINT_INTEL, 2048, nullptr, retVal));

    auto notCompressedGmm = bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm();
    if (notCompressedGmm) {
        notCompressedGmm->setCompressionEnabled(false);
    }

    auto gmmHelper = clDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmHelper();
    uint32_t compressionFormat = gmmHelper->getClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);

    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
        EXPECT_EQ(bltCmd->getDestinationAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_NONE);
        EXPECT_EQ(bltCmd->getDestinationCompressionFormat(), 0u);
        EXPECT_EQ(bltCmd->getSourceCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
        EXPECT_EQ(bltCmd->getSourceAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        EXPECT_EQ(bltCmd->getSourceCompressionFormat(), compressionFormat);
    }

    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());
        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
        EXPECT_EQ(bltCmd->getDestinationAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        EXPECT_EQ(bltCmd->getDestinationCompressionFormat(), compressionFormat);
        EXPECT_EQ(bltCmd->getSourceCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
        EXPECT_EQ(bltCmd->getSourceAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_NONE);
        EXPECT_EQ(bltCmd->getSourceCompressionFormat(), 0u);
    }
}

XE_HPG_CORETEST_F(BlitXeHpgCoreTests, givenDebugFlagSetWhenCompressionEnabledThenForceCompressionFormat) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    uint32_t compressionFormat = 3;
    debugManager.flags.ForceBufferCompressionFormat.set(compressionFormat);

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto bufferCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL, 2048, nullptr, retVal));
    bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm()->setCompressionEnabled(true);
    auto bufferNotCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_UNCOMPRESSED_HINT_INTEL, 2048, nullptr, retVal));

    auto uncompressedGmm = bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm();
    if (uncompressedGmm) {
        uncompressedGmm->setCompressionEnabled(false);
    }

    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
        EXPECT_EQ(bltCmd->getDestinationAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_NONE);
        EXPECT_EQ(bltCmd->getDestinationCompressionFormat(), 0u);
        EXPECT_EQ(bltCmd->getSourceCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
        EXPECT_EQ(bltCmd->getSourceAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        EXPECT_EQ(bltCmd->getSourceCompressionFormat(), compressionFormat);
    }

    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());
        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
        EXPECT_EQ(bltCmd->getDestinationAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        EXPECT_EQ(bltCmd->getDestinationCompressionFormat(), compressionFormat);
        EXPECT_EQ(bltCmd->getSourceCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
        EXPECT_EQ(bltCmd->getSourceAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_NONE);
        EXPECT_EQ(bltCmd->getSourceCompressionFormat(), 0u);
    }
}
