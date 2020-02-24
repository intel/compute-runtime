/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/libult/create_command_stream.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"
#include "opencl/source/command_stream/aub_command_stream_receiver.h"
#include "opencl/source/command_stream/create_command_stream_impl.h"
#include "opencl/source/command_stream/tbx_command_stream_receiver.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"

#include <cassert>

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();

    if (ultHwConfig.useHwCsr) {
        return createCommandStreamImpl(executionEnvironment, rootDeviceIndex);
    }

    auto funcCreate = commandStreamReceiverFactory[IGFX_MAX_CORE + hwInfo->platform.eRenderCoreFamily];
    if (funcCreate) {
        return funcCreate(false, executionEnvironment, rootDeviceIndex);
    }
    return nullptr;
}

bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    auto currentHwInfo = executionEnvironment.getHardwareInfo();
    if (currentHwInfo->platform.eProductFamily == IGFX_UNKNOWN && currentHwInfo->platform.eRenderCoreFamily == IGFX_UNKNOWN_CORE) {
        executionEnvironment.setHwInfo(platformDevices[0]);
    }
    if (ultHwConfig.useMockedGetDevicesFunc) {
        numDevicesReturned = DebugManager.flags.CreateMultipleRootDevices.get() != 0 ? DebugManager.flags.CreateMultipleRootDevices.get() : 1u;
        executionEnvironment.prepareRootDeviceEnvironments(static_cast<uint32_t>(numDevicesReturned));
        executionEnvironment.calculateMaxOsContextCount();
        executionEnvironment.initializeMemoryManager();
        return ultHwConfig.mockedGetDevicesFuncResult;
    }

    return getDevicesImpl(numDevicesReturned, executionEnvironment);
}
} // namespace NEO
