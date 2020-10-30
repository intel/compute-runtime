/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

#include <iomanip>

namespace NEO {

static const char *spirvWithVersion = "SPIR-V_1.2 ";

void Device::initializeCaps() {
    auto &hwInfo = getHardwareInfo();
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto addressing32bitAllowed = is64bit;

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    bool ocl21FeaturesEnabled = hwInfo.capabilityTable.supportsOcl21Features;
    if (DebugManager.flags.ForceOCLVersion.get() != 0) {
        ocl21FeaturesEnabled = (DebugManager.flags.ForceOCLVersion.get() == 21);
    }
    if (DebugManager.flags.ForceOCL21FeaturesSupport.get() != -1) {
        ocl21FeaturesEnabled = DebugManager.flags.ForceOCL21FeaturesSupport.get();
    }
    if (ocl21FeaturesEnabled) {
        addressing32bitAllowed = false;
    }

    deviceInfo.vendorId = 0x8086;
    deviceInfo.maxReadImageArgs = 128;
    deviceInfo.maxWriteImageArgs = 128;
    deviceInfo.maxParameterSize = 2048;

    deviceInfo.addressBits = 64;
    deviceInfo.ilVersion = spirvWithVersion;

    //copy system info to prevent misaligned reads
    const auto systemInfo = hwInfo.gtSystemInfo;

    deviceInfo.globalMemCachelineSize = 64;

    uint32_t allSubDevicesMask = static_cast<uint32_t>(getDeviceBitfield().to_ulong());
    constexpr uint32_t singleSubDeviceMask = 1;

    deviceInfo.globalMemSize = getGlobalMemorySize(allSubDevicesMask);
    deviceInfo.maxMemAllocSize = getGlobalMemorySize(singleSubDeviceMask); // Allocation can be placed only on one SubDevice

    if (DebugManager.flags.Force32bitAddressing.get() || addressing32bitAllowed || is32bit) {
        deviceInfo.globalMemSize = std::min(deviceInfo.globalMemSize, static_cast<uint64_t>(4 * GB * 0.8));
        deviceInfo.addressBits = 32;
        deviceInfo.force32BitAddressess = is64bit;
    }

    deviceInfo.globalMemSize = alignDown(deviceInfo.globalMemSize, MemoryConstants::pageSize);
    deviceInfo.maxMemAllocSize = std::min(deviceInfo.globalMemSize, deviceInfo.maxMemAllocSize); // if globalMemSize was reduced for 32b

    // OpenCL 1.2 requires 128MB minimum
    deviceInfo.maxMemAllocSize = std::min(std::max(deviceInfo.maxMemAllocSize / 2, static_cast<uint64_t>(128llu * MB)), this->hardwareCapabilities.maxMemAllocSize);

    deviceInfo.profilingTimerResolution = getProfilingTimerResolution();
    deviceInfo.outProfilingTimerResolution = static_cast<size_t>(deviceInfo.profilingTimerResolution);

    constexpr uint64_t maxPixelSize = 16;
    deviceInfo.imageMaxBufferSize = static_cast<size_t>(deviceInfo.maxMemAllocSize / maxPixelSize);

    deviceInfo.maxNumEUsPerSubSlice = 0;
    deviceInfo.numThreadsPerEU = 0;
    auto simdSizeUsed = DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get()
                            ? CommonConstants::maximalSimdSize
                            : hwHelper.getMinimalSIMDSize();

    deviceInfo.maxNumEUsPerSubSlice = (systemInfo.EuCountPerPoolMin == 0 || hwInfo.featureTable.ftrPooledEuEnabled == 0)
                                          ? (systemInfo.EUCount / systemInfo.SubSliceCount)
                                          : systemInfo.EuCountPerPoolMin;
    deviceInfo.numThreadsPerEU = systemInfo.ThreadCount / systemInfo.EUCount;
    deviceInfo.threadsPerEUConfigs = hwHelper.getThreadsPerEUConfigs();
    auto maxWS = hwHelper.getMaxThreadsForWorkgroup(hwInfo, static_cast<uint32_t>(deviceInfo.maxNumEUsPerSubSlice)) * simdSizeUsed;

    maxWS = Math::prevPowerOfTwo(maxWS);
    deviceInfo.maxWorkGroupSize = std::min(maxWS, 1024u);

    if (DebugManager.flags.OverrideMaxWorkgroupSize.get() != -1) {
        deviceInfo.maxWorkGroupSize = DebugManager.flags.OverrideMaxWorkgroupSize.get();
    }

    deviceInfo.maxWorkItemSizes[0] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxWorkItemSizes[1] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxWorkItemSizes[2] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxSamplers = hwHelper.getMaxNumSamplers();

    deviceInfo.computeUnitsUsedForScratch = hwHelper.getComputeUnitsUsedForScratch(&hwInfo);
    deviceInfo.maxFrontEndThreads = HwHelper::getMaxThreadsForVfe(hwInfo);

    deviceInfo.localMemSize = hwInfo.capabilityTable.slmSize * KB;

    deviceInfo.imageSupport = hwInfo.capabilityTable.supportsImages;
    deviceInfo.image2DMaxWidth = 16384;
    deviceInfo.image2DMaxHeight = 16384;
    deviceInfo.image3DMaxDepth = 2048;
    deviceInfo.imageMaxArraySize = 2048;

    deviceInfo.printfBufferSize = 4 * MB;
    deviceInfo.maxClockFrequency = hwInfo.capabilityTable.maxRenderFrequency;

    deviceInfo.maxSubGroups = hwHelper.getDeviceSubGroupSizes();

    deviceInfo.vmeAvcSupportsPreemption = hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption;

    NEO::Debugger *debugger = getRootDeviceEnvironment().debugger.get();
    deviceInfo.debuggerActive = false;
    if (debugger) {
        UNRECOVERABLE_IF(!debugger->isLegacy());
        deviceInfo.debuggerActive = static_cast<NEO::SourceLevelDebugger *>(debugger)->isDebuggerActive();
    }

    if (deviceInfo.debuggerActive) {
        this->preemptionMode = PreemptionMode::Disabled;
    }

    deviceInfo.sharedSystemAllocationsSupport = hwInfoConfig->getSharedSystemMemCapabilities();
    if (DebugManager.flags.EnableSharedSystemUsmSupport.get() != -1) {
        deviceInfo.sharedSystemAllocationsSupport = DebugManager.flags.EnableSharedSystemUsmSupport.get();
    }

    std::stringstream deviceName;

    deviceName << this->getDeviceName(hwInfo);
    deviceName << " [0x" << std::hex << std::setw(4) << std::setfill('0') << hwInfo.platform.usDeviceID << "]";

    deviceInfo.name = deviceName.str();
}

} // namespace NEO
