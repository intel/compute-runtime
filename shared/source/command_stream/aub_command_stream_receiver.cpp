/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/options.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/release_helper/release_helper.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace NEO {
AubCommandStreamReceiverCreateFunc aubCommandStreamReceiverFactory[IGFX_MAX_CORE] = {};

std::string AUBCommandStreamReceiver::createFullFilePath(const HardwareInfo &hwInfo, const std::string &filename, uint32_t rootDeviceIndex) {
    std::string hwPrefix = hardwarePrefix[hwInfo.platform.eProductFamily];

    // Generate the full filename
    const auto &gtSystemInfo = hwInfo.gtSystemInfo;
    std::stringstream strfilename;
    auto subDevicesCount = GfxCoreHelper::getSubDevicesCount(&hwInfo);
    uint32_t subSlicesPerSlice = gtSystemInfo.SubSliceCount / gtSystemInfo.SliceCount;
    strfilename << hwPrefix << "_";
    std::stringstream strExtendedFileName;

    strExtendedFileName << filename;
    if (debugManager.flags.GenerateAubFilePerProcessId.get()) {
        strExtendedFileName << "_PID_" << SysCalls::getProcessId();
    }
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    const auto deviceConfig = AubHelper::getDeviceConfigString(releaseHelper.get(), subDevicesCount, gtSystemInfo.SliceCount, subSlicesPerSlice, gtSystemInfo.MaxEuPerSubSlice);
    strfilename << deviceConfig << "_" << rootDeviceIndex << "_" << strExtendedFileName.str() << ".aub";

    // clean-up any fileName issues because of the file system incompatibilities
    auto fileName = strfilename.str();
    for (char &i : fileName) {
        i = i == '/' ? '_' : i;
    }

    std::string filePath(folderAUB);
    if (debugManager.flags.AUBDumpCaptureDirPath.get() != "unk") {
        filePath.assign(debugManager.flags.AUBDumpCaptureDirPath.get());
    }

    filePath.append(Os::fileSeparator);
    filePath.append(fileName);

    return filePath;
}

CommandStreamReceiver *AUBCommandStreamReceiver::create(const std::string &baseName,
                                                        bool standalone,
                                                        ExecutionEnvironment &executionEnvironment,
                                                        uint32_t rootDeviceIndex,
                                                        const DeviceBitfield deviceBitfield) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    std::string filePath = AUBCommandStreamReceiver::createFullFilePath(*hwInfo, baseName, rootDeviceIndex);
    if (debugManager.flags.AUBDumpCaptureFileName.get() != "unk") {
        filePath.assign(debugManager.flags.AUBDumpCaptureFileName.get());
    }

    if (hwInfo->platform.eRenderCoreFamily >= IGFX_MAX_CORE) {
        DEBUG_BREAK_IF(!false);
        return nullptr;
    }

    auto pCreate = aubCommandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily];
    return pCreate ? pCreate(filePath, standalone, executionEnvironment, rootDeviceIndex, deviceBitfield) : nullptr;
}
} // namespace NEO

namespace AubMemDump {
using CmdServicesMemTraceMemoryCompare = AubMemDump::CmdServicesMemTraceMemoryCompare;
using CmdServicesMemTraceMemoryWrite = AubMemDump::CmdServicesMemTraceMemoryWrite;
using CmdServicesMemTraceRegisterPoll = AubMemDump::CmdServicesMemTraceRegisterPoll;
using CmdServicesMemTraceRegisterWrite = AubMemDump::CmdServicesMemTraceRegisterWrite;
using CmdServicesMemTraceVersion = AubMemDump::CmdServicesMemTraceVersion;
} // namespace AubMemDump
