/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_info_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/sharings/sharing_factory.h"

#include "driver_version.h"

#include <iterator>
#include <sstream>

namespace NEO {
static std::string vendor = "Intel(R) Corporation";
static std::string profile = "FULL_PROFILE";
static std::string spirVersions = "1.2 ";
const char *latestConformanceVersionPassed = "v2025-04-14-00";
#define QTR(a) #a
#define TOSTR(b) QTR(b)
static std::string driverVersion = TOSTR(NEO_OCL_DRIVER_VERSION);

static constexpr cl_device_fp_config defaultFpFlags = static_cast<cl_device_fp_config>(CL_FP_ROUND_TO_NEAREST |
                                                                                       CL_FP_ROUND_TO_ZERO |
                                                                                       CL_FP_ROUND_TO_INF |
                                                                                       CL_FP_INF_NAN |
                                                                                       CL_FP_DENORM |
                                                                                       CL_FP_FMA);

static constexpr cl_device_fp_atomic_capabilities_ext defaultFpAtomicCapabilities = static_cast<cl_device_fp_atomic_capabilities_ext>(CL_DEVICE_GLOBAL_FP_ATOMIC_LOAD_STORE_EXT |
                                                                                                                                      CL_DEVICE_LOCAL_FP_ATOMIC_LOAD_STORE_EXT);

void ClDevice::setupFp64Flags() {
    auto &hwInfo = getHardwareInfo();

    if (debugManager.flags.OverrideDefaultFP64Settings.get() == 1) {
        deviceInfo.singleFpConfig = static_cast<cl_device_fp_config>(CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
        deviceInfo.doubleFpConfig = defaultFpFlags;
        deviceInfo.nativeVectorWidthDouble = 1;
        deviceInfo.preferredVectorWidthDouble = 1;
    } else if (debugManager.flags.OverrideDefaultFP64Settings.get() == -1) {
        if (hwInfo.capabilityTable.ftrSupportsFP64) {
            deviceInfo.doubleFpConfig = defaultFpFlags;
            deviceInfo.nativeVectorWidthDouble = 1;
            deviceInfo.preferredVectorWidthDouble = 1;
        } else {
            if (hwInfo.capabilityTable.ftrSupportsFP64Emulation) {
                if (getDevice().getExecutionEnvironment()->isFP64EmulationEnabled()) {
                    deviceInfo.doubleFpConfig = defaultFpFlags | CL_FP_SOFT_FLOAT;
                    deviceInfo.nativeVectorWidthDouble = 1;
                    deviceInfo.preferredVectorWidthDouble = 1;
                }
            } else {
                deviceInfo.doubleFpConfig = 0;
                deviceInfo.nativeVectorWidthDouble = 0;
                deviceInfo.preferredVectorWidthDouble = 0;
            }
        }

        deviceInfo.singleFpConfig = static_cast<cl_device_fp_config>(
            hwInfo.capabilityTable.ftrSupports64BitMath
                ? CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT
                : 0);
    }
}

void ClDevice::initializeCaps() {
    auto &hwInfo = getHardwareInfo();
    auto &rootDeviceEnvironment = getRootDeviceEnvironment();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto &sharedDeviceInfo = getSharedDeviceInfo();
    deviceExtensions.clear();
    deviceExtensions.append(compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper));

    driverVersion = TOSTR(NEO_OCL_DRIVER_VERSION);

    if (debugManager.flags.OverrideDeviceName.get() != "unk") {
        name.assign(debugManager.flags.OverrideDeviceName.get().c_str());
    } else {
        name = getClDeviceName();
        if (driverInfo) {
            name.assign(driverInfo->getDeviceName(name).c_str());
        }
    }

    if (driverInfo) {
        driverVersion.assign(driverInfo->getVersion(driverVersion).c_str());
        sharingFactory.verifyExtensionSupport(driverInfo.get());
    }

    deviceInfo.name = name.c_str();
    deviceInfo.driverVersion = driverVersion.c_str();

    setupFp64Flags();

    deviceInfo.vendor = vendor.c_str();
    deviceInfo.profile = profile.c_str();
    enabledClVersion = hwInfo.capabilityTable.clVersionSupport;
    ocl21FeaturesEnabled = HwInfoHelper::checkIfOcl21FeaturesEnabledOrEnforced(hwInfo);
    if (debugManager.flags.ForceOCLVersion.get() != 0) {
        enabledClVersion = debugManager.flags.ForceOCLVersion.get();
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
    initializeILsWithVersion();

    deviceInfo.independentForwardProgress = hwInfo.capabilityTable.supportsIndependentForwardProgress;
    deviceInfo.maxNumOfSubGroups = 0;

    auto simdSizeUsed = debugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get()
                            ? CommonConstants::maximalSimdSize
                            : gfxCoreHelper.getMinimalSIMDSize();

    if (ocl21FeaturesEnabled) {

        // calculate a maximum number of subgroups in a workgroup (for the required SIMD size)
        deviceInfo.maxNumOfSubGroups = static_cast<uint32_t>(sharedDeviceInfo.maxWorkGroupSize / simdSizeUsed);
    }

    if (enabledClVersion >= 20) {
        deviceInfo.singleFpAtomicCapabilities = defaultFpAtomicCapabilities;
        deviceInfo.halfFpAtomicCapabilities = 0;
        if (ocl21FeaturesEnabled && hwInfo.capabilityTable.supportsFloatAtomics) {
            uint32_t fp16Caps = 0u;
            uint32_t fp32Caps = 0u;
            compilerProductHelper.getKernelFp16AtomicCapabilities(releaseHelper, fp16Caps);
            compilerProductHelper.getKernelFp32AtomicCapabilities(fp32Caps);
            deviceInfo.halfFpAtomicCapabilities = fp16Caps;
            deviceInfo.singleFpAtomicCapabilities = fp32Caps;
        }

        const cl_device_fp_atomic_capabilities_ext baseFP64AtomicCapabilities = hwInfo.capabilityTable.ftrSupportsInteger64BitAtomics || hwInfo.capabilityTable.supportsFloatAtomics ? defaultFpAtomicCapabilities : 0;
        const cl_device_fp_atomic_capabilities_ext optionalFP64AtomicCapabilities = ocl21FeaturesEnabled && hwInfo.capabilityTable.supportsFloatAtomics ? static_cast<cl_device_fp_atomic_capabilities_ext>(
                                                                                                                                                              CL_DEVICE_GLOBAL_FP_ATOMIC_ADD_EXT | CL_DEVICE_GLOBAL_FP_ATOMIC_MIN_MAX_EXT |
                                                                                                                                                              CL_DEVICE_LOCAL_FP_ATOMIC_ADD_EXT | CL_DEVICE_LOCAL_FP_ATOMIC_MIN_MAX_EXT)
                                                                                                                                                        : 0;

        deviceInfo.doubleFpAtomicCapabilities = deviceInfo.doubleFpConfig != 0u ? baseFP64AtomicCapabilities | optionalFP64AtomicCapabilities : 0;
        static_assert(CL_DEVICE_GLOBAL_FP_ATOMIC_LOAD_STORE_EXT == FpAtomicExtFlags::globalLoadStore, "Mismatch between internal and API - specific capabilities.");
        static_assert(CL_DEVICE_GLOBAL_FP_ATOMIC_ADD_EXT == FpAtomicExtFlags::globalAdd, "Mismatch between internal and API - specific capabilities.");
        static_assert(CL_DEVICE_GLOBAL_FP_ATOMIC_MIN_MAX_EXT == FpAtomicExtFlags::globalMinMax, "Mismatch between internal and API - specific capabilities.");
        static_assert(CL_DEVICE_LOCAL_FP_ATOMIC_LOAD_STORE_EXT == FpAtomicExtFlags::localLoadStore, "Mismatch between internal and API - specific capabilities.");
        static_assert(CL_DEVICE_LOCAL_FP_ATOMIC_ADD_EXT == FpAtomicExtFlags::localAdd, "Mismatch between internal and API - specific capabilities.");
        static_assert(CL_DEVICE_LOCAL_FP_ATOMIC_MIN_MAX_EXT == FpAtomicExtFlags::localMinMax, "Mismatch between internal and API - specific capabilities.");
    }

    if (debugManager.flags.EnableNV12.get() && hwInfo.capabilityTable.supportsImages) {
        deviceInfo.nv12Extension = true;
    }
    if (debugManager.flags.EnablePackedYuv.get() && hwInfo.capabilityTable.supportsImages) {
        deviceInfo.packedYuvExtension = true;
    }

    auto sharingAllowed = (getNumGenericSubDevices() <= 1u) && productHelper.isSharingWith3dOrMediaAllowed();
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

    deviceInfo.deviceExtensions = deviceExtensions.c_str();
    deviceInfo.builtInKernels = "";

    deviceInfo.deviceType = CL_DEVICE_TYPE_GPU;
    deviceInfo.endianLittle = 1;
    deviceInfo.hostUnifiedMemory = (false == gfxCoreHelper.isLocalMemoryEnabled(hwInfo));
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
    deviceInfo.preferredVectorWidthChar = gfxCoreHelper.getPreferredVectorWidthChar(simdSizeUsed);
    deviceInfo.preferredVectorWidthShort = gfxCoreHelper.getPreferredVectorWidthShort(simdSizeUsed);
    deviceInfo.preferredVectorWidthInt = gfxCoreHelper.getPreferredVectorWidthInt(simdSizeUsed);
    deviceInfo.preferredVectorWidthLong = gfxCoreHelper.getPreferredVectorWidthLong(simdSizeUsed);
    deviceInfo.preferredVectorWidthFloat = gfxCoreHelper.getPreferredVectorWidthFloat(simdSizeUsed);
    deviceInfo.preferredVectorWidthHalf = gfxCoreHelper.getPreferredVectorWidthHalf(simdSizeUsed);
    deviceInfo.nativeVectorWidthChar = gfxCoreHelper.getNativeVectorWidthChar(simdSizeUsed);
    deviceInfo.nativeVectorWidthShort = gfxCoreHelper.getNativeVectorWidthShort(simdSizeUsed);
    deviceInfo.nativeVectorWidthInt = gfxCoreHelper.getNativeVectorWidthInt(simdSizeUsed);
    deviceInfo.nativeVectorWidthLong = gfxCoreHelper.getNativeVectorWidthLong(simdSizeUsed);
    deviceInfo.nativeVectorWidthFloat = gfxCoreHelper.getNativeVectorWidthFloat(simdSizeUsed);
    deviceInfo.nativeVectorWidthHalf = gfxCoreHelper.getNativeVectorWidthHalf(simdSizeUsed);
    deviceInfo.maxReadWriteImageArgs = hwInfo.capabilityTable.supportsImages ? 128 : 0;
    deviceInfo.executionCapabilities = CL_EXEC_KERNEL;

    // copy system info to prevent misaligned reads
    const auto systemInfo = hwInfo.gtSystemInfo;

    const auto subDevicesCount = std::max(getNumGenericSubDevices(), 1u);
    deviceInfo.globalMemCacheSize = systemInfo.L3CacheSizeInKb * MemoryConstants::kiloByte * subDevicesCount;
    deviceInfo.grfSize = hwInfo.capabilityTable.grfSize;

    deviceInfo.globalMemCacheType = CL_READ_WRITE_CACHE;
    deviceInfo.memBaseAddressAlign = 1024;
    deviceInfo.minDataTypeAlignSize = 128;

    deviceInfo.preferredInteropUserSync = 1u;

    // OpenCL 1.2 requires 128MB minimum

    deviceInfo.maxConstantBufferSize = sharedDeviceInfo.maxMemAllocSize;

    deviceInfo.maxWorkItemDimensions = 3;

    deviceInfo.maxComputUnits = systemInfo.EUCount * subDevicesCount;

    deviceInfo.maxConstantArgs = 8;
    deviceInfo.maxSliceCount = systemInfo.SliceCount;

    deviceInfo.singleFpConfig |= defaultFpFlags;

    deviceInfo.halfFpConfig = defaultFpFlags;

    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "computeUnitsUsedForScratch: %d\n", sharedDeviceInfo.computeUnitsUsedForScratch);

    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "hwInfo: {%d, %d}: (%d, %d, %d)\n",
                       systemInfo.EUCount,
                       systemInfo.ThreadCount,
                       systemInfo.MaxEuPerSubSlice,
                       systemInfo.MaxSlicesSupported,
                       systemInfo.MaxSubSlicesSupported);

    deviceInfo.localMemType = CL_LOCAL;

    deviceInfo.image3DMaxWidth = gfxCoreHelper.getMax3dImageWidthOrHeight();
    deviceInfo.image3DMaxHeight = gfxCoreHelper.getMax3dImageWidthOrHeight();

    // cl_khr_image2d_from_buffer
    deviceInfo.imagePitchAlignment = gfxCoreHelper.getPitchAlignmentForImage(rootDeviceEnvironment);
    deviceInfo.imageBaseAddressAlignment = 4;
    deviceInfo.queueOnHostProperties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;

    deviceInfo.pipeSupport = false;
    deviceInfo.maxPipeArgs = 0;
    deviceInfo.pipeMaxPacketSize = 0;
    deviceInfo.pipeMaxActiveReservations = 0;

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
    deviceInfo.svmCapabilities = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;
    auto reportFineGrained = hwInfo.capabilityTable.ftrSupportsCoherency;
    if (debugManager.flags.ForceFineGrainedSVMSupport.get() != -1) {
        reportFineGrained = !!debugManager.flags.ForceFineGrainedSVMSupport.get();
    }
    if (reportFineGrained) {
        deviceInfo.svmCapabilities |= static_cast<cl_device_svm_capabilities>(CL_DEVICE_SVM_FINE_GRAIN_BUFFER | CL_DEVICE_SVM_ATOMICS);
    }

    for (auto &engineGroup : this->getDevice().getRegularEngineGroups()) {
        cl_queue_family_properties_intel properties = {};
        properties.capabilities = getQueueFamilyCapabilities(engineGroup.engineGroupType);
        properties.count = static_cast<cl_uint>(engineGroup.engines.size());
        properties.properties = deviceInfo.queueOnHostProperties;
        getQueueFamilyName(properties.name, engineGroup.engineGroupType);
        deviceInfo.queueFamilyProperties.push_back(properties);
    }
    auto &clGfxCoreHelper = rootDeviceEnvironment.getHelper<ClGfxCoreHelper>();
    const std::vector<uint32_t> &supportedThreadArbitrationPolicies = clGfxCoreHelper.getSupportedThreadArbitrationPolicies();
    deviceInfo.supportedThreadArbitrationPolicies.resize(supportedThreadArbitrationPolicies.size());
    for (size_t policy = 0u; policy < supportedThreadArbitrationPolicies.size(); policy++) {
        deviceInfo.supportedThreadArbitrationPolicies[policy] = supportedThreadArbitrationPolicies[policy];
    }
    deviceInfo.preemptionSupported = false;
    deviceInfo.maxGlobalVariableSize = ocl21FeaturesEnabled ? 64 * MemoryConstants::kiloByte : 0;
    deviceInfo.globalVariablePreferredTotalSize = ocl21FeaturesEnabled ? static_cast<size_t>(sharedDeviceInfo.maxMemAllocSize) : 0;

    deviceInfo.planarYuvMaxWidth = 16384;
    deviceInfo.planarYuvMaxHeight = gfxCoreHelper.getPlanarYuvMaxHeight();

    deviceInfo.platformHostTimerResolution = getPlatformHostTimerResolution();

    deviceInfo.internalDriverVersion = CL_DEVICE_DRIVER_VERSION_INTEL_NEO1;

    deviceInfo.preferredGlobalAtomicAlignment = MemoryConstants::cacheLineSize;
    deviceInfo.preferredLocalAtomicAlignment = MemoryConstants::cacheLineSize;
    deviceInfo.preferredPlatformAtomicAlignment = MemoryConstants::cacheLineSize;

    deviceInfo.preferredWorkGroupSizeMultiple = gfxCoreHelper.isFusedEuDispatchEnabled(hwInfo, false)
                                                    ? CommonConstants::maximalSimdSize * 2
                                                    : CommonConstants::maximalSimdSize;

    deviceInfo.hostMemCapabilities = productHelper.getHostMemCapabilities(&hwInfo);
    deviceInfo.deviceMemCapabilities = productHelper.getDeviceMemCapabilities();

    const bool isKmdMigrationAvailable{getMemoryManager()->isKmdMigrationAvailable(getRootDeviceIndex())};
    deviceInfo.singleDeviceSharedMemCapabilities = productHelper.getSingleDeviceSharedMemCapabilities(isKmdMigrationAvailable);
    deviceInfo.crossDeviceSharedMemCapabilities = productHelper.getCrossDeviceSharedMemCapabilities();
    deviceInfo.sharedSystemMemCapabilities = productHelper.getSharedSystemMemCapabilities(&hwInfo);

    if (compilerProductHelper.isDotIntegerProductExtensionSupported()) {
        deviceInfo.integerDotCapabilities = CL_DEVICE_INTEGER_DOT_PRODUCT_INPUT_4x8BIT_KHR | CL_DEVICE_INTEGER_DOT_PRODUCT_INPUT_4x8BIT_PACKED_KHR;
        deviceInfo.integerDotAccelerationProperties8Bit = {
            CL_TRUE,  // signed_accelerated;
            CL_TRUE,  // unsigned_accelerated;
            CL_TRUE,  // mixed_signedness_accelerated;
            CL_TRUE,  // accumulating_saturating_signed_accelerated;
            CL_TRUE,  // accumulating_saturating_unsigned_accelerated;
            CL_TRUE}; // accumulating_saturating_mixed_signedness_accelerated;
        deviceInfo.integerDotAccelerationProperties4x8BitPacked = {
            CL_TRUE,  // signed_accelerated;
            CL_TRUE,  // unsigned_accelerated;
            CL_TRUE,  // mixed_signedness_accelerated;
            CL_TRUE,  // accumulating_saturating_signed_accelerated;
            CL_TRUE,  // accumulating_saturating_unsigned_accelerated;
            CL_TRUE}; // accumulating_saturating_mixed_signedness_accelerated;
    }

    initializeOsSpecificCaps();
    getOpenclCFeaturesList(hwInfo, deviceInfo.openclCFeatures, getDevice().getCompilerProductHelper(), releaseHelper);
}

void ClDevice::initializeExtensionsWithVersion() {
    std::stringstream deviceExtensionsStringStream{deviceExtensions};
    std::vector<std::string> deviceExtensionsVector{
        std::istream_iterator<std::string>{deviceExtensionsStringStream}, std::istream_iterator<std::string>{}};
    deviceInfo.extensionsWithVersion.reserve(deviceExtensionsVector.size());
    for (auto &deviceExtension : deviceExtensionsVector) {
        cl_name_version deviceExtensionWithVersion;
        deviceExtensionWithVersion.version = getExtensionVersion(deviceExtension);
        strcpy_s(deviceExtensionWithVersion.name, CL_NAME_VERSION_MAX_NAME_SIZE, deviceExtension.c_str());
        deviceInfo.extensionsWithVersion.push_back(deviceExtensionWithVersion);
    }
}

void ClDevice::initializeOpenclCAllVersions() {
    auto deviceOpenCLCVersions = this->getCompilerProductHelper().getDeviceOpenCLCVersions(this->getHardwareInfo(), {static_cast<unsigned short>(enabledClVersion / 10), static_cast<unsigned short>(enabledClVersion % 10)});
    cl_name_version openClCVersion;
    strcpy_s(openClCVersion.name, CL_NAME_VERSION_MAX_NAME_SIZE, "OpenCL C");

    for (auto &ver : deviceOpenCLCVersions) {
        openClCVersion.version = CL_MAKE_VERSION(ver.major, ver.minor, 0);
        deviceInfo.openclCAllVersions.push_back(openClCVersion);
    }
}

void ClDevice::initializeILsWithVersion() {
    std::stringstream ilsStringStream{device.getDeviceInfo().ilVersion};
    std::vector<std::string> ilsVector{
        std::istream_iterator<std::string>{ilsStringStream}, std::istream_iterator<std::string>{}};
    deviceInfo.ilsWithVersion.reserve(ilsVector.size());
    for (auto &il : ilsVector) {
        size_t majorVersionPos = il.find_last_of('_');
        size_t minorVersionPos = il.find_last_of('.');
        if (majorVersionPos != std::string::npos && minorVersionPos != std::string::npos) {
            cl_name_version ilWithVersion;
            uint32_t majorVersion = static_cast<uint32_t>(std::stoul(il.substr(majorVersionPos + 1)));
            uint32_t minorVersion = static_cast<uint32_t>(std::stoul(il.substr(minorVersionPos + 1)));
            strcpy_s(ilWithVersion.name, CL_NAME_VERSION_MAX_NAME_SIZE, il.substr(0, majorVersionPos).c_str());
            ilWithVersion.version = CL_MAKE_VERSION(majorVersion, minorVersion, 0);
            deviceInfo.ilsWithVersion.push_back(ilWithVersion);
        }
    }
}

void ClDevice::initializeMaxPoolCount() {
    auto &device = getDevice();
    const auto bitfield = device.getDeviceBitfield();
    const auto deviceMemory = device.getGlobalMemorySize(static_cast<uint32_t>(bitfield.to_ulong()));
    const auto preferredBufferPoolParams = SmallBuffersParams::getPreferredBufferPoolParams(device.getProductHelper());
    const auto maxPoolCount = Context::BufferPoolAllocator::calculateMaxPoolCount(preferredBufferPoolParams, deviceMemory, 2);
    device.updateMaxPoolCount(maxPoolCount);
}

const std::string ClDevice::getClDeviceName() const {
    return this->getDevice().getDeviceInfo().name;
}

} // namespace NEO
