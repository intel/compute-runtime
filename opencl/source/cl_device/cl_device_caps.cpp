/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/device/device_info.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/sharings/sharing_factory.h"

#include "driver_version.h"

#include <sstream>
#include <string>

namespace NEO {
extern const char *familyName[];

static std::string vendor = "Intel(R) Corporation";
static std::string profile = "FULL_PROFILE";
static std::string spirVersions = "1.2 ";
static std::string spirvName = "SPIR-V";
const char *latestConformanceVersionPassed = "v2022-04-22-00";
#define QTR(a) #a
#define TOSTR(b) QTR(b)
static std::string driverVersion = TOSTR(NEO_OCL_DRIVER_VERSION);

static constexpr cl_device_fp_config defaultFpFlags = static_cast<cl_device_fp_config>(CL_FP_ROUND_TO_NEAREST |
                                                                                       CL_FP_ROUND_TO_ZERO |
                                                                                       CL_FP_ROUND_TO_INF |
                                                                                       CL_FP_INF_NAN |
                                                                                       CL_FP_DENORM |
                                                                                       CL_FP_FMA);

void ClDevice::setupFp64Flags() {
    auto &hwInfo = getHardwareInfo();

    if (DebugManager.flags.OverrideDefaultFP64Settings.get() == 1) {
        deviceExtensions += "cl_khr_fp64 ";
        deviceInfo.singleFpConfig = static_cast<cl_device_fp_config>(CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
        deviceInfo.doubleFpConfig = defaultFpFlags;
    } else if (DebugManager.flags.OverrideDefaultFP64Settings.get() == -1) {
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
    }
}

void ClDevice::initializeCaps() {
    auto &hwInfo = getHardwareInfo();
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto &sharedDeviceInfo = getSharedDeviceInfo();
    deviceExtensions.clear();
    deviceExtensions.append(deviceExtensionsList);

    driverVersion = TOSTR(NEO_OCL_DRIVER_VERSION);

    if (DebugManager.flags.OverrideDeviceName.get() != "unk") {
        name.assign(DebugManager.flags.OverrideDeviceName.get().c_str());
    } else {
        name = getClDeviceName(hwInfo);
        if (driverInfo) {
            name.assign(driverInfo->getDeviceName(name).c_str());
        }
    }

    if (driverInfo) {
        driverVersion.assign(driverInfo->getVersion(driverVersion).c_str());
        sharingFactory.verifyExtensionSupport(driverInfo.get());
    }

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    deviceInfo.name = name.c_str();
    deviceInfo.driverVersion = driverVersion.c_str();

    setupFp64Flags();

    deviceInfo.vendor = vendor.c_str();
    deviceInfo.profile = profile.c_str();
    enabledClVersion = hwInfo.capabilityTable.clVersionSupport;
    ocl21FeaturesEnabled = hwInfo.capabilityTable.supportsOcl21Features;
    if (DebugManager.flags.ForceOCLVersion.get() != 0) {
        enabledClVersion = DebugManager.flags.ForceOCLVersion.get();
        ocl21FeaturesEnabled = (enabledClVersion == 21);
    }
    if (DebugManager.flags.ForceOCL21FeaturesSupport.get() != -1) {
        ocl21FeaturesEnabled = DebugManager.flags.ForceOCL21FeaturesSupport.get();
    }
    switch (enabledClVersion) {
    case 30:
        deviceInfo.clVersion = "OpenCL 3.0 NEO ";
        deviceInfo.clCVersion = "OpenCL C 1.2 ";
        deviceInfo.numericClVersion = CL_MAKE_VERSION(3, 0, 0);
        break;
    case 21:
        deviceInfo.clVersion = "OpenCL 2.1 NEO ";
        deviceInfo.clCVersion = "OpenCL C 2.0 ";
        deviceInfo.numericClVersion = CL_MAKE_VERSION(2, 1, 0);
        break;
    case 12:
    default:
        deviceInfo.clVersion = "OpenCL 1.2 NEO ";
        deviceInfo.clCVersion = "OpenCL C 1.2 ";
        deviceInfo.numericClVersion = CL_MAKE_VERSION(1, 2, 0);
        break;
    }
    deviceInfo.latestConformanceVersionPassed = latestConformanceVersionPassed;
    initializeOpenclCAllVersions();
    deviceInfo.platformLP = (hwInfo.capabilityTable.supportsOcl21Features == false);
    deviceInfo.spirVersions = spirVersions.c_str();
    deviceInfo.ilsWithVersion[0].version = CL_MAKE_VERSION(1, 2, 0);
    strcpy_s(deviceInfo.ilsWithVersion[0].name, CL_NAME_VERSION_MAX_NAME_SIZE, spirvName.c_str());
    auto supportsVme = hwInfo.capabilityTable.supportsVme;
    auto supportsAdvancedVme = hwInfo.capabilityTable.supportsVme;

    deviceInfo.independentForwardProgress = hwInfo.capabilityTable.supportsIndependentForwardProgress;
    deviceInfo.maxNumOfSubGroups = 0;

    if (ocl21FeaturesEnabled) {

        auto simdSizeUsed = DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get()
                                ? CommonConstants::maximalSimdSize
                                : hwHelper.getMinimalSIMDSize();

        // calculate a maximum number of subgroups in a workgroup (for the required SIMD size)
        deviceInfo.maxNumOfSubGroups = static_cast<uint32_t>(sharedDeviceInfo.maxWorkGroupSize / simdSizeUsed);

        if (deviceInfo.independentForwardProgress) {
            deviceExtensions += "cl_khr_subgroups ";
        }

        if (supportsVme) {
            deviceExtensions += "cl_intel_spirv_device_side_avc_motion_estimation ";
        }
        if (hwInfo.capabilityTable.supportsMediaBlock) {
            deviceExtensions += "cl_intel_spirv_media_block_io ";
        }
        deviceExtensions += "cl_intel_spirv_subgroups ";
        deviceExtensions += "cl_khr_spirv_no_integer_wrap_decoration ";

        deviceExtensions += "cl_intel_unified_shared_memory ";
        if (hwInfo.capabilityTable.supportsImages) {
            deviceExtensions += "cl_khr_mipmap_image cl_khr_mipmap_image_writes ";
        }
    }

    if (DebugManager.flags.EnableNV12.get() && hwInfo.capabilityTable.supportsImages) {
        deviceExtensions += "cl_intel_planar_yuv ";
        deviceInfo.nv12Extension = true;
    }
    if (DebugManager.flags.EnablePackedYuv.get() && hwInfo.capabilityTable.supportsImages) {
        deviceExtensions += "cl_intel_packed_yuv ";
        deviceInfo.packedYuvExtension = true;
    }
    if (DebugManager.flags.EnableIntelVme.get() != -1) {
        supportsVme = !!DebugManager.flags.EnableIntelVme.get();
    }

    if (supportsVme) {
        deviceExtensions += "cl_intel_motion_estimation cl_intel_device_side_avc_motion_estimation ";
        deviceInfo.vmeExtension = true;
    }

    if (DebugManager.flags.EnableIntelAdvancedVme.get() != -1) {
        supportsAdvancedVme = !!DebugManager.flags.EnableIntelAdvancedVme.get();
    }
    if (supportsAdvancedVme) {
        deviceExtensions += "cl_intel_advanced_motion_estimation ";
    }

    if (hwInfo.capabilityTable.ftrSupportsInteger64BitAtomics) {
        deviceExtensions += "cl_khr_int64_base_atomics ";
        deviceExtensions += "cl_khr_int64_extended_atomics ";
    }

    if (hwInfo.capabilityTable.supportsImages) {
        deviceExtensions += "cl_khr_image2d_from_buffer ";
        deviceExtensions += "cl_khr_depth_images ";
        deviceExtensions += "cl_khr_3d_image_writes ";
    }

    if (hwInfo.capabilityTable.supportsMediaBlock) {
        deviceExtensions += "cl_intel_media_block_io ";
    }

    if (hwInfoConfig->isBFloat16ConversionSupported(hwInfo)) {
        deviceExtensions += "cl_intel_bfloat16_conversions ";
    }

    auto sharingAllowed = (getNumGenericSubDevices() <= 1u);
    if (sharingAllowed) {
        deviceExtensions += sharingFactory.getExtensions(driverInfo.get());
    }

    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue);

    if (driverInfo) {
        pciBusInfo = driverInfo->getPciBusInfo();
    }

    deviceInfo.pciBusInfo.pci_domain = pciBusInfo.pciDomain;
    deviceInfo.pciBusInfo.pci_bus = pciBusInfo.pciBus;
    deviceInfo.pciBusInfo.pci_device = pciBusInfo.pciDevice;
    deviceInfo.pciBusInfo.pci_function = pciBusInfo.pciFunction;

    if (isPciBusInfoValid()) {
        deviceExtensions += "cl_khr_pci_bus_info ";
    }

    deviceExtensions += hwHelper.getExtensions(hwInfo);
    deviceInfo.deviceExtensions = deviceExtensions.c_str();

    std::vector<std::string> exposedBuiltinKernelsVector;
    if (supportsVme) {
        exposedBuiltinKernelsVector.push_back("block_motion_estimate_intel");
    }
    if (supportsAdvancedVme) {
        exposedBuiltinKernelsVector.push_back("block_advanced_motion_estimate_check_intel");
        exposedBuiltinKernelsVector.push_back("block_advanced_motion_estimate_bidirectional_check_intel");
    }
    for (auto builtInKernel : exposedBuiltinKernelsVector) {
        exposedBuiltinKernels.append(builtInKernel);
        exposedBuiltinKernels.append(";");

        cl_name_version kernelNameVersion;
        kernelNameVersion.version = CL_MAKE_VERSION(1, 0, 0);
        strcpy_s(kernelNameVersion.name, CL_NAME_VERSION_MAX_NAME_SIZE, builtInKernel.c_str());
        deviceInfo.builtInKernelsWithVersion.push_back(kernelNameVersion);
    }
    deviceInfo.builtInKernels = exposedBuiltinKernels.c_str();

    deviceInfo.deviceType = CL_DEVICE_TYPE_GPU;
    deviceInfo.endianLittle = 1;
    deviceInfo.hostUnifiedMemory = (false == hwHelper.isLocalMemoryEnabled(hwInfo));
    deviceInfo.deviceAvailable = CL_TRUE;
    deviceInfo.compilerAvailable = CL_TRUE;
    deviceInfo.parentDevice = nullptr;
    deviceInfo.partitionMaxSubDevices = device.getNumSubDevices();
    if (deviceInfo.partitionMaxSubDevices > 0) {
        deviceInfo.partitionProperties[0] = CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN;
        deviceInfo.partitionProperties[1] = 0;
        deviceInfo.partitionAffinityDomain = CL_DEVICE_AFFINITY_DOMAIN_NUMA | CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE;
    } else {
        deviceInfo.partitionMaxSubDevices = 0;
        deviceInfo.partitionProperties[0] = 0;
        deviceInfo.partitionAffinityDomain = 0;
    }
    deviceInfo.partitionType[0] = 0;
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
    deviceInfo.maxReadWriteImageArgs = hwInfo.capabilityTable.supportsImages ? 128 : 0;
    deviceInfo.executionCapabilities = CL_EXEC_KERNEL;

    //copy system info to prevent misaligned reads
    const auto systemInfo = hwInfo.gtSystemInfo;

    const auto subDevicesCount = std::max(getNumGenericSubDevices(), 1u);
    deviceInfo.globalMemCacheSize = systemInfo.L3CacheSizeInKb * KB * subDevicesCount;
    deviceInfo.grfSize = hwInfo.capabilityTable.grfSize;

    deviceInfo.globalMemCacheType = CL_READ_WRITE_CACHE;
    deviceInfo.memBaseAddressAlign = 1024;
    deviceInfo.minDataTypeAlignSize = 128;

    deviceInfo.preferredInteropUserSync = 1u;

    // OpenCL 1.2 requires 128MB minimum

    deviceInfo.maxConstantBufferSize = sharedDeviceInfo.maxMemAllocSize;

    deviceInfo.maxWorkItemDimensions = 3;

    deviceInfo.maxComputUnits = systemInfo.EUCount * subDevicesCount;
    if (device.isEngineInstanced()) {
        deviceInfo.maxComputUnits /= systemInfo.CCSInfo.NumberOfCCSEnabled;
    }

    deviceInfo.maxConstantArgs = 8;
    deviceInfo.maxSliceCount = systemInfo.SliceCount;

    deviceInfo.singleFpConfig |= defaultFpFlags;

    deviceInfo.halfFpConfig = defaultFpFlags;

    PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "computeUnitsUsedForScratch: %d\n", sharedDeviceInfo.computeUnitsUsedForScratch);

    PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "hwInfo: {%d, %d}: (%d, %d, %d)\n",
                       systemInfo.EUCount,
                       systemInfo.ThreadCount,
                       systemInfo.MaxEuPerSubSlice,
                       systemInfo.MaxSlicesSupported,
                       systemInfo.MaxSubSlicesSupported);

    deviceInfo.localMemType = CL_LOCAL;

    deviceInfo.image3DMaxWidth = hwHelper.getMax3dImageWidthOrHeight();
    deviceInfo.image3DMaxHeight = hwHelper.getMax3dImageWidthOrHeight();

    // cl_khr_image2d_from_buffer
    deviceInfo.imagePitchAlignment = hwHelper.getPitchAlignmentForImage(&hwInfo);
    deviceInfo.imageBaseAddressAlignment = 4;
    deviceInfo.queueOnHostProperties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;

    deviceInfo.pipeSupport = arePipesSupported();
    if (arePipesSupported()) {
        deviceInfo.maxPipeArgs = 16;
        deviceInfo.pipeMaxPacketSize = 1024;
        deviceInfo.pipeMaxActiveReservations = 1;
    } else {
        deviceInfo.maxPipeArgs = 0;
        deviceInfo.pipeMaxPacketSize = 0;
        deviceInfo.pipeMaxActiveReservations = 0;
    }

    deviceInfo.atomicMemoryCapabilities = CL_DEVICE_ATOMIC_ORDER_RELAXED | CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP;
    if (ocl21FeaturesEnabled) {
        deviceInfo.atomicMemoryCapabilities |= CL_DEVICE_ATOMIC_ORDER_ACQ_REL | CL_DEVICE_ATOMIC_ORDER_SEQ_CST |
                                               CL_DEVICE_ATOMIC_SCOPE_ALL_DEVICES | CL_DEVICE_ATOMIC_SCOPE_DEVICE;
    }

    deviceInfo.atomicFenceCapabilities = CL_DEVICE_ATOMIC_ORDER_RELAXED | CL_DEVICE_ATOMIC_ORDER_ACQ_REL |
                                         CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP;
    if (ocl21FeaturesEnabled) {
        deviceInfo.atomicFenceCapabilities |= CL_DEVICE_ATOMIC_ORDER_SEQ_CST | CL_DEVICE_ATOMIC_SCOPE_ALL_DEVICES |
                                              CL_DEVICE_ATOMIC_SCOPE_DEVICE | CL_DEVICE_ATOMIC_SCOPE_WORK_ITEM;
    }

    deviceInfo.nonUniformWorkGroupSupport = true;
    deviceInfo.workGroupCollectiveFunctionsSupport = ocl21FeaturesEnabled;
    deviceInfo.genericAddressSpaceSupport = ocl21FeaturesEnabled;

    deviceInfo.linkerAvailable = true;
    deviceInfo.svmCapabilities = hwInfo.capabilityTable.ftrSvm * CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;
    if (hwInfo.capabilityTable.ftrSvm) {
        auto reportFineGrained = hwInfo.capabilityTable.ftrSvm * hwInfo.capabilityTable.ftrSupportsCoherency;
        if (DebugManager.flags.ForceFineGrainedSVMSupport.get() != -1) {
            reportFineGrained = !!DebugManager.flags.ForceFineGrainedSVMSupport.get();
        }
        if (reportFineGrained) {
            deviceInfo.svmCapabilities |= static_cast<cl_device_svm_capabilities>(CL_DEVICE_SVM_FINE_GRAIN_BUFFER | CL_DEVICE_SVM_ATOMICS);
        }
    }

    for (auto &engineGroup : this->getDevice().getRegularEngineGroups()) {
        cl_queue_family_properties_intel properties = {};
        properties.capabilities = getQueueFamilyCapabilities(engineGroup.engineGroupType);
        properties.count = static_cast<cl_uint>(engineGroup.engines.size());
        properties.properties = deviceInfo.queueOnHostProperties;
        getQueueFamilyName(properties.name, engineGroup.engineGroupType);
        deviceInfo.queueFamilyProperties.push_back(properties);
    }
    auto &clHwHelper = NEO::ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);
    const std::vector<uint32_t> &supportedThreadArbitrationPolicies = clHwHelper.getSupportedThreadArbitrationPolicies();
    deviceInfo.supportedThreadArbitrationPolicies.resize(supportedThreadArbitrationPolicies.size());
    for (size_t policy = 0u; policy < supportedThreadArbitrationPolicies.size(); policy++) {
        deviceInfo.supportedThreadArbitrationPolicies[policy] = supportedThreadArbitrationPolicies[policy];
    }
    deviceInfo.preemptionSupported = false;
    deviceInfo.maxGlobalVariableSize = ocl21FeaturesEnabled ? 64 * KB : 0;
    deviceInfo.globalVariablePreferredTotalSize = ocl21FeaturesEnabled ? static_cast<size_t>(sharedDeviceInfo.maxMemAllocSize) : 0;

    deviceInfo.planarYuvMaxWidth = 16384;
    deviceInfo.planarYuvMaxHeight = hwHelper.getPlanarYuvMaxHeight();

    deviceInfo.vmeAvcSupportsTextureSampler = hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler;
    if (hwInfo.capabilityTable.supportsVme) {
        deviceInfo.vmeAvcVersion = CL_AVC_ME_VERSION_1_INTEL;
        deviceInfo.vmeVersion = CL_ME_VERSION_ADVANCED_VER_2_INTEL;
    }
    deviceInfo.platformHostTimerResolution = getPlatformHostTimerResolution();

    deviceInfo.internalDriverVersion = CL_DEVICE_DRIVER_VERSION_INTEL_NEO1;

    deviceInfo.preferredGlobalAtomicAlignment = MemoryConstants::cacheLineSize;
    deviceInfo.preferredLocalAtomicAlignment = MemoryConstants::cacheLineSize;
    deviceInfo.preferredPlatformAtomicAlignment = MemoryConstants::cacheLineSize;

    deviceInfo.preferredWorkGroupSizeMultiple = hwHelper.isFusedEuDispatchEnabled(hwInfo, false)
                                                    ? CommonConstants::maximalSimdSize * 2
                                                    : CommonConstants::maximalSimdSize;

    deviceInfo.hostMemCapabilities = hwInfoConfig->getHostMemCapabilities(&hwInfo);
    deviceInfo.deviceMemCapabilities = hwInfoConfig->getDeviceMemCapabilities();
    deviceInfo.singleDeviceSharedMemCapabilities = hwInfoConfig->getSingleDeviceSharedMemCapabilities();
    deviceInfo.crossDeviceSharedMemCapabilities = hwInfoConfig->getCrossDeviceSharedMemCapabilities();
    deviceInfo.sharedSystemMemCapabilities = hwInfoConfig->getSharedSystemMemCapabilities(&hwInfo);

    initializeOsSpecificCaps();
    getOpenclCFeaturesList(hwInfo, deviceInfo.openclCFeatures);
}

void ClDevice::initializeExtensionsWithVersion() {
    std::stringstream deviceExtensionsStringStream{deviceExtensions};
    std::vector<std::string> deviceExtensionsVector{
        std::istream_iterator<std::string>{deviceExtensionsStringStream}, std::istream_iterator<std::string>{}};
    deviceInfo.extensionsWithVersion.reserve(deviceExtensionsVector.size());
    for (auto deviceExtension : deviceExtensionsVector) {
        cl_name_version deviceExtensionWithVersion;
        deviceExtensionWithVersion.version = CL_MAKE_VERSION(1, 0, 0);
        strcpy_s(deviceExtensionWithVersion.name, CL_NAME_VERSION_MAX_NAME_SIZE, deviceExtension.c_str());
        deviceInfo.extensionsWithVersion.push_back(deviceExtensionWithVersion);
    }
}

void ClDevice::initializeOpenclCAllVersions() {
    cl_name_version openClCVersion;
    strcpy_s(openClCVersion.name, CL_NAME_VERSION_MAX_NAME_SIZE, "OpenCL C");

    openClCVersion.version = CL_MAKE_VERSION(1, 0, 0);
    deviceInfo.openclCAllVersions.push_back(openClCVersion);
    openClCVersion.version = CL_MAKE_VERSION(1, 1, 0);
    deviceInfo.openclCAllVersions.push_back(openClCVersion);
    openClCVersion.version = CL_MAKE_VERSION(1, 2, 0);
    deviceInfo.openclCAllVersions.push_back(openClCVersion);

    if (enabledClVersion == 30) {
        openClCVersion.version = CL_MAKE_VERSION(3, 0, 0);
        deviceInfo.openclCAllVersions.push_back(openClCVersion);
    }
}

const std::string ClDevice::getClDeviceName(const HardwareInfo &hwInfo) const {
    return this->getDevice().getDeviceInfo().name;
}

} // namespace NEO
