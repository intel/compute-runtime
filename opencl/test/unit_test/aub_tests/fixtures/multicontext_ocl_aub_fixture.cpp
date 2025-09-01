/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_ocl_aub_fixture.h"

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/tests_configuration.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {

void MulticontextOclAubFixture::setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool enableCompression) {
    MulticontextAubFixture::setUp(numberOfTiles, enabledCommandStreamers, enableCompression);

    cl_int retVal = CL_SUCCESS;

    auto createCommandQueueForEngine = [&](uint32_t tileNumber, size_t engineFamily, size_t engineIndex) {
        cl_queue_properties properties[] = {CL_QUEUE_FAMILY_INTEL, engineFamily, CL_QUEUE_INDEX_INTEL, engineIndex, 0};
        auto clQueue = clCreateCommandQueueWithProperties(context.get(), tileDevices[tileNumber], properties, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        return std::unique_ptr<CommandQueue>(castToObject<CommandQueue>(clQueue));
    };

    {
        cl_device_id deviceId = rootDevice;
        ClDeviceVector clDeviceVector{&deviceId, 1};
        if (numberOfTiles > 1) {
            for (uint32_t i = 0; i < numberOfTiles; i++) {
                clDeviceVector.push_back(rootDevice->getNearestGenericSubDevice(i));
            }
        }
        context.reset(MockContext::create<MockContext>(nullptr, clDeviceVector, nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        this->svmAllocsManager = context->getSVMAllocsManager();
    }

    commandQueues.resize(numberOfTiles);
    for (uint32_t tile = 0; tile < numberOfTiles; tile++) {
        EXPECT_NE(nullptr, tileDevices[tile]);
        if (EnabledCommandStreamers::single == enabledCommandStreamers) {
            auto engineGroupType = isRenderEngineSupported ? EngineGroupType::renderCompute : EngineGroupType::compute;
            auto familyQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType);
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, familyQueue, 0));
        } else if (EnabledCommandStreamers::dual == enabledCommandStreamers) {
            auto ccsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
            if (isRenderEngineSupported) {
                auto rcsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::renderCompute);
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, rcsQueue, 0));
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 0));
            } else if (isCcs1Supported) {
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 0));
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 1));
            } else {
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 0));
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 0));
            }
        } else if (EnabledCommandStreamers::all == enabledCommandStreamers) {
            if (isRenderEngineSupported) {
                auto rcsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::renderCompute);
                commandQueues[tile].push_back(createCommandQueueForEngine(tile, rcsQueue, 0));
            }
            auto ccsQueue = tileDevices[tile]->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 0));
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 1));
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 2));
            commandQueues[tile].push_back(createCommandQueueForEngine(tile, ccsQueue, 3));
        }
    }

    {
        cl_int retVal = CL_SUCCESS;
        cl_device_id deviceId = rootDevice;
        multiTileDefaultContext.reset(MockContext::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

CommandStreamReceiver *MulticontextOclAubFixture::getGpgpuCsr(uint32_t tile, uint32_t engine) {
    return &(commandQueues[tile][engine]->getGpgpuCommandStreamReceiver());
}

void MulticontextOclAubFixture::createDevices(const HardwareInfo &hwInfo, uint32_t numTiles) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;

    auto device = MockClDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, rootDeviceIndex);
    auto platform = constructPlatform(device->getExecutionEnvironment());
    initPlatform({device});
    rootDevice = static_cast<MockClDevice *>(platform->getClDevice(0u));

    EXPECT_EQ(rootDeviceIndex, rootDevice->getRootDeviceIndex());

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        tileDevices.push_back(rootDevice->getNearestGenericSubDevice(tile));
        EXPECT_NE(nullptr, tileDevices[tile]);
    }
}

} // namespace NEO
