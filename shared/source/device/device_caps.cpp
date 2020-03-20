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

namespace NEO {

static const char *spirvVersion = "SPIR-V_1.2 ";

void Device::initializeCaps() {
    auto &hwInfo = getHardwareInfo();
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto addressing32bitAllowed = is64bit;

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    deviceInfo.ilVersion = "";
    auto enabledClVersion = hwInfo.capabilityTable.clVersionSupport;
    if (DebugManager.flags.ForceOCLVersion.get() != 0) {
        enabledClVersion = DebugManager.flags.ForceOCLVersion.get();
    }
    switch (enabledClVersion) {
    case 21:
        deviceInfo.ilVersion = spirvVersion;
        addressing32bitAllowed = false;
        break;
    case 20:
        addressing32bitAllowed = false;
        break;
    }

    deviceInfo.vendorId = 0x8086;
    deviceInfo.maxReadImageArgs = 128;
    deviceInfo.maxWriteImageArgs = 128;
    deviceInfo.maxParameterSize = 1024;

    deviceInfo.addressBits = 64;

    //copy system info to prevent misaligned reads
    const auto systemInfo = hwInfo.gtSystemInfo;

    deviceInfo.globalMemCachelineSize = 64;

    deviceInfo.globalMemSize = getMemoryManager()->isLocalMemorySupported(this->getRootDeviceIndex())
                                   ? getMemoryManager()->getLocalMemorySize(this->getRootDeviceIndex())
                                   : getMemoryManager()->getSystemSharedMemory(this->getRootDeviceIndex());
    deviceInfo.globalMemSize = std::min(deviceInfo.globalMemSize, getMemoryManager()->getMaxApplicationAddress() + 1);
    deviceInfo.globalMemSize = static_cast<uint64_t>(static_cast<double>(deviceInfo.globalMemSize) * 0.8);

    if (DebugManager.flags.Force32bitAddressing.get() || addressing32bitAllowed || is32bit) {
        deviceInfo.globalMemSize = std::min(deviceInfo.globalMemSize, static_cast<uint64_t>(4 * GB * 0.8));
        deviceInfo.addressBits = 32;
        deviceInfo.force32BitAddressess = is64bit;
    }

    deviceInfo.globalMemSize = alignDown(deviceInfo.globalMemSize, MemoryConstants::pageSize);

    deviceInfo.profilingTimerResolution = getProfilingTimerResolution();
    deviceInfo.outProfilingTimerResolution = static_cast<size_t>(deviceInfo.profilingTimerResolution);

    deviceInfo.maxOnDeviceQueues = 1;

    // OpenCL 1.2 requires 128MB minimum
    deviceInfo.maxMemAllocSize = std::min(std::max(deviceInfo.globalMemSize / 2, static_cast<uint64_t>(128llu * MB)), this->hardwareCapabilities.maxMemAllocSize);

    static const int maxPixelSize = 16;
    deviceInfo.imageMaxBufferSize = static_cast<size_t>(deviceInfo.maxMemAllocSize / maxPixelSize);

    deviceInfo.maxNumEUsPerSubSlice = 0;
    deviceInfo.numThreadsPerEU = 0;
    auto simdSizeUsed = DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get() ? 32u : hwHelper.getMinimalSIMDSize();

    deviceInfo.maxNumEUsPerSubSlice = (systemInfo.EuCountPerPoolMin == 0 || hwInfo.featureTable.ftrPooledEuEnabled == 0)
                                          ? (systemInfo.EUCount / systemInfo.SubSliceCount)
                                          : systemInfo.EuCountPerPoolMin;
    deviceInfo.numThreadsPerEU = systemInfo.ThreadCount / systemInfo.EUCount;
    auto maxWS = hwHelper.getMaxThreadsForWorkgroup(hwInfo, static_cast<uint32_t>(deviceInfo.maxNumEUsPerSubSlice)) * simdSizeUsed;

    maxWS = Math::prevPowerOfTwo(maxWS);
    deviceInfo.maxWorkGroupSize = std::min(maxWS, 1024u);

    if (DebugManager.flags.OverrideMaxWorkgroupSize.get() != -1) {
        deviceInfo.maxWorkGroupSize = DebugManager.flags.OverrideMaxWorkgroupSize.get();
    }

    deviceInfo.maxWorkItemSizes[0] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxWorkItemSizes[1] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxWorkItemSizes[2] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxSamplers = 16;

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

    deviceInfo.debuggerActive = (getRootDeviceEnvironment().debugger.get()) ? getRootDeviceEnvironment().debugger->isDebuggerActive() : false;
    if (deviceInfo.debuggerActive) {
        this->preemptionMode = PreemptionMode::Disabled;
    }

    deviceInfo.sharedSystemAllocationsSupport = hwInfoConfig->getSharedSystemMemCapabilities();
    if (DebugManager.flags.EnableSharedSystemUsmSupport.get() != -1) {
        deviceInfo.sharedSystemAllocationsSupport = DebugManager.flags.EnableSharedSystemUsmSupport.get();
    }
}

} // namespace NEO
