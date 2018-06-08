/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/device/driver_info.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/platform/extensions.h"
#include "runtime/sharings/sharing_factory.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

#include "CL/cl_ext_intel.h"
#include "driver_version.h"

#include <algorithm>

namespace OCLRT {
extern const char *familyName[];

static std::string name(128, '\0');
static std::string vendor = "Intel(R) Corporation";
static std::string profile = "FULL_PROFILE";
static std::string spirVersions = "1.2 ";
static const char *spirvVersion = "SPIR-V_1.0 ";
#define QTR(a) #a
#define TOSTR(b) QTR(b)
static std::string driverVersion = TOSTR(NEO_DRIVER_VERSION);

const char *builtInKernels = ""; // the "always available" (extension-independent) builtin kernels

static constexpr cl_device_fp_config defaultFpFlags = static_cast<cl_device_fp_config>(CL_FP_ROUND_TO_NEAREST |
                                                                                       CL_FP_ROUND_TO_ZERO |
                                                                                       CL_FP_ROUND_TO_INF |
                                                                                       CL_FP_INF_NAN |
                                                                                       CL_FP_DENORM |
                                                                                       CL_FP_FMA);

bool Device::getEnabled64kbPages() {
    if (DebugManager.flags.Enable64kbpages.get() == -1) {
        // assign value according to os and hw configuration
        return OSInterface::osEnabled64kbPages && hwInfo.capabilityTable.ftr64KBpages;
    } else {
        // force debug settings
        return (DebugManager.flags.Enable64kbpages.get() != 0);
    }
};

void Device::setupFp64Flags() {
    if (DebugManager.flags.OverrideDefaultFP64Settings.get() == -1) {
        if (hwInfo.capabilityTable.ftrSupportsFP64) {
            deviceExtensions += "cl_khr_fp64 ";
        }

        deviceInfo.singleFpConfig = static_cast<cl_device_fp_config>(
            hwInfo.capabilityTable.ftrSupports64BitMath
                ? CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT
                : 0);

        deviceInfo.doubleFpConfig = hwInfo.capabilityTable.ftrSupportsFP64
                                        ? defaultFpFlags
                                        : 0;
    } else {
        if (DebugManager.flags.OverrideDefaultFP64Settings.get() == 1) {
            deviceExtensions += "cl_khr_fp64 ";
            deviceInfo.singleFpConfig = static_cast<cl_device_fp_config>(CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
            deviceInfo.doubleFpConfig = defaultFpFlags;
        }
    }
}

void Device::initializeCaps() {
    deviceExtensions.clear();
    deviceExtensions.append(deviceExtensionsList);
    // Add our graphics family name to the device name
    auto addressing32bitAllowed = is32BitOsAllocatorAvailable;
    if (is32bit) {
        addressing32bitAllowed = false;
    }
    std::string tempName = "Intel(R) ";
    tempName += familyName[hwInfo.pPlatform->eRenderCoreFamily];
    tempName += " HD Graphics NEO";

    DEBUG_BREAK_IF(tempName.size() > name.size());
    name = tempName;

    driverVersion = TOSTR(NEO_DRIVER_VERSION);

    if (driverInfo) {
        name.assign(driverInfo.get()->getDeviceName(tempName).c_str());
        driverVersion.assign(driverInfo.get()->getVersion(driverVersion).c_str());
    }

    HardwareCapabilities hwCaps = {0};
    auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);
    hwHelper.setupHardwareCapabilities(&hwCaps);

    deviceInfo.name = name.c_str();
    deviceInfo.driverVersion = driverVersion.c_str();

    setupFp64Flags();

    deviceInfo.vendor = vendor.c_str();
    deviceInfo.profile = profile.c_str();
    deviceInfo.ilVersion = "";
    enabledClVersion = hwInfo.capabilityTable.clVersionSupport;
    if (DebugManager.flags.ForceOCLVersion.get() != 0) {
        enabledClVersion = DebugManager.flags.ForceOCLVersion.get();
    }
    switch (enabledClVersion) {
    case 21:
        deviceInfo.clVersion = "OpenCL 2.1 NEO ";
        deviceInfo.clCVersion = "OpenCL C 2.0 ";
        deviceInfo.ilVersion = spirvVersion;
        addressing32bitAllowed = false;
        break;
    case 20:
        deviceInfo.clVersion = "OpenCL 2.0 NEO ";
        deviceInfo.clCVersion = "OpenCL C 2.0 ";
        addressing32bitAllowed = false;
        break;
    case 12:
    default:
        deviceInfo.clVersion = "OpenCL 1.2 NEO ";
        deviceInfo.clCVersion = "OpenCL C 1.2 ";
        break;
    }
    deviceInfo.platformLP = (hwInfo.capabilityTable.clVersionSupport == 12) ? true : false;
    deviceInfo.cpuCopyAllowed = true;
    deviceInfo.spirVersions = spirVersions.c_str();

    if (enabledClVersion >= 21) {
        deviceInfo.independentForwardProgress = true;
        deviceExtensions += "cl_khr_subgroups ";
        deviceExtensions += "cl_khr_il_program ";
    } else {
        deviceInfo.independentForwardProgress = false;
    }

    if (enabledClVersion >= 20) {
        deviceExtensions += "cl_khr_mipmap_image cl_khr_mipmap_image_writes ";
    }

    if (DebugManager.flags.EnableNV12.get()) {
        deviceExtensions += "cl_intel_planar_yuv ";
        deviceInfo.nv12Extension = true;
    }
    if (DebugManager.flags.EnablePackedYuv.get()) {
        deviceExtensions += "cl_intel_packed_yuv ";
        deviceInfo.packedYuvExtension = true;
    }
    if (DebugManager.flags.EnableIntelVme.get()) {
        deviceExtensions += "cl_intel_motion_estimation ";
        deviceInfo.vmeExtension = true;
    }
    if (DebugManager.flags.EnableIntelAdvancedVme.get()) {
        deviceExtensions += "cl_intel_advanced_motion_estimation ";
    }

    deviceExtensions += sharingFactory.getExtensions();

    simultaneousInterops = {0};
    appendOSExtensions(deviceExtensions);

    deviceInfo.deviceExtensions = deviceExtensions.c_str();

    exposedBuiltinKernels = builtInKernels;
    if (deviceExtensions.find("cl_intel_motion_estimation") != std::string::npos) {
        exposedBuiltinKernels.append("block_motion_estimate_intel;");
    }
    if (deviceExtensions.find("cl_intel_advanced_motion_estimation") != std::string::npos) {
        auto advVmeKernels = "block_advanced_motion_estimate_check_intel;block_advanced_motion_estimate_bidirectional_check_intel;";
        exposedBuiltinKernels.append(advVmeKernels);
    }

    deviceInfo.builtInKernels = exposedBuiltinKernels.c_str();

    deviceInfo.deviceType = CL_DEVICE_TYPE_GPU;
    deviceInfo.vendorId = 0x8086;
    deviceInfo.endianLittle = 1;
    deviceInfo.hostUnifiedMemory = CL_TRUE;
    deviceInfo.deviceAvailable = CL_TRUE;
    deviceInfo.compilerAvailable = CL_TRUE;
    deviceInfo.preferredVectorWidthChar = 16;
    deviceInfo.preferredVectorWidthShort = 8;
    deviceInfo.preferredVectorWidthInt = 4;
    deviceInfo.preferredVectorWidthLong = 1;
    deviceInfo.preferredVectorWidthFloat = 1;
    deviceInfo.preferredVectorWidthDouble = 1;
    deviceInfo.preferredVectorWidthHalf = 8;
    deviceInfo.nativeVectorWidthChar = 16;
    deviceInfo.nativeVectorWidthShort = 8;
    deviceInfo.nativeVectorWidthInt = 4;
    deviceInfo.nativeVectorWidthLong = 1;
    deviceInfo.nativeVectorWidthFloat = 1;
    deviceInfo.nativeVectorWidthDouble = 1;
    deviceInfo.nativeVectorWidthHalf = 8;
    deviceInfo.maxReadImageArgs = 128;
    deviceInfo.maxWriteImageArgs = 128;
    deviceInfo.maxReadWriteImageArgs = 128;
    deviceInfo.maxParameterSize = 1024;
    deviceInfo.executionCapabilities = CL_EXEC_KERNEL;

    deviceInfo.addressBits = 64;

    //copy system info to prevent misaligned reads
    const auto systemInfo = *hwInfo.pSysInfo;

    deviceInfo.globalMemCachelineSize = 64;
    deviceInfo.globalMemCacheSize = systemInfo.L3BankCount * 128 * KB;

    deviceInfo.globalMemSize = (cl_ulong)getMemoryManager()->getSystemSharedMemory();
    deviceInfo.globalMemSize = std::min(deviceInfo.globalMemSize, (cl_ulong)(getMemoryManager()->getMaxApplicationAddress() + 1));
    deviceInfo.globalMemSize = (cl_ulong)((double)deviceInfo.globalMemSize * 0.8);

    if (DebugManager.flags.Force32bitAddressing.get() || addressing32bitAllowed || is32bit) {
        deviceInfo.globalMemSize = std::min(deviceInfo.globalMemSize, (uint64_t)(4 * GB * 0.8));
        deviceInfo.addressBits = 32;
        deviceInfo.force32BitAddressess = is64bit;
    }

    deviceInfo.globalMemSize = alignDown(deviceInfo.globalMemSize, MemoryConstants::pageSize);

    deviceInfo.globalMemCacheType = CL_READ_WRITE_CACHE;
    deviceInfo.profilingTimerResolution = getProfilingTimerResolution();
    deviceInfo.outProfilingTimerResolution = static_cast<size_t>(deviceInfo.profilingTimerResolution);
    deviceInfo.memBaseAddressAlign = 1024;
    deviceInfo.minDataTypeAlignSize = 128;

    deviceInfo.maxOnDeviceEvents = 1024;
    deviceInfo.maxOnDeviceQueues = 1;
    deviceInfo.queueOnDeviceMaxSize = 64 * MB;
    deviceInfo.queueOnDevicePreferredSize = 128 * KB;
    deviceInfo.queueOnDeviceProperties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;

    deviceInfo.preferredInteropUserSync = 1u;

    // OpenCL 1.2 requires 128MB minimum
    auto maxMemAllocSize = std::max((uint64_t)(deviceInfo.globalMemSize / 2), (uint64_t)(128 * MB));
    deviceInfo.maxMemAllocSize = std::min(maxMemAllocSize, hwCaps.maxMemAllocSize);

    deviceInfo.maxConstantBufferSize = deviceInfo.maxMemAllocSize;

    static const int maxPixelSize = 16;
    deviceInfo.imageMaxBufferSize = static_cast<size_t>(deviceInfo.maxMemAllocSize / maxPixelSize);

    deviceInfo.maxWorkItemDimensions = 3;

    deviceInfo.maxComputUnits = systemInfo.EUCount;
    deviceInfo.maxConstantArgs = 8;
    deviceInfo.maxNumEUsPerSubSlice = 0;
    deviceInfo.numThreadsPerEU = 0;
    auto simdSizeUsed = DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get() ? 32 : 8;

    deviceInfo.maxNumEUsPerSubSlice = (systemInfo.EuCountPerPoolMin == 0 || hwInfo.pSkuTable->ftrPooledEuEnabled == 0)
                                          ? (systemInfo.EUCount / systemInfo.SubSliceCount)
                                          : systemInfo.EuCountPerPoolMin;
    deviceInfo.numThreadsPerEU = systemInfo.ThreadCount / systemInfo.EUCount;
    auto maxWkgSize = DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get() ? 1024u : 256u;
    auto maxWS = deviceInfo.maxNumEUsPerSubSlice * deviceInfo.numThreadsPerEU * simdSizeUsed;

    maxWS = Math::prevPowerOfTwo(uint32_t(maxWS));
    deviceInfo.maxWorkGroupSize = std::min(uint32_t(maxWS), maxWkgSize);

    DEBUG_BREAK_IF(!DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get() && deviceInfo.maxWorkGroupSize > 256);

    // calculate a maximum number of subgroups in a workgroup (for the required SIMD size)
    deviceInfo.maxNumOfSubGroups = static_cast<uint32_t>(deviceInfo.maxWorkGroupSize / simdSizeUsed);

    deviceInfo.maxWorkItemSizes[0] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxWorkItemSizes[1] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxWorkItemSizes[2] = deviceInfo.maxWorkGroupSize;
    deviceInfo.maxSamplers = 16;

    deviceInfo.singleFpConfig |= defaultFpFlags;

    deviceInfo.halfFpConfig = defaultFpFlags;

    printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "hwInfo: {%d, %d}: (%d, %d, %d)\n",
                     systemInfo.EUCount,
                     systemInfo.ThreadCount,
                     systemInfo.MaxEuPerSubSlice,
                     systemInfo.MaxSlicesSupported,
                     systemInfo.MaxSubSlicesSupported);

    deviceInfo.computeUnitsUsedForScratch = hwHelper.getComputeUnitsUsedForScratch(&hwInfo);

    printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "computeUnitsUsedForScratch: %d\n", deviceInfo.computeUnitsUsedForScratch);

    deviceInfo.localMemType = CL_LOCAL;
    deviceInfo.localMemSize = 64 << 10;

    deviceInfo.imageSupport = CL_TRUE;
    deviceInfo.image2DMaxWidth = 16384;
    deviceInfo.image2DMaxHeight = 16384;
    deviceInfo.image3DMaxWidth = hwCaps.image3DMaxWidth;
    deviceInfo.image3DMaxHeight = hwCaps.image3DMaxHeight;
    deviceInfo.image3DMaxDepth = 2048;
    deviceInfo.imageMaxArraySize = 2048;

    // cl_khr_image2d_from_buffer
    deviceInfo.imagePitchAlignment = 4;
    deviceInfo.imageBaseAddressAlignment = 4;
    deviceInfo.maxPipeArgs = 16;
    deviceInfo.pipeMaxPacketSize = 1024;
    deviceInfo.pipeMaxActiveReservations = 1;
    deviceInfo.queueOnHostProperties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;

    deviceInfo.printfBufferSize = 4 * 1024 * 1024;
    deviceInfo.maxClockFrequency = hwInfo.capabilityTable.maxRenderFrequency;

    deviceInfo.maxSubGroups[0] = 8;
    deviceInfo.maxSubGroups[1] = 16;
    deviceInfo.maxSubGroups[2] = 32;

    deviceInfo.linkerAvailable = true;
    deviceInfo.svmCapabilities = hwInfo.capabilityTable.ftrSvm * CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;
    deviceInfo.svmCapabilities |= static_cast<cl_device_svm_capabilities>(
        hwInfo.capabilityTable.ftrSvm * hwInfo.capabilityTable.ftrSupportsCoherency *
        (CL_DEVICE_SVM_FINE_GRAIN_BUFFER | CL_DEVICE_SVM_ATOMICS));
    deviceInfo.preemptionSupported = false;
    deviceInfo.maxGlobalVariableSize = 64 * 1024;
    deviceInfo.globalVariablePreferredTotalSize = (size_t)deviceInfo.maxMemAllocSize;

    deviceInfo.planarYuvMaxWidth = 16384;
    deviceInfo.planarYuvMaxHeight = 16380;

    deviceInfo.vmeAvcSupportsPreemption = hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption;
    deviceInfo.vmeAvcSupportsTextureSampler = hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler;
    deviceInfo.vmeAvcVersion = CL_AVC_ME_VERSION_1_INTEL;
    deviceInfo.vmeVersion = CL_ME_VERSION_ADVANCED_VER_2_INTEL;
    deviceInfo.platformHostTimerResolution = getPlatformHostTimerResolution();

    deviceInfo.internalDriverVersion = CL_DEVICE_DRIVER_VERSION_INTEL_NEO1;
    deviceInfo.enabled64kbPages = getEnabled64kbPages();

    deviceInfo.preferredGlobalAtomicAlignment = MemoryConstants::cacheLineSize;
    deviceInfo.preferredLocalAtomicAlignment = MemoryConstants::cacheLineSize;
    deviceInfo.preferredPlatformAtomicAlignment = MemoryConstants::cacheLineSize;

    if (hwInfo.capabilityTable.sourceLevelDebuggerSupported) {
        deviceInfo.sourceLevelDebuggerActive = (sourceLevelDebugger) ? sourceLevelDebugger->isDebuggerActive() : false;
        if (deviceInfo.sourceLevelDebuggerActive) {
            this->preemptionMode = PreemptionMode::Disabled;
        }
    } else {
        deviceInfo.sourceLevelDebuggerActive = false;
    }
}
} // namespace OCLRT
