/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_info_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"

#include <iomanip>

namespace NEO {

static const char *spirvWithVersion = "SPIR-V_1.5 SPIR-V_1.4 SPIR-V_1.3 SPIR-V_1.2 SPIR-V_1.1 SPIR-V_1.0 ";

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
    auto addressing32bitAllowed = is64bit;

    auto &productHelper = this->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
    auto &gfxCoreHelper = this->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    auto releaseHelper = this->getRootDeviceEnvironment().getReleaseHelper();

    bool ocl21FeaturesEnabled = HwInfoHelper::checkIfOcl21FeaturesEnabledOrEnforced(hwInfo);
    if (ocl21FeaturesEnabled) {
        addressing32bitAllowed = false;
    }

    deviceInfo.vendorId = 0x8086;
    deviceInfo.maxReadImageArgs = 128;
    deviceInfo.maxWriteImageArgs = 128;
    deviceInfo.maxParameterSize = 2048;

    deviceInfo.addressBits = 64;
    deviceInfo.ilVersion = spirvWithVersion;

    // copy system info to prevent misaligned reads
    const auto systemInfo = hwInfo.gtSystemInfo;

    deviceInfo.globalMemCachelineSize = productHelper.getCacheLineSize();

    uint32_t allSubDevicesMask = static_cast<uint32_t>(getDeviceBitfield().to_ulong());
    constexpr uint32_t singleSubDeviceMask = 1;

    deviceInfo.globalMemSize = getGlobalMemorySize(allSubDevicesMask);
    deviceInfo.maxMemAllocSize = getGlobalMemorySize(singleSubDeviceMask); // Allocation can be placed only on one SubDevice

    if (debugManager.flags.Force32bitAddressing.get() || addressing32bitAllowed || is32bit) {
        double percentOfGlobalMemoryAvailable = getPercentOfGlobalMemoryAvailable();
        deviceInfo.globalMemSize = std::min(deviceInfo.globalMemSize, static_cast<uint64_t>(4 * MemoryConstants::gigaByte * percentOfGlobalMemoryAvailable));
        deviceInfo.addressBits = 32;
        deviceInfo.force32BitAddresses = is64bit;
    }

    deviceInfo.globalMemSize = alignDown(deviceInfo.globalMemSize, MemoryConstants::pageSize);
    deviceInfo.maxMemAllocSize = std::min(deviceInfo.globalMemSize, deviceInfo.maxMemAllocSize); // if globalMemSize was reduced for 32b

    uint32_t subDeviceCount = gfxCoreHelper.getSubDevicesCount(&getHardwareInfo());
    auto &rootDeviceEnvironment = this->getRootDeviceEnvironment();
    bool platformImplicitScaling = gfxCoreHelper.platformSupportsImplicitScaling(rootDeviceEnvironment);
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();

    if ((NEO::ImplicitScalingHelper::isImplicitScalingEnabled(
            getDeviceBitfield(), platformImplicitScaling)) &&
        (!isSubDevice()) && (subDeviceCount > 1)) {
        deviceInfo.maxMemAllocSize = deviceInfo.globalMemSize;
    } else if (!compilerProductHelper.isForceToStatelessRequired()) {
        deviceInfo.maxMemAllocSize = ApiSpecificConfig::getReducedMaxAllocSize(deviceInfo.maxMemAllocSize);
        deviceInfo.maxMemAllocSize = std::min(deviceInfo.maxMemAllocSize, gfxCoreHelper.getMaxMemAllocSize());
    }

    // Some specific driver model configurations may impose additional limitations
    auto driverModelMaxMemAlloc = std::numeric_limits<size_t>::max();
    if (this->executionEnvironment->rootDeviceEnvironments[0]->osInterface) {
        driverModelMaxMemAlloc = this->executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->getMaxMemAllocSize();
    }
    deviceInfo.maxMemAllocSize = std::min<std::uint64_t>(driverModelMaxMemAlloc, deviceInfo.maxMemAllocSize);

    if (debugManager.flags.OverrideMaxMemAllocSizeMb.get() != -1) {
        deviceInfo.maxMemAllocSize = static_cast<uint64_t>(debugManager.flags.OverrideMaxMemAllocSizeMb.get()) * MemoryConstants::megaByte;
    }

    deviceInfo.profilingTimerResolution = getProfilingTimerResolution();
    if (debugManager.flags.OverrideProfilingTimerResolution.get() != -1) {
        deviceInfo.profilingTimerResolution = static_cast<double>(debugManager.flags.OverrideProfilingTimerResolution.get());
        deviceInfo.outProfilingTimerClock = static_cast<size_t>(1000000000.0 / deviceInfo.profilingTimerResolution);
    } else {
        deviceInfo.outProfilingTimerClock = static_cast<size_t>(getProfilingTimerClock());
    }

    deviceInfo.outProfilingTimerResolution = static_cast<size_t>(deviceInfo.profilingTimerResolution);

    constexpr uint64_t maxPixelSize = 16;
    deviceInfo.imageMaxBufferSize = static_cast<size_t>(deviceInfo.maxMemAllocSize / maxPixelSize);

    deviceInfo.maxNumEUsPerSubSlice = 0;
    deviceInfo.numThreadsPerEU = 0;
    auto simdSizeUsed = debugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get()
                            ? CommonConstants::maximalSimdSize
                            : gfxCoreHelper.getMinimalSIMDSize();

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

    if (releaseHelper) {
        deviceInfo.threadsPerEUConfigs = releaseHelper->getThreadsPerEUConfigs(deviceInfo.numThreadsPerEU);
    }
    auto maxWS = productHelper.getMaxThreadsForWorkgroupInDSSOrSS(hwInfo, static_cast<uint32_t>(deviceInfo.maxNumEUsPerSubSlice), static_cast<uint32_t>(deviceInfo.maxNumEUsPerDualSubSlice)) * simdSizeUsed;

    maxWS = Math::prevPowerOfTwo(maxWS);
    deviceInfo.maxWorkGroupSize = gfxCoreHelper.overrideMaxWorkGroupSize(maxWS);

    if (debugManager.flags.OverrideMaxWorkgroupSize.get() != -1) {
        deviceInfo.maxWorkGroupSize = debugManager.flags.OverrideMaxWorkgroupSize.get();
    }

    deviceInfo.maxWorkItemSizes[0] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxWorkItemSizes[1] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxWorkItemSizes[2] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxSamplers = productHelper.getMaxNumSamplers();

    deviceInfo.computeUnitsUsedForScratch = gfxCoreHelper.getComputeUnitsUsedForScratch(this->getRootDeviceEnvironment());
    deviceInfo.maxFrontEndThreads = gfxCoreHelper.getMaxThreadsForVfe(hwInfo);

    deviceInfo.localMemSize = hwInfo.capabilityTable.maxProgrammableSlmSize * MemoryConstants::kiloByte;

    deviceInfo.imageSupport = hwInfo.capabilityTable.supportsImages;
    deviceInfo.image2DMaxWidth = 16384;
    deviceInfo.image2DMaxHeight = 16384;
    deviceInfo.image3DMaxDepth = 2048;
    deviceInfo.imageMaxArraySize = 2048;

    deviceInfo.printfBufferSize = 4 * MemoryConstants::megaByte;
    deviceInfo.maxClockFrequency = hwInfo.capabilityTable.maxRenderFrequency;

    deviceInfo.maxSubGroups = gfxCoreHelper.getDeviceSubGroupSizes();

    deviceInfo.name = this->getDeviceName();

    size_t maxParameterSizeFromIgc = getMaxParameterSizeFromIGC();
    if (maxParameterSizeFromIgc > 0) {
        deviceInfo.maxParameterSize = maxParameterSizeFromIgc;
    }
}

} // namespace NEO
