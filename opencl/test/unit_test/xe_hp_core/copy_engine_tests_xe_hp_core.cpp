/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct BlitXE_HP_CORETests : public ::testing::Test {
    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }

        DebugManager.flags.RenderCompressedBuffersEnabled.set(true);
        DebugManager.flags.EnableLocalMemory.set(true);

        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.featureTable.ftrBcsInfo = 1;
        hwInfo.capabilityTable.blitterOperationsSupported = true;

        clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    }

    std::optional<uint32_t> flushBcsTask(CommandStreamReceiver *csr, const BlitProperties &blitProperties, bool blocking, Device &device) {
        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);

        return csr->flushBcsTask(blitPropertiesContainer, blocking, false, device);
    }

    std::unique_ptr<MockClDevice> clDevice;
    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    DebugManagerStateRestore debugRestorer;
};

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenCompressedBufferWhenProgrammingBltCommandThenSetCompressionFields) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto bufferCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled = true;
    auto bufferNotCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled = false;

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

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenDebugFlagSetWhenCompressionEnabledThenForceCompressionFormat) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    uint32_t compressionFormat = 3;
    DebugManager.flags.ForceBufferCompressionFormat.set(compressionFormat);

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto bufferCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled = true;
    auto bufferNotCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled = false;

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

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenBufferWhenProgrammingBltCommandThenSetMocs) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForCopy(buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     0, 0, {1, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

    flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
    EXPECT_NE(nullptr, bltCmd);

    auto mocs = clDevice->getRootDeviceEnvironment().getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);

    EXPECT_EQ(mocs, bltCmd->getDestinationMOCS());
    EXPECT_EQ(mocs, bltCmd->getSourceMOCS());
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenBufferWhenProgrammingBltCommandThenSetMocsToValueOfDebugKey) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideBlitterMocs.set(0u);
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForCopy(buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     0, 0, {1, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

    flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
    EXPECT_NE(nullptr, bltCmd);

    EXPECT_EQ(0u, bltCmd->getDestinationMOCS());
    EXPECT_EQ(0u, bltCmd->getSourceMOCS());
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenCompressedBufferWhenResolveBlitIsCalledThenProgramSpecialOperationMode) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection::AuxToNonAux,
                                                                               buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()), csr->getClearColorAllocation());

    flushBcsTask(csr, blitProperties, false, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
    EXPECT_NE(nullptr, bltCmd);

    EXPECT_EQ(XY_COPY_BLT::SPECIAL_MODE_OF_OPERATION::SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE, bltCmd->getSpecialModeofOperation());
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenCompressedBufferWhenNonAuxToAuxBlitIsCalledThenDontProgramSourceCompression) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection::NonAuxToAux,
                                                                               buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()), csr->getClearColorAllocation());

    flushBcsTask(csr, blitProperties, false, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
    EXPECT_NE(nullptr, bltCmd);

    EXPECT_EQ(XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE, bltCmd->getSourceCompressionEnable());
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, given2dBlitCommandWhenDispatchingThenSetValidSurfaceType) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto allocation = buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex());

    size_t offset = 0;
    {
        // 1D
        auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, allocation,
                                                                         0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());
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
                                                                         0, 0, {(2 * BlitterConstants::maxBlitWidth) + 1, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());
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

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenBufferWhenProgrammingBltCommandThenSetTargetMemory) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto bufferInSystemPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_FORCE_HOST_MEMORY_INTEL, 2048, nullptr, retVal));
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));
    auto bufferInLocalPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));

    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }

    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenBufferWhenProgrammingBltCommandThenSetTargetMemoryInCpuAccesingLocalMemoryMode) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(1);
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    PLATFORM platform = clDevice->getHardwareInfo().platform;
    const auto &hwInfoConfig = *HwInfoConfig::get(platform.eProductFamily);
    const bool isXeHPRev0 = (platform.eProductFamily == IGFX_XE_HP_SDV) &&
                            (hwInfoConfig.getSteppingFromHwRevId(clDevice->getHardwareInfo()) < REVISION_B);

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto bufferInSystemPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_FORCE_HOST_MEMORY_INTEL, 2048, nullptr, retVal));
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));
    auto bufferInLocalPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));

    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        if (isXeHPRev0) {
            EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
        } else {
            EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        }
        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }

    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        if (isXeHPRev0) {
            EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
        } else {
            EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        }
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenBufferWhenProgrammingBltCommandThenSetTargetMemoryToSystemWhenDebugKeyPresent) {
    DebugManagerStateRestore restorer;

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto bufferInSystemPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_FORCE_HOST_MEMORY_INTEL, 2048, nullptr, retVal));
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));
    auto bufferInLocalPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));

    DebugManager.flags.OverrideBlitterTargetMemory.set(0u);
    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
    DebugManager.flags.OverrideBlitterTargetMemory.set(1u);
    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    }

    DebugManager.flags.OverrideBlitterTargetMemory.set(2u);
    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenBufferWhenProgrammingBltCommandAndRevisionB0ThenSetTargetMemory) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    HardwareInfo *hwInfo = clDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo);

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto bufferInSystemPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_FORCE_HOST_MEMORY_INTEL, 2048, nullptr, retVal));
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));
    auto bufferInLocalPool = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getMemoryPool()));

    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }

    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferInLocalPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferInSystemPool->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, csr->getClearColorAllocation());

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenDebugFlagSetWhenCompressionIsUsedThenForceCompressionEnableFields) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto blitCmd = FamilyType::cmdInitXyCopyBlt;
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    auto gmm = std::make_unique<MockGmm>(clDevice->getGmmHelper());
    gmm->isCompressionEnabled = true;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocation;
    properties.dstAllocation = &mockAllocation;
    properties.clearColorAllocation = &mockAllocation;

    {
        DebugManager.flags.ForceCompressionDisabledForCompressedBlitCopies.set(1);

        BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(properties, blitCmd, *clDevice->getExecutionEnvironment()->rootDeviceEnvironments[clDevice->getRootDeviceIndex()]);

        EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
        EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);

        EXPECT_EQ(blitCmd.getSourceCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
        EXPECT_EQ(blitCmd.getSourceAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    }

    {
        DebugManager.flags.ForceCompressionDisabledForCompressedBlitCopies.set(0);

        BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(properties, blitCmd, *clDevice->getExecutionEnvironment()->rootDeviceEnvironments[clDevice->getRootDeviceIndex()]);

        EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
        EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);

        EXPECT_EQ(blitCmd.getSourceCompressionEnable(), XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
        EXPECT_EQ(blitCmd.getSourceAuxiliarysurfacemode(), XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    }
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenDebugFlagForClearColorNotSetWhenProgrammingBlitCommandForBuffersThenClearColorAddressIsNotProgrammed) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    DebugManagerStateRestore restore;
    DebugManager.flags.UseClearColorAllocationForBlitter.set(false);
    auto blitCmd = FamilyType::cmdInitXyCopyBlt;
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    MockGraphicsAllocation mockAllocation;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocation;
    properties.dstAllocation = &mockAllocation;
    properties.clearColorAllocation = &mockAllocation;

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(properties, blitCmd, *clDevice->getExecutionEnvironment()->rootDeviceEnvironments[clDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getSourceClearValueEnable(), XY_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_DISABLE);
    EXPECT_EQ(0u, blitCmd.getSourceClearAddressLow());
    EXPECT_EQ(0u, blitCmd.getSourceClearAddressHigh());

    EXPECT_EQ(blitCmd.getDestinationClearValueEnable(), XY_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_DISABLE);
    EXPECT_EQ(0u, blitCmd.getDestinationClearAddressLow());
    EXPECT_EQ(0u, blitCmd.getDestinationClearAddressHigh());
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenDebugFlagForClearColorSetWhenProgrammingBlitCommandForBuffersThenClearColorAddressIsProgrammed) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    DebugManagerStateRestore restore;
    DebugManager.flags.UseClearColorAllocationForBlitter.set(true);
    auto blitCmd = FamilyType::cmdInitXyCopyBlt;
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    MockGraphicsAllocation mockAllocation;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocation;
    properties.dstAllocation = &mockAllocation;
    properties.clearColorAllocation = &mockAllocation;

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(properties, blitCmd, *clDevice->getExecutionEnvironment()->rootDeviceEnvironments[clDevice->getRootDeviceIndex()]);

    auto addressLow = static_cast<uint32_t>(mockAllocation.getGpuAddress() & 0xFFFFFFFFULL);
    auto addressHigh = static_cast<uint32_t>(mockAllocation.getGpuAddress() >> 32);

    EXPECT_EQ(blitCmd.getSourceClearValueEnable(), XY_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_ENABLE);
    EXPECT_EQ(addressLow, blitCmd.getSourceClearAddressLow());
    EXPECT_EQ(addressHigh, blitCmd.getSourceClearAddressHigh());

    EXPECT_EQ(blitCmd.getDestinationClearValueEnable(), XY_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_ENABLE);
    EXPECT_EQ(addressLow, blitCmd.getDestinationClearAddressLow());
    EXPECT_EQ(addressHigh, blitCmd.getDestinationClearAddressHigh());
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenDebugFlagForClearColorNotSetWhenProgrammingBlitCommandForImagesThenClearColorAddressIsNotProgrammed) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    DebugManagerStateRestore restore;
    DebugManager.flags.UseClearColorAllocationForBlitter.set(false);
    auto blitCmd = FamilyType::cmdInitXyCopyBlt;

    MockGraphicsAllocation mockAllocation;
    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.srcAllocation = &mockAllocation;
    properties.dstAllocation = &mockAllocation;
    properties.clearColorAllocation = &mockAllocation;
    uint32_t srcSlicePitch = 0u;
    uint32_t dstSlicePitch = 0u;

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, blitCmd, *clDevice->getExecutionEnvironment()->rootDeviceEnvironments[clDevice->getRootDeviceIndex()], srcSlicePitch, dstSlicePitch);

    EXPECT_EQ(blitCmd.getSourceClearValueEnable(), XY_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_DISABLE);
    EXPECT_EQ(0u, blitCmd.getSourceClearAddressLow());
    EXPECT_EQ(0u, blitCmd.getSourceClearAddressHigh());

    EXPECT_EQ(blitCmd.getDestinationClearValueEnable(), XY_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_DISABLE);
    EXPECT_EQ(0u, blitCmd.getDestinationClearAddressLow());
    EXPECT_EQ(0u, blitCmd.getDestinationClearAddressHigh());
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenDebugFlagForClearColorSetWhenProgrammingBlitCommandForImagesThenClearColorAddressIsProgrammed) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    DebugManagerStateRestore restore;
    DebugManager.flags.UseClearColorAllocationForBlitter.set(true);
    auto blitCmd = FamilyType::cmdInitXyCopyBlt;

    MockGraphicsAllocation mockAllocation;
    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.srcAllocation = &mockAllocation;
    properties.dstAllocation = &mockAllocation;
    properties.clearColorAllocation = &mockAllocation;
    uint32_t srcSlicePitch = 0;
    uint32_t dstSlicePitch = 0;

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, blitCmd, *clDevice->getExecutionEnvironment()->rootDeviceEnvironments[clDevice->getRootDeviceIndex()], srcSlicePitch, dstSlicePitch);

    auto addressLow = static_cast<uint32_t>(mockAllocation.getGpuAddress() & 0xFFFFFFFFULL);
    auto addressHigh = static_cast<uint32_t>(mockAllocation.getGpuAddress() >> 32);

    EXPECT_EQ(blitCmd.getSourceClearValueEnable(), XY_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_ENABLE);
    EXPECT_EQ(addressLow, blitCmd.getSourceClearAddressLow());
    EXPECT_EQ(addressHigh, blitCmd.getSourceClearAddressHigh());

    EXPECT_EQ(blitCmd.getDestinationClearValueEnable(), XY_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_ENABLE);
    EXPECT_EQ(addressLow, blitCmd.getDestinationClearAddressLow());
    EXPECT_EQ(addressHigh, blitCmd.getDestinationClearAddressHigh());
}

XE_HP_CORE_TEST_F(BlitXE_HP_CORETests, givenCommandQueueWhenAskingForCacheFlushOnBcsThenReturnTrue) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(clDevice.get());
    cl_int retVal = CL_SUCCESS;
    auto cmdQ = std::unique_ptr<CommandQueue>(CommandQueue::create(&context, clDevice.get(), nullptr, false, retVal));

    auto pHwQ = static_cast<CommandQueueHw<FamilyType> *>(cmdQ.get());

    EXPECT_TRUE(pHwQ->isCacheFlushForBcsRequired());
}
