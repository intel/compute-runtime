/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

#include <iomanip>

namespace NEO {

static const char *spirvWithVersion = "SPIR-V_1.2 ";

size_t Device::getMaxParameterSizeFromIGC() const {
    CompilerInterface *compilerInterface = getCompilerInterface();
    if (nullptr != compilerInterface) {
        auto igcFtrWa = compilerInterface->getIgcFeaturesAndWorkarounds(*this);
        return igcFtrWa->GetMaxOCLParamSize();
    }
    return 0;
}

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
        double percentOfGlobalMemoryAvailable = getPercentOfGlobalMemoryAvailable();
        deviceInfo.globalMemSize = std::min(deviceInfo.globalMemSize, static_cast<uint64_t>(4 * GB * percentOfGlobalMemoryAvailable));
        deviceInfo.addressBits = 32;
        deviceInfo.force32BitAddressess = is64bit;
    }

    deviceInfo.globalMemSize = alignDown(deviceInfo.globalMemSize, MemoryConstants::pageSize);
    deviceInfo.maxMemAllocSize = std::min(deviceInfo.globalMemSize, deviceInfo.maxMemAllocSize); // if globalMemSize was reduced for 32b

    uint32_t subDeviceCount = HwHelper::getSubDevicesCount(&getHardwareInfo());

    if (((NEO::ImplicitScalingHelper::isImplicitScalingEnabled(
            getDeviceBitfield(), true))) &&
        (!isSubDevice()) && (subDeviceCount > 1)) {
        deviceInfo.maxMemAllocSize = deviceInfo.globalMemSize;
    }

    if (!areSharedSystemAllocationsAllowed()) {
        deviceInfo.maxMemAllocSize = ApiSpecificConfig::getReducedMaxAllocSize(deviceInfo.maxMemAllocSize);
        deviceInfo.maxMemAllocSize = std::min(deviceInfo.maxMemAllocSize, hwHelper.getMaxMemAllocSize());
    }

    // Some specific driver model configurations may impose additional limitations
    auto driverModelMaxMemAlloc = std::numeric_limits<size_t>::max();
    if (this->executionEnvironment->rootDeviceEnvironments[0]->osInterface) {
        driverModelMaxMemAlloc = this->executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->getMaxMemAllocSize();
    }
    deviceInfo.maxMemAllocSize = std::min<std::uint64_t>(driverModelMaxMemAlloc, deviceInfo.maxMemAllocSize);

    deviceInfo.profilingTimerResolution = getProfilingTimerResolution();
    if (DebugManager.flags.OverrideProfilingTimerResolution.get() != -1) {
        deviceInfo.profilingTimerResolution = static_cast<double>(DebugManager.flags.OverrideProfilingTimerResolution.get());
        deviceInfo.outProfilingTimerClock = static_cast<size_t>(1000000000.0 / deviceInfo.profilingTimerResolution);
    } else {
        deviceInfo.outProfilingTimerClock = static_cast<size_t>(getProfilingTimerClock());
    }

    deviceInfo.outProfilingTimerResolution = static_cast<size_t>(deviceInfo.profilingTimerResolution);

    constexpr uint64_t maxPixelSize = 16;
    deviceInfo.imageMaxBufferSize = static_cast<size_t>(deviceInfo.maxMemAllocSize / maxPixelSize);

    deviceInfo.maxNumEUsPerSubSlice = 0;
    deviceInfo.numThreadsPerEU = 0;
    auto simdSizeUsed = DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get()
                            ? CommonConstants::maximalSimdSize
                            : hwHelper.getMinimalSIMDSize();

    deviceInfo.maxNumEUsPerSubSlice = (systemInfo.EuCountPerPoolMin == 0 || hwInfo.featureTable.flags.ftrPooledEuEnabled == 0)
                                          ? (systemInfo.EUCount / systemInfo.SubSliceCount)
                                          : systemInfo.EuCountPerPoolMin;
    if (systemInfo.DualSubSliceCount != 0) {
        deviceInfo.maxNumEUsPerDualSubSlice = (systemInfo.EuCountPerPoolMin == 0 || hwInfo.featureTable.flags.ftrPooledEuEnabled == 0)
                                                  ? (systemInfo.EUCount / systemInfo.DualSubSliceCount)
                                                  : systemInfo.EuCountPerPoolMin;

    } else {
        deviceInfo.maxNumEUsPerDualSubSlice = deviceInfo.maxNumEUsPerSubSlice;
    }
    deviceInfo.numThreadsPerEU = systemInfo.ThreadCount / systemInfo.EUCount;
    deviceInfo.threadsPerEUConfigs = hwHelper.getThreadsPerEUConfigs();
    auto maxWS = hwInfoConfig->getMaxThreadsForWorkgroupInDSSOrSS(hwInfo, static_cast<uint32_t>(deviceInfo.maxNumEUsPerSubSlice), static_cast<uint32_t>(deviceInfo.maxNumEUsPerDualSubSlice)) * simdSizeUsed;

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
    if (DebugManager.flags.OverrideSlmSize.get() != -1) {
        deviceInfo.localMemSize = DebugManager.flags.OverrideSlmSize.get() * KB;
    }

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

    std::stringstream deviceName;

    deviceName << this->getDeviceName(hwInfo);
    deviceName << " [0x" << std::hex << std::setw(4) << std::setfill('0') << hwInfo.platform.usDeviceID << "]";

    deviceInfo.name = deviceName.str();

    size_t maxParameterSizeFromIgc = getMaxParameterSizeFromIGC();
    if (maxParameterSizeFromIgc > 0) {
        deviceInfo.maxParameterSize = maxParameterSizeFromIgc;
    }
}

} // namespace NEO
