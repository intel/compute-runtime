/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/logger.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>

namespace NEO {

IoctlHelperPrelim20::IoctlHelperPrelim20(Drm &drmArg) : IoctlHelperI915(drmArg) {
    const auto &productHelper = this->drm.getRootDeviceEnvironment().getHelper<ProductHelper>();
    handleExecBufferInNonBlockMode = productHelper.isNonBlockingGpuSubmissionSupported();
    if (debugManager.flags.ForceNonblockingExecbufferCalls.get() != -1) {
        handleExecBufferInNonBlockMode = debugManager.flags.ForceNonblockingExecbufferCalls.get();
    }
    if (handleExecBufferInNonBlockMode) {
        auto fileDescriptor = this->drm.getFileDescriptor();
        auto flags = SysCalls::fcntl(fileDescriptor, F_GETFL);
        [[maybe_unused]] auto status = SysCalls::fcntl(fileDescriptor, F_SETFL, flags | O_NONBLOCK);
        DEBUG_BREAK_IF(status != 0);
    }
};

bool IoctlHelperPrelim20::isSetPairAvailable() {
    int setPairSupported = 0;
    GetParam getParam{};
    getParam.param = PRELIM_I915_PARAM_HAS_SET_PAIR;
    getParam.value = &setPairSupported;
    int retVal = IoctlHelper::ioctl(DrmIoctl::getparam, &getParam);
    if (retVal) {
        return false;
    }
    return setPairSupported;
}

bool IoctlHelperPrelim20::isChunkingAvailable() {
    int chunkSupported = 0;
    GetParam getParam{};
    getParam.param = PRELIM_I915_PARAM_HAS_CHUNK_SIZE;
    getParam.value = &chunkSupported;
    int retVal = IoctlHelper::ioctl(DrmIoctl::getparam, &getParam);
    if (retVal) {
        return false;
    }
    return chunkSupported;
}

bool IoctlHelperPrelim20::getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) {

    auto request = this->getDrmParamValue(DrmParam::queryComputeSlices);
    auto engineInfo = drm.getEngineInfo();
    auto nTiles = hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount;

    auto useNewQuery = debugManager.flags.UseNewQueryTopoIoctl.get() &&
                       engineInfo &&
                       (nTiles > 0);

    if (useNewQuery) {

        bool success = true;

        int sliceCount = 0;
        int subSliceCount = 0;
        int euCount = 0;

        for (auto i = 0u; i < nTiles; i++) {
            auto classInstance = engineInfo->getEngineInstance(i, hwInfo.capabilityTable.defaultEngineType);
            UNRECOVERABLE_IF(!classInstance);

            uint32_t flags = classInstance->engineClass;
            flags |= (classInstance->engineInstance << 8);

            auto dataQuery = drm.query<uint64_t>(request, flags);
            if (dataQuery.empty()) {
                success = false;
                break;
            }

            auto data = reinterpret_cast<QueryTopologyInfo *>(dataQuery.data());
            DrmQueryTopologyData tileTopologyData = {};
            TopologyMapping mapping;
            if (!this->translateTopologyInfo(data, tileTopologyData, mapping)) {
                success = false;
                break;
            }

            // pick smallest config
            sliceCount = (sliceCount == 0) ? tileTopologyData.sliceCount : std::min(sliceCount, tileTopologyData.sliceCount);
            subSliceCount = (subSliceCount == 0) ? tileTopologyData.subSliceCount : std::min(subSliceCount, tileTopologyData.subSliceCount);
            euCount = (euCount == 0) ? tileTopologyData.euCount : std::min(euCount, tileTopologyData.euCount);

            topologyData.maxSlices = std::max(topologyData.maxSlices, tileTopologyData.maxSlices);
            topologyData.maxSubSlicesPerSlice = std::max(topologyData.maxSubSlicesPerSlice, tileTopologyData.maxSubSlicesPerSlice);
            topologyData.maxEusPerSubSlice = std::max(topologyData.maxEusPerSubSlice, static_cast<int>(data->maxEusPerSubslice));

            topologyMap[i] = mapping;
        }

        if (success) {
            topologyData.sliceCount = sliceCount;
            topologyData.subSliceCount = subSliceCount;
            topologyData.euCount = euCount;
            return true;
        }
    }

    // fallback to DRM_I915_QUERY_TOPOLOGY_INFO
    return IoctlHelperI915::getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
}

bool IoctlHelperPrelim20::isVmBindAvailable() {
    const auto &productHelper = drm.getRootDeviceEnvironment().getHelper<ProductHelper>();
    if (!productHelper.isNewResidencyModelSupported()) {
        return false;
    }
    int vmBindSupported = 0;
    GetParam getParam{};
    getParam.param = PRELIM_I915_PARAM_HAS_VM_BIND;
    getParam.value = &vmBindSupported;
    int retVal = IoctlHelper::ioctl(DrmIoctl::getparam, &getParam);
    if (retVal) {
        return false;
    }
    return vmBindSupported;
}

int IoctlHelperPrelim20::createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent) {
    uint32_t regionsSize = static_cast<uint32_t>(memClassInstances.size());
    std::vector<prelim_drm_i915_gem_memory_class_instance> regions(regionsSize);
    for (uint32_t i = 0; i < regionsSize; i++) {
        regions[i].memory_class = memClassInstances[i].memoryClass;
        regions[i].memory_instance = memClassInstances[i].memoryInstance;
    }
    prelim_drm_i915_gem_object_param regionParam{};
    regionParam.size = regionsSize;
    regionParam.data = reinterpret_cast<uintptr_t>(regions.data());
    regionParam.param = PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_MEMORY_REGIONS;

    prelim_drm_i915_gem_create_ext_setparam setparamRegion{};
    setparamRegion.base.name = PRELIM_I915_GEM_CREATE_EXT_SETPARAM;
    setparamRegion.param = regionParam;

    prelim_drm_i915_gem_create_ext_vm_private vmPrivate{};
    prelim_drm_i915_gem_create_ext_setparam pairSetparamRegion{};
    prelim_drm_i915_gem_create_ext_setparam chunkingParamRegion{};
    prelim_drm_i915_gem_create_ext_memory_policy memPolicy{};

    if (vmId != std::nullopt) {
        vmPrivate.base.name = PRELIM_I915_GEM_CREATE_EXT_VM_PRIVATE;
        vmPrivate.vm_id = vmId.value();
    }

    if (memPolicyMode != std::nullopt) {
        UNRECOVERABLE_IF(memPolicyNodemask == std::nullopt);
        memPolicy.base.name = PRELIM_I915_GEM_CREATE_EXT_MEMORY_POLICY;
        memPolicy.mode = memPolicyMode.value();
        memPolicy.flags = 0;
        memPolicy.nodemask_max = static_cast<uint32_t>(memPolicyNodemask.value().size());
        memPolicy.nodemask_ptr = reinterpret_cast<uintptr_t>(memPolicyNodemask.value().data());
    }

    if (pairHandle != -1) {
        pairSetparamRegion.base.name = PRELIM_I915_GEM_CREATE_EXT_SETPARAM;
        pairSetparamRegion.param.param = PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_SET_PAIR;
        pairSetparamRegion.param.data = pairHandle;
    }

    size_t chunkingSize = 0u;
    if (isChunked) {
        chunkingSize = allocSize / numOfChunks;
        chunkingParamRegion.base.name = PRELIM_I915_GEM_CREATE_EXT_SETPARAM;
        chunkingParamRegion.param.param = PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_SET_CHUNK_SIZE;
        UNRECOVERABLE_IF(chunkingSize & (MemoryConstants::pageSize64k - 1));
        chunkingParamRegion.param.data = chunkingSize;
        setparamRegion.base.next_extension = reinterpret_cast<uintptr_t>(&chunkingParamRegion);
        if (memPolicyMode != std::nullopt) {
            chunkingParamRegion.base.next_extension = reinterpret_cast<uintptr_t>(&memPolicy);
        }
    } else {
        auto *lastExtension = &(setparamRegion.base);
        if (vmId != std::nullopt) {
            setparamRegion.base.next_extension = reinterpret_cast<uintptr_t>(&vmPrivate);
            lastExtension = &(vmPrivate.base);
        }
        if (pairHandle != -1) {
            lastExtension->next_extension = reinterpret_cast<uintptr_t>(&pairSetparamRegion);
            lastExtension = &(pairSetparamRegion.base);
        }
        if (memPolicyMode != std::nullopt) {
            lastExtension->next_extension = reinterpret_cast<uintptr_t>(&memPolicy);
        }
    }

    prelim_drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;

    createExt.extensions = reinterpret_cast<uintptr_t>(&setparamRegion);

    printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Performing GEM_CREATE_EXT with { size: %lu, param: 0x%llX",
                     allocSize, regionParam.param);

    if (debugManager.flags.PrintBOCreateDestroyResult.get()) {
        for (uint32_t i = 0; i < regionsSize; i++) {
            auto region = regions[i];
            printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, ", memory class: %d, memory instance: %d",
                             region.memory_class, region.memory_instance);
        }
        if (memPolicyMode != std::nullopt) {
            printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout,
                             ", memory policy:{ mode: %d, nodemask_max: 0x%d, nodemask_ptr: 0x%llX }",
                             memPolicy.mode,
                             memPolicy.nodemask_max,
                             memPolicy.nodemask_ptr);
        }
        printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "%s", " }\n");
    }

    auto ret = ioctl(DrmIoctl::gemCreateExt, &createExt);

    if (isChunked) {
        printDebugString(debugManager.flags.PrintBOChunkingLogs.get(), stdout,
                         "GEM_CREATE_EXT BO-%d with BOChunkingSize %d, chunkingParamRegion.param.data %d, numOfChunks %d\n",
                         createExt.handle,
                         chunkingSize,
                         chunkingParamRegion.param.data,
                         numOfChunks);
    }
    printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    handle = createExt.handle;
    return ret;
}

CacheRegion IoctlHelperPrelim20::closAlloc(CacheLevel cacheLevel) {
    struct prelim_drm_i915_gem_clos_reserve clos = {};

    int ret = IoctlHelper::ioctl(DrmIoctl::gemClosReserve, &clos);
    if (ret != 0) {
        int err = errno;
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_CLOS_RESERVE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(true);
        return CacheRegion::none;
    }

    return static_cast<CacheRegion>(clos.clos_index);
}

uint16_t IoctlHelperPrelim20::closAllocWays(CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) {
    struct prelim_drm_i915_gem_cache_reserve cache = {};

    cache.clos_index = static_cast<uint16_t>(closIndex);
    cache.cache_level = cacheLevel;
    cache.num_ways = numWays;

    int ret = IoctlHelper::ioctl(DrmIoctl::gemCacheReserve, &cache);
    if (ret != 0) {
        int err = errno;
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_CACHE_RESERVE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        return 0;
    }

    return cache.num_ways;
}

CacheRegion IoctlHelperPrelim20::closFree(CacheRegion closIndex) {
    struct prelim_drm_i915_gem_clos_free clos = {};

    clos.clos_index = static_cast<uint16_t>(closIndex);

    int ret = IoctlHelper::ioctl(DrmIoctl::gemClosFree, &clos);
    if (ret != 0) {
        int err = errno;
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_CLOS_FREE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(true);
        return CacheRegion::none;
    }

    return closIndex;
}

int IoctlHelperPrelim20::waitUserFence(uint32_t ctxId, uint64_t address,
                                       uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags,
                                       bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) {
    prelim_drm_i915_gem_wait_user_fence wait = {};

    wait.ctx_id = ctxId;
    wait.flags = flags;

    switch (dataWidth) {
    case 3u:
        wait.mask = PRELIM_I915_UFENCE_WAIT_U64;
        break;
    case 2u:
        wait.mask = PRELIM_I915_UFENCE_WAIT_U32;
        break;
    case 1u:
        wait.mask = PRELIM_I915_UFENCE_WAIT_U16;
        break;
    default:
        wait.mask = PRELIM_I915_UFENCE_WAIT_U8;
        break;
    }

    wait.op = PRELIM_I915_UFENCE_WAIT_GTE;
    wait.addr = address;
    wait.value = value;
    wait.timeout = timeout;

    return IoctlHelper::ioctl(DrmIoctl::gemWaitUserFence, &wait);
}

uint32_t IoctlHelperPrelim20::getAtomicAdvise(bool isNonAtomic) {
    return isNonAtomic ? PRELIM_I915_VM_ADVISE_ATOMIC_NONE : PRELIM_I915_VM_ADVISE_ATOMIC_SYSTEM;
}

uint32_t IoctlHelperPrelim20::getAtomicAccess(AtomicAccessMode mode) {
    uint32_t retVal = 0;

    switch (mode) {
    case AtomicAccessMode::device:
        retVal = PRELIM_I915_VM_ADVISE_ATOMIC_DEVICE;
        break;
    case AtomicAccessMode::system:
        retVal = PRELIM_I915_VM_ADVISE_ATOMIC_SYSTEM;
        break;
    case AtomicAccessMode::none:
        retVal = PRELIM_I915_VM_ADVISE_ATOMIC_NONE;
        break;
    case AtomicAccessMode::host:
    default:
        break;
    }
    return retVal;
}

uint32_t IoctlHelperPrelim20::getPreferredLocationAdvise() {
    return PRELIM_I915_VM_ADVISE_PREFERRED_LOCATION;
}

std::optional<MemoryClassInstance> IoctlHelperPrelim20::getPreferredLocationRegion(PreferredLocation memoryLocation, uint32_t memoryInstance) {
    MemoryClassInstance region{};
    if (NEO::debugManager.flags.SetVmAdvisePreferredLocation.get() != -1) {
        memoryLocation = static_cast<PreferredLocation>(NEO::debugManager.flags.SetVmAdvisePreferredLocation.get());
    }
    switch (memoryLocation) {
    case PreferredLocation::clear:
        region.memoryClass = -1;
        region.memoryInstance = 0;
        break;
    case PreferredLocation::system:
        region.memoryClass = getDrmParamValue(DrmParam::memoryClassSystem);
        region.memoryInstance = 0;
        break;
    case PreferredLocation::device:
    default:
        region.memoryClass = getDrmParamValue(DrmParam::memoryClassDevice);
        region.memoryInstance = memoryInstance;
        break;
    case PreferredLocation::none:
        return std::nullopt;
    }
    return region;
}

bool IoctlHelperPrelim20::setVmBoAdviseForChunking(int32_t handle, uint64_t start, uint64_t length, uint32_t attribute, void *region) {
    prelim_drm_i915_gem_vm_advise vmAdvise{};
    vmAdvise.handle = handle;
    vmAdvise.start = start;
    vmAdvise.length = length;
    vmAdvise.attribute = attribute;
    UNRECOVERABLE_IF(region == nullptr);
    vmAdvise.region = *reinterpret_cast<prelim_drm_i915_gem_memory_class_instance *>(region);

    int ret = IoctlHelper::ioctl(DrmIoctl::gemVmAdvise, &vmAdvise);
    if (ret != 0) {
        int err = errno;

        CREATE_DEBUG_STRING(str, "ioctl(PRELIM_DRM_I915_GEM_VM_ADVISE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        drm.getRootDeviceEnvironment().executionEnvironment.setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, str.get());
        DEBUG_BREAK_IF(true);

        return false;
    }
    return true;
}

bool IoctlHelperPrelim20::setVmBoAdvise(int32_t handle, uint32_t attribute, void *region) {
    prelim_drm_i915_gem_vm_advise vmAdvise{};

    vmAdvise.handle = handle;
    vmAdvise.attribute = attribute;
    if (region != nullptr) {
        vmAdvise.region = *reinterpret_cast<prelim_drm_i915_gem_memory_class_instance *>(region);
    }

    int ret = IoctlHelper::ioctl(DrmIoctl::gemVmAdvise, &vmAdvise);
    if (ret != 0) {
        int err = errno;

        CREATE_DEBUG_STRING(str, "ioctl(PRELIM_DRM_I915_GEM_VM_ADVISE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        drm.getRootDeviceEnvironment().executionEnvironment.setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, str.get());
        DEBUG_BREAK_IF(true);

        return false;
    }
    return true;
}

bool IoctlHelperPrelim20::setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) {
    prelim_drm_i915_gem_vm_prefetch vmPrefetch{};

    vmPrefetch.length = length;
    vmPrefetch.region = region;
    vmPrefetch.start = start;
    vmPrefetch.vm_id = vmId;

    int ret = IoctlHelper::ioctl(DrmIoctl::gemVmPrefetch, &vmPrefetch);
    if (ret != 0) {
        int err = errno;

        CREATE_DEBUG_STRING(str, "ioctl(PRELIM_DRM_I915_GEM_VM_PREFETCH) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        drm.getRootDeviceEnvironment().executionEnvironment.setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, str.get());
        DEBUG_BREAK_IF(true);

        return false;
    }
    return true;
}

uint32_t IoctlHelperPrelim20::getDirectSubmissionFlag() {
    return PRELIM_I915_CONTEXT_CREATE_FLAGS_LONG_RUNNING;
}

uint16_t IoctlHelperPrelim20::getWaitUserFenceSoftFlag() {
    return PRELIM_I915_UFENCE_WAIT_SOFT;
};

int IoctlHelperPrelim20::execBuffer(ExecBuffer *execBuffer, uint64_t completionGpuAddress, TaskCountType counterValue) {
    prelim_drm_i915_gem_execbuffer_ext_user_fence fenceObject = {};
    if (completionGpuAddress != 0) {
        fenceObject.base.name = PRELIM_DRM_I915_GEM_EXECBUFFER_EXT_USER_FENCE;
        fenceObject.addr = completionGpuAddress;
        fenceObject.value = counterValue;

        auto &drmExecBuffer = *reinterpret_cast<drm_i915_gem_execbuffer2 *>(execBuffer->data);
        drmExecBuffer.flags |= I915_EXEC_USE_EXTENSIONS;
        drmExecBuffer.num_cliprects = 0;
        drmExecBuffer.cliprects_ptr = castToUint64(&fenceObject);

        if (debugManager.flags.PrintCompletionFenceUsage.get()) {
            std::cout << "Completion fence submitted."
                      << " GPU address: " << std::hex << completionGpuAddress << std::dec
                      << ", value: " << counterValue << std::endl;
        }
    }

    return IoctlHelper::ioctl(DrmIoctl::gemExecbuffer2, execBuffer);
}

bool IoctlHelperPrelim20::completionFenceExtensionSupported(const bool isVmBindAvailable) {
    return isVmBindAvailable;
}

std::unique_ptr<uint8_t[]> IoctlHelperPrelim20::prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles, uint64_t cookie) {
    static_assert(std::is_trivially_destructible_v<prelim_drm_i915_vm_bind_ext_uuid>,
                  "Storage must be allowed to be reused without calling the destructor!");

    static_assert(alignof(prelim_drm_i915_vm_bind_ext_uuid) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                  "Alignment of a buffer returned via new[] operator must allow storing the required type!");

    const auto bufferSize{sizeof(prelim_drm_i915_vm_bind_ext_uuid) * bindExtHandles.size()};
    std::unique_ptr<uint8_t[]> extensionsBuffer{new uint8_t[bufferSize]};

    auto extensions = new (extensionsBuffer.get()) prelim_drm_i915_vm_bind_ext_uuid[bindExtHandles.size()];
    std::memset(extensionsBuffer.get(), 0, bufferSize);

    extensions[0].uuid_handle = bindExtHandles[0];
    extensions[0].base.name = PRELIM_I915_VM_BIND_EXT_UUID;

    for (size_t i = 1; i < bindExtHandles.size(); i++) {
        extensions[i - 1].base.next_extension = reinterpret_cast<uint64_t>(&extensions[i]);
        extensions[i].uuid_handle = bindExtHandles[i];
        extensions[i].base.name = PRELIM_I915_VM_BIND_EXT_UUID;
    }
    return extensionsBuffer;
}

uint64_t IoctlHelperPrelim20::getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident, bool bindLockedMemory, bool readOnlyResource) {
    uint64_t flags = 0u;
    if (bindCapture) {
        flags |= PRELIM_I915_GEM_VM_BIND_CAPTURE;
    }
    if (bindImmediate) {
        flags |= PRELIM_I915_GEM_VM_BIND_IMMEDIATE;
    }
    if (bindMakeResident || bindLockedMemory) { // lockedMemory is equal to residency in i915_prelim
        flags |= PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT;
    }
    if (readOnlyResource) {
        flags |= PRELIM_I915_GEM_VM_BIND_READONLY;
    }
    return flags;
}

prelim_drm_i915_query_distance_info translateToi915(const DistanceInfo &distanceInfo) {
    prelim_drm_i915_query_distance_info dist{};
    dist.engine.engine_class = distanceInfo.engine.engineClass;
    dist.engine.engine_instance = distanceInfo.engine.engineInstance;

    dist.region.memory_class = distanceInfo.region.memoryClass;
    dist.region.memory_instance = distanceInfo.region.memoryInstance;
    return dist;
}

int IoctlHelperPrelim20::queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) {
    std::vector<prelim_drm_i915_query_distance_info> i915Distances(distanceInfos.size());
    std::transform(distanceInfos.begin(), distanceInfos.end(), i915Distances.begin(), translateToi915);

    for (auto i = 0u; i < i915Distances.size(); i++) {
        queryItems[i].queryId = PRELIM_DRM_I915_QUERY_DISTANCE_INFO;
        queryItems[i].length = sizeof(prelim_drm_i915_query_distance_info);
        queryItems[i].flags = 0u;
        queryItems[i].dataPtr = reinterpret_cast<uint64_t>(&i915Distances[i]);
    }

    Query query{};
    query.itemsPtr = reinterpret_cast<uint64_t>(queryItems.data());
    query.numItems = static_cast<uint32_t>(queryItems.size());
    auto ret = IoctlHelper::ioctl(DrmIoctl::query, &query);
    for (auto i = 0u; i < i915Distances.size(); i++) {
        queryItems[i].dataPtr = 0u;
        distanceInfos[i].distance = i915Distances[i].distance;
    }
    return ret;
}

bool IoctlHelperPrelim20::isPageFaultSupported() {
    int pagefaultSupport{};
    GetParam getParam{};
    getParam.param = PRELIM_I915_PARAM_HAS_PAGE_FAULT;
    getParam.value = &pagefaultSupport;

    int retVal = ioctl(DrmIoctl::getparam, &getParam);
    if (debugManager.flags.PrintIoctlEntries.get()) {
        printf("DRM_IOCTL_I915_GETPARAM: param: PRELIM_I915_PARAM_HAS_PAGE_FAULT, output value: %d, retCode:% d\n",
               *getParam.value,
               retVal);
    }
    return (retVal == 0) && (pagefaultSupport > 0);
};

bool IoctlHelperPrelim20::isEuStallSupported() {
    return true;
}

uint32_t IoctlHelperPrelim20::getEuStallFdParameter() {
    return PRELIM_I915_PERF_FLAG_FD_EU_STALL;
}

bool IoctlHelperPrelim20::perfOpenEuStallStream(uint32_t euStallFdParameter, uint32_t &samplingPeriodNs, uint64_t engineInstance, uint64_t notifyNReports, uint64_t gpuTimeStampfrequency, int32_t *stream) {
    static constexpr uint32_t maxDssBufferSize = 512 * MemoryConstants::kiloByte;
    static constexpr uint32_t defaultPollPeriodNs = 10000000u;

    // Get sampling unit.
    static constexpr uint32_t samplingClockGranularity = 251u;
    static constexpr uint32_t minSamplingUnit = 1u;
    static constexpr uint32_t maxSamplingUnit = 7u;

    uint64_t gpuClockPeriodNs = CommonConstants::nsecPerSec / gpuTimeStampfrequency;
    uint64_t numberOfClocks = samplingPeriodNs / gpuClockPeriodNs;

    uint32_t samplingUnit = 0;
    samplingUnit = std::clamp(static_cast<uint32_t>(numberOfClocks / samplingClockGranularity), minSamplingUnit, maxSamplingUnit);
    samplingPeriodNs = samplingUnit * samplingClockGranularity * static_cast<uint32_t>(gpuClockPeriodNs);

    // Populate the EU stall properties.
    std::array<uint64_t, 12u> properties;

    properties[0] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_BUF_SZ;
    properties[1] = maxDssBufferSize;
    properties[2] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_SAMPLE_RATE;
    properties[3] = samplingUnit;
    properties[4] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_POLL_PERIOD;
    properties[5] = defaultPollPeriodNs;
    properties[6] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_CLASS;
    properties[7] = prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE;
    properties[8] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_INSTANCE;
    properties[9] = engineInstance;
    properties[10] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_EVENT_REPORT_COUNT;
    properties[11] = notifyNReports;

    // Call perf open ioctl.
    NEO::PrelimI915::drm_i915_perf_open_param param = {};
    param.flags = I915_PERF_FLAG_FD_CLOEXEC |
                  euStallFdParameter |
                  I915_PERF_FLAG_FD_NONBLOCK;
    param.num_properties = sizeof(properties) / 16;
    param.properties_ptr = reinterpret_cast<uintptr_t>(properties.data());
    *stream = ioctl(DrmIoctl::perfOpen, &param);
    if (*stream < 0) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (*stream < 0), stderr,
                           "%s failed errno = %d | ret = %d \n", "DRM_IOCTL_I915_PERF_OPEN", errno, *stream);
        return false;
    }
    auto ret = ioctl(*stream, DrmIoctl::perfEnable, 0);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (ret < 0), stderr,
                       "%s failed errno = %d | ret = %d \n", "I915_PERF_IOCTL_ENABLE", errno, ret);
    return (ret == 0) ? true : false;
}

bool IoctlHelperPrelim20::perfDisableEuStallStream(int32_t *stream) {
    int disableStatus = ioctl(*stream, DrmIoctl::perfDisable, 0);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (disableStatus < 0), stderr,
                       "I915_PERF_IOCTL_DISABLE failed errno = %d | ret = %d \n", errno, disableStatus);

    int closeStatus = NEO::SysCalls::close(*stream);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (closeStatus < 0), stderr,
                       "close() failed errno = %d | ret = %d \n", errno, closeStatus);
    *stream = -1;
    return ((closeStatus == 0) && (disableStatus == 0)) ? true : false;
}

std::unique_ptr<uint8_t[]> IoctlHelperPrelim20::createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) {

    if (regionInstanceClass) {
        auto retVal = std::make_unique<uint8_t[]>(sizeof(prelim_drm_i915_gem_vm_region_ext));

        auto regionExt = reinterpret_cast<prelim_drm_i915_gem_vm_region_ext *>(retVal.get());

        *regionExt = {};
        regionExt->base.name = PRELIM_I915_GEM_VM_CONTROL_EXT_REGION;
        regionExt->region.memory_class = regionInstanceClass.value().memoryClass;
        regionExt->region.memory_instance = regionInstanceClass.value().memoryInstance;

        return retVal;
    }
    return {};
}

uint32_t IoctlHelperPrelim20::getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) {
    uint32_t flags = 0u;
    if (disableScratch) {
        flags |= PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH;
    }

    if (enablePageFault) {
        flags |= PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT;
    }

    if (useVmBind) {
        flags |= PRELIM_I915_VM_CREATE_FLAGS_USE_VM_BIND;
    }

    return flags;
}

uint32_t gemCreateContextExt(IoctlHelper &ioctlHelper, GemContextCreateExt &gcc, GemContextCreateExtSetParam &extSetparam) {
    gcc.flags |= I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS;
    extSetparam.base.nextExtension = gcc.extensions;
    gcc.extensions = reinterpret_cast<uint64_t>(&extSetparam);

    auto ioctlResult = ioctlHelper.ioctl(DrmIoctl::gemContextCreateExt, &gcc);
    UNRECOVERABLE_IF(ioctlResult != 0);
    return gcc.contextId;
}

uint32_t gemCreateContextAcc(IoctlHelper &ioctlHelper, GemContextCreateExt &gcc, uint16_t trigger, uint8_t granularity) {
    prelim_drm_i915_gem_context_param_acc paramAcc = {};
    paramAcc.trigger = trigger;
    paramAcc.notify = 1;
    paramAcc.granularity = granularity;

    DrmUserExtension userExt{};
    userExt.name = I915_CONTEXT_CREATE_EXT_SETPARAM;

    GemContextParam ctxParam = {};
    ctxParam.param = PRELIM_I915_CONTEXT_PARAM_ACC;
    ctxParam.contextId = 0;
    ctxParam.size = sizeof(paramAcc);
    ctxParam.value = reinterpret_cast<uint64_t>(&paramAcc);

    GemContextCreateExtSetParam extSetparam{};
    extSetparam.base = userExt;
    extSetparam.param = ctxParam;

    return gemCreateContextExt(ioctlHelper, gcc, extSetparam);
}
uint32_t IoctlHelperPrelim20::createContextWithAccessCounters(GemContextCreateExt &gcc) {
    uint16_t trigger = 0;
    if (debugManager.flags.AccessCountersTrigger.get() != -1) {
        trigger = static_cast<uint16_t>(debugManager.flags.AccessCountersTrigger.get());
    }
    uint8_t granularity = PRELIM_I915_CONTEXT_ACG_2M;
    if (debugManager.flags.AccessCountersGranularity.get() != -1) {
        granularity = static_cast<uint8_t>(debugManager.flags.AccessCountersGranularity.get());
    }
    return gemCreateContextAcc(*this, gcc, trigger, granularity);
}

uint32_t IoctlHelperPrelim20::createCooperativeContext(GemContextCreateExt &gcc) {
    GemContextCreateExtSetParam extSetparam{};
    extSetparam.base.name = I915_CONTEXT_CREATE_EXT_SETPARAM;
    extSetparam.param.param = PRELIM_I915_CONTEXT_PARAM_RUNALONE;
    return gemCreateContextExt(*this, gcc, extSetparam);
}

static_assert(sizeof(VmBindExtSetPatT) == sizeof(prelim_drm_i915_vm_bind_ext_set_pat), "Invalid size for VmBindExtSetPat");

void IoctlHelperPrelim20::fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) {
    auto prelimVmBindExtSetPat = reinterpret_cast<prelim_drm_i915_vm_bind_ext_set_pat *>(vmBindExtSetPat);
    UNRECOVERABLE_IF(!prelimVmBindExtSetPat);
    prelimVmBindExtSetPat->base.name = PRELIM_I915_VM_BIND_EXT_SET_PAT;
    prelimVmBindExtSetPat->pat_index = patIndex;
    prelimVmBindExtSetPat->base.next_extension = nextExtension;
}

static_assert(sizeof(VmBindExtUserFenceT) == sizeof(prelim_drm_i915_vm_bind_ext_user_fence), "Invalid size for VmBindExtUserFence");

void IoctlHelperPrelim20::fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) {
    auto prelimVmBindExtUserFence = reinterpret_cast<prelim_drm_i915_vm_bind_ext_user_fence *>(vmBindExtUserFence);
    UNRECOVERABLE_IF(!prelimVmBindExtUserFence);
    prelimVmBindExtUserFence->base.name = PRELIM_I915_VM_BIND_EXT_USER_FENCE;
    prelimVmBindExtUserFence->base.next_extension = nextExtension;
    prelimVmBindExtUserFence->addr = fenceAddress;
    prelimVmBindExtUserFence->val = fenceValue;
}
void IoctlHelperPrelim20::setVmBindUserFence(VmBindParams &vmBind, VmBindExtUserFenceT vmBindUserFence) {
    vmBind.extensions = castToUint64(vmBindUserFence);
    return;
}

EngineCapabilities::Flags IoctlHelperPrelim20::getEngineCapabilitiesFlags(uint64_t capabilities) const {
    EngineCapabilities::Flags flags{};
    flags.copyClassSaturateLink = isValueSet(capabilities, PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK);
    flags.copyClassSaturatePCIE = isValueSet(capabilities, PRELIM_I915_COPY_CLASS_CAP_SATURATE_PCIE);

    return flags;
}

std::optional<uint32_t> IoctlHelperPrelim20::getVmAdviseAtomicAttribute() {
    switch (NEO::debugManager.flags.SetVmAdviseAtomicAttribute.get()) {
    case 0:
        return PRELIM_I915_VM_ADVISE_ATOMIC_NONE;
    case 1:
        return PRELIM_I915_VM_ADVISE_ATOMIC_DEVICE;
    default:
        return PRELIM_I915_VM_ADVISE_ATOMIC_SYSTEM;
    }
}

prelim_drm_i915_gem_vm_bind translateVmBindParamsToPrelimStruct(const VmBindParams &vmBindParams) {
    prelim_drm_i915_gem_vm_bind vmBind{};
    vmBind.vm_id = vmBindParams.vmId;
    vmBind.handle = vmBindParams.handle;
    vmBind.start = vmBindParams.start;
    vmBind.offset = vmBindParams.offset;
    vmBind.length = vmBindParams.length;
    vmBind.flags = vmBindParams.flags;
    vmBind.extensions = vmBindParams.extensions;
    return vmBind;
}

int IoctlHelperPrelim20::vmBind(const VmBindParams &vmBindParams) {
    // Expect canonical address
    DEBUG_BREAK_IF(drm.getRootDeviceEnvironment().getGmmHelper()->canonize(vmBindParams.start) != vmBindParams.start);

    auto prelimVmBind = translateVmBindParamsToPrelimStruct(vmBindParams);
    return IoctlHelper::ioctl(DrmIoctl::gemVmBind, &prelimVmBind);
}

int IoctlHelperPrelim20::vmUnbind(const VmBindParams &vmBindParams) {
    // Expect canonical address
    DEBUG_BREAK_IF(drm.getRootDeviceEnvironment().getGmmHelper()->canonize(vmBindParams.start) != vmBindParams.start);

    auto prelimVmBind = translateVmBindParamsToPrelimStruct(vmBindParams);
    return IoctlHelper::ioctl(DrmIoctl::gemVmUnbind, &prelimVmBind);
}

int IoctlHelperPrelim20::getResetStats(ResetStats &resetStats, uint32_t *status, ResetStatsFault *resetStatsFault) {
    prelim_drm_i915_reset_stats prelimResetStats{};
    prelimResetStats.ctx_id = resetStats.contextId;
    prelimResetStats.flags = resetStats.flags;

    const auto retVal = ioctl(DrmIoctl::getResetStatsPrelim, &prelimResetStats);
    if (retVal != 0) {
        return ioctl(DrmIoctl::getResetStats, &resetStats);
    }
    resetStats.resetCount = prelimResetStats.reset_count;
    resetStats.batchActive = prelimResetStats.batch_active;
    resetStats.batchPending = prelimResetStats.batch_pending;
    if (status) {
        *status = prelimResetStats.status;
    }
    if (resetStatsFault) {
        auto fault = reinterpret_cast<ResetStatsFault *>(&(prelimResetStats.fault));
        *resetStatsFault = *fault;
    }

    return retVal;
}

UuidRegisterResult IoctlHelperPrelim20::registerUuid(const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) {
    prelim_drm_i915_uuid_control uuidControl = {};
    memcpy_s(uuidControl.uuid, sizeof(uuidControl.uuid), uuid.c_str(), uuid.size());
    uuidControl.uuid_class = uuidClass;
    uuidControl.ptr = ptr;
    uuidControl.size = size;

    const auto retVal = IoctlHelper::ioctl(DrmIoctl::uuidRegister, &uuidControl);

    return {
        retVal,
        uuidControl.handle,
    };
}

UuidRegisterResult IoctlHelperPrelim20::registerStringClassUuid(const std::string &uuid, uint64_t ptr, uint64_t size) {
    return registerUuid(uuid, PRELIM_I915_UUID_CLASS_STRING, ptr, size);
}

int IoctlHelperPrelim20::unregisterUuid(uint32_t handle) {
    prelim_drm_i915_uuid_control uuidControl = {};
    uuidControl.handle = handle;

    return IoctlHelper::ioctl(DrmIoctl::uuidUnregister, &uuidControl);
}

bool IoctlHelperPrelim20::isContextDebugSupported() {
    GemContextParam ctxParam = {};
    ctxParam.size = 0;
    ctxParam.param = PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS;
    ctxParam.contextId = 0;
    ctxParam.value = 0;

    const auto retVal = IoctlHelper::ioctl(DrmIoctl::gemContextGetparam, &ctxParam);
    return retVal == 0 && ctxParam.value == (PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP << 32);
}

int IoctlHelperPrelim20::setContextDebugFlag(uint32_t drmContextId) {
    GemContextParam ctxParam = {};
    ctxParam.size = 0;
    ctxParam.param = PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS;
    ctxParam.contextId = drmContextId;
    ctxParam.value = PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP << 32 | PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP;

    return IoctlHelper::ioctl(DrmIoctl::gemContextSetparam, &ctxParam);
}

bool IoctlHelperPrelim20::isDebugAttachAvailable() {
    return true;
}

unsigned int IoctlHelperPrelim20::getIoctlRequestValue(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemVmBind:
        return PRELIM_DRM_IOCTL_I915_GEM_VM_BIND;
    case DrmIoctl::gemVmUnbind:
        return PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND;
    case DrmIoctl::gemWaitUserFence:
        return PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE;
    case DrmIoctl::gemCreateExt:
        return PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT;
    case DrmIoctl::gemVmAdvise:
        return PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE;
    case DrmIoctl::gemVmPrefetch:
        return PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH;
    case DrmIoctl::uuidRegister:
        return PRELIM_DRM_IOCTL_I915_UUID_REGISTER;
    case DrmIoctl::uuidUnregister:
        return PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER;
    case DrmIoctl::debuggerOpen:
        return PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN;
    case DrmIoctl::gemClosReserve:
        return PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE;
    case DrmIoctl::gemClosFree:
        return PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE;
    case DrmIoctl::gemCacheReserve:
        return PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE;
    case DrmIoctl::getResetStatsPrelim:
        return PRELIM_DRM_IOCTL_I915_GET_RESET_STATS;
    case DrmIoctl::syncObjFdToHandle:
        return DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE;
    case DrmIoctl::syncObjWait:
        return DRM_IOCTL_SYNCOBJ_WAIT;
    case DrmIoctl::syncObjSignal:
        return DRM_IOCTL_SYNCOBJ_SIGNAL;
    case DrmIoctl::syncObjTimelineWait:
        return DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT;
    case DrmIoctl::syncObjTimelineSignal:
        return DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL;
    default:
        return IoctlHelperI915::getIoctlRequestValue(ioctlRequest);
    }
}

int IoctlHelperPrelim20::getDrmParamValue(DrmParam drmParam) const {
    switch (drmParam) {
    case DrmParam::engineClassCompute:
        return prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE;
    case DrmParam::paramHasVmBind:
        return PRELIM_I915_PARAM_HAS_VM_BIND;
    case DrmParam::paramHasPageFault:
        return PRELIM_I915_PARAM_HAS_PAGE_FAULT;
    case DrmParam::queryHwconfigTable:
        return PRELIM_DRM_I915_QUERY_HWCONFIG_TABLE;
    case DrmParam::queryComputeSlices:
        return PRELIM_DRM_I915_QUERY_COMPUTE_SUBSLICES;
    default:
        return IoctlHelperI915::getDrmParamValueBase(drmParam);
    }
}
std::string IoctlHelperPrelim20::getDrmParamString(DrmParam drmParam) const {
    switch (drmParam) {
    case DrmParam::paramHasVmBind:
        return "PRELIM_I915_PARAM_HAS_VM_BIND";
    case DrmParam::paramHasPageFault:
        return "PRELIM_I915_PARAM_HAS_PAGE_FAULT";
    default:
        return IoctlHelperI915::getDrmParamString(drmParam);
    }
}

std::string IoctlHelperPrelim20::getIoctlString(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemVmBind:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_BIND";
    case DrmIoctl::gemVmUnbind:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND";
    case DrmIoctl::gemWaitUserFence:
        return "PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE";
    case DrmIoctl::gemCreateExt:
        return "PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT";
    case DrmIoctl::gemVmAdvise:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE";
    case DrmIoctl::gemVmPrefetch:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH";
    case DrmIoctl::uuidRegister:
        return "PRELIM_DRM_IOCTL_I915_UUID_REGISTER";
    case DrmIoctl::uuidUnregister:
        return "PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER";
    case DrmIoctl::debuggerOpen:
        return "PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN";
    case DrmIoctl::gemClosReserve:
        return "PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE";
    case DrmIoctl::gemClosFree:
        return "PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE";
    case DrmIoctl::gemCacheReserve:
        return "PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE";
    case DrmIoctl::getResetStatsPrelim:
        return "PRELIM_DRM_IOCTL_I915_GET_RESET_STATS";
    default:
        return IoctlHelperI915::getIoctlString(ioctlRequest);
    }
}

bool IoctlHelperPrelim20::checkIfIoctlReinvokeRequired(int error, DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::debuggerOpen:
        return (error == EINTR || error == EAGAIN);
    case DrmIoctl::gemExecbuffer2:
        if (handleExecBufferInNonBlockMode) {
            return (error == EINTR || error == EBUSY || error == -EBUSY);
        } else {
            return IoctlHelper::checkIfIoctlReinvokeRequired(error, ioctlRequest);
        }
    default:
        break;
    }
    return IoctlHelper::checkIfIoctlReinvokeRequired(error, ioctlRequest);
}

bool IoctlHelperPrelim20::getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) {
    Query query = {};
    QueryItem queryItem = {};
    PrelimI915::prelim_drm_i915_query_fabric_info info = {};
    info.fabric_id = fabricId;

    queryItem.queryId = PRELIM_DRM_I915_QUERY_FABRIC_INFO;
    queryItem.length = static_cast<int32_t>(sizeof(info));
    queryItem.dataPtr = reinterpret_cast<uint64_t>(&info);
    queryItem.flags = 0;

    query.itemsPtr = reinterpret_cast<uint64_t>(&queryItem);
    query.numItems = 1;
    auto ret = IoctlHelper::ioctl(DrmIoctl::query, &query);
    if (ret != 0) {
        return false;
    }

    if (info.latency < 10 || info.bandwidth == 0) {
        return false;
    }

    // Latency is in tenths of path length. 10 == 1 fabric link between src and dst
    // 1 link = zero hops
    latency = (info.latency / 10) - 1;
    bandwidth = info.bandwidth;
    return true;
}

bool IoctlHelperPrelim20::isWaitBeforeBindRequired(bool bind) const {
    bool userFenceOnUnbind = false;
    if (debugManager.flags.EnableUserFenceUponUnbind.get() != -1) {
        userFenceOnUnbind = !!debugManager.flags.EnableUserFenceUponUnbind.get();
    }

    return (bind || userFenceOnUnbind);
}

void *IoctlHelperPrelim20::pciBarrierMmap() {
    static constexpr uint64_t pciBarrierMmapOffset = 0x50 << 12;
    return SysCalls::mmap(NULL, MemoryConstants::pageSize, PROT_WRITE, MAP_SHARED, drm.getFileDescriptor(), pciBarrierMmapOffset);
}

bool IoctlHelperPrelim20::queryHwIpVersion(EngineClassInstance &engineInfo, HardwareIpVersion &ipVersion, int &ret) {
    QueryItem queryItem{};
    queryItem.queryId = PRELIM_DRM_I915_QUERY_HW_IP_VERSION;

    Query query{};
    query.itemsPtr = reinterpret_cast<uint64_t>(&queryItem);
    query.numItems = 1u;
    ret = ioctl(DrmIoctl::query, &query);

    if (ret != 0) {
        return false;
    }

    if (queryItem.length != sizeof(prelim_drm_i915_query_hw_ip_version)) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n",
                           "Size got from PRELIM_DRM_I915_QUERY_HW_IP_VERSION query does not match PrelimI915::prelim_drm_i915_query_hw_ip_version size");
        return false;
    }

    prelim_drm_i915_query_hw_ip_version queryHwIpVersion{};
    queryHwIpVersion.engine.engine_class = engineInfo.engineClass;
    queryHwIpVersion.engine.engine_instance = engineInfo.engineInstance;
    queryItem.dataPtr = reinterpret_cast<uint64_t>(&queryHwIpVersion);

    ret = ioctl(DrmIoctl::query, &query);
    if (ret != 0) {
        return false;
    }

    ipVersion.architecture = queryHwIpVersion.arch;
    ipVersion.release = queryHwIpVersion.release;
    ipVersion.revision = queryHwIpVersion.stepping;

    return true;
}

bool IoctlHelperPrelim20::initialize() {
    initializeGetGpuTimeFunction();
    return true;
}

void IoctlHelperPrelim20::setupIpVersion() {
    auto &rootDeviceEnvironment = drm.getRootDeviceEnvironment();
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    auto &productHelper = drm.getRootDeviceEnvironment().getHelper<ProductHelper>();

    EngineClassInstance engineInfo = {static_cast<uint16_t>(getDrmParamValue(DrmParam::engineClassRender)), 0};
    int ret = 0;

    auto isPlatformQuerySupported = productHelper.isPlatformQuerySupported();
    bool result = false;

    if (isPlatformQuerySupported) {
        result = queryHwIpVersion(engineInfo, hwInfo->ipVersion, ret);

        if (result == false && ret != 0) {
            int err = drm.getErrno();
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                               "ioctl(PRELIM_DRM_I915_QUERY_HW_IP_VERSION) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        }
    }

    if (result == false) {
        IoctlHelper::setupIpVersion();
    }
}

bool IoctlHelperPrelim20::registerResourceClasses() {
    for (auto &classNameUUID : classNamesToUuid) {
        auto className = classNameUUID.first;
        auto uuid = classNameUUID.second;

        const auto result = registerStringClassUuid(uuid, (uintptr_t)className, strnlen_s(className, 100));
        if (result.retVal != 0) {
            return false;
        }

        classHandles.push_back(result.handle);
    }
    return true;
}

uint32_t IoctlHelperPrelim20::registerIsaCookie(uint32_t isaHandle) {
    auto uuid = generateUUID();

    const auto result = registerUuid(uuid, isaHandle, 0, 0);

    PRINT_DEBUGGER_INFO_LOG("PRELIM_DRM_IOCTL_I915_UUID_REGISTER: isa handle = %lu, uuid = %s, data = %p, handle = %lu, ret = %d\n", isaHandle, std::string(uuid, 36).c_str(), 0, result.handle, result.retVal);
    if (result.retVal != 0) {
        int err = errno;

        CREATE_DEBUG_STRING(str, "ioctl(PRELIM_DRM_IOCTL_I915_UUID_REGISTER) failed with %d. errno=%d(%s)\n", result.retVal, err, strerror(err));
        drm.getRootDeviceEnvironment().executionEnvironment.setErrorDescription(std::string(str.get()));
        DEBUG_BREAK_IF(true);
    }

    return result.handle;
}

void IoctlHelperPrelim20::unregisterResource(uint32_t handle) {
    PRINT_DEBUGGER_INFO_LOG("PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER: handle = %lu\n", handle);
    [[maybe_unused]] const auto ret = unregisterUuid(handle);
    if (ret != 0) {
        int err = errno;

        CREATE_DEBUG_STRING(str, "ioctl(PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        drm.getRootDeviceEnvironment().executionEnvironment.setErrorDescription(std::string(str.get()));
        DEBUG_BREAK_IF(true);
    }
}

std::string IoctlHelperPrelim20::generateUUID() {
    const char uuidString[] = "00000000-0000-0000-%04" SCNx64 "-%012" SCNx64;
    char buffer[36 + 1] = "00000000-0000-0000-0000-000000000000";
    uuid++;

    UNRECOVERABLE_IF(uuid == 0xFFFFFFFFFFFFFFFF);

    uint64_t parts[2] = {0, 0};
    parts[0] = uuid & 0xFFFFFFFFFFFF;
    parts[1] = (uuid & 0xFFFF000000000000) >> 48;
    snprintf(buffer, sizeof(buffer), uuidString, parts[1], parts[0]);

    return std::string(buffer, 36);
}

std::string IoctlHelperPrelim20::generateElfUUID(const void *data) {
    std::string elfClassUuid = classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::elf)].second;
    std::string uuiD1st = elfClassUuid.substr(0, 18);

    const char uuidString[] = "%s-%04" SCNx64 "-%012" SCNx64;
    char buffer[36 + 1] = "00000000-0000-0000-0000-000000000000";

    uint64_t parts[2] = {0, 0};
    parts[0] = reinterpret_cast<uintptr_t>(data) & 0xFFFFFFFFFFFF;
    parts[1] = (reinterpret_cast<uintptr_t>(data) & 0xFFFF000000000000) >> 48;
    snprintf(buffer, sizeof(buffer), uuidString, uuiD1st.c_str(), parts[1], parts[0]);

    return std::string(buffer, 36);
}

uint32_t IoctlHelperPrelim20::registerResource(DrmResourceClass classType, const void *data, size_t size) {
    const auto classIndex = static_cast<uint32_t>(classType);
    if (classHandles.size() <= classIndex) {
        return 0;
    }

    std::string uuid;
    if (classType == NEO::DrmResourceClass::elf) {
        uuid = generateElfUUID(data);
    } else {
        uuid = generateUUID();
    }

    const auto uuidClass = classHandles[classIndex];
    const auto ptr = size > 0 ? (uintptr_t)data : 0;
    const auto result = registerUuid(uuid, uuidClass, ptr, size);

    PRINT_DEBUGGER_INFO_LOG("PRELIM_DRM_IOCTL_I915_UUID_REGISTER: classType = %d, uuid = %s, data = %p, handle = %lu, ret = %d\n", (int)classType, std::string(uuid, 36).c_str(), ptr, result.handle, result.retVal);

    if (result.retVal != 0) {
        int err = errno;

        CREATE_DEBUG_STRING(str, "ioctl(PRELIM_DRM_IOCTL_I915_UUID_REGISTER) failed with %d. errno=%d(%s)\n", result.retVal, err, strerror(err));
        drm.getRootDeviceEnvironment().executionEnvironment.setErrorDescription(std::string(str.get()));
        DEBUG_BREAK_IF(true);
    }

    return result.handle;
}

uint32_t IoctlHelperPrelim20::notifyFirstCommandQueueCreated(const void *data, size_t size) {
    const auto result = registerStringClassUuid(uuidL0CommandQueueHash, (uintptr_t)data, size);
    DEBUG_BREAK_IF(result.retVal);
    return result.handle;
}

void IoctlHelperPrelim20::notifyLastCommandQueueDestroyed(uint32_t handle) {
    unregisterResource(handle);
}

int IoctlHelperPrelim20::getEuDebugSysFsEnable() {
    char enabledEuDebug = '0';
    std::string sysFsPciPath = drm.getSysFsPciPath();
    std::string euDebugPath = sysFsPciPath + "/prelim_enable_eu_debug";

    FILE *fileDescriptor = IoFunctions::fopenPtr(euDebugPath.c_str(), "r");
    if (fileDescriptor) {
        [[maybe_unused]] auto bytesRead = IoFunctions::freadPtr(&enabledEuDebug, 1, 1, fileDescriptor);
        IoFunctions::fclosePtr(fileDescriptor);
    }

    return enabledEuDebug - '0';
}

bool IoctlHelperPrelim20::validPageFault(uint16_t flags) {
    if ((flags & I915_RESET_STATS_FAULT_VALID) != 0) {
        return true;
    }
    return false;
}

uint32_t IoctlHelperPrelim20::getStatusForResetStats(bool banned) {
    uint32_t retVal = 0u;
    if (banned) {
        retVal |= I915_RESET_STATS_BANNED;
    }
    return retVal;
}

void IoctlHelperPrelim20::registerBOBindHandle(Drm *drm, DrmAllocation *drmAllocation) {
    DrmResourceClass resourceClass = DrmResourceClass::maxSize;

    switch (drmAllocation->getAllocationType()) {
    case AllocationType::debugContextSaveArea:
        resourceClass = DrmResourceClass::contextSaveArea;
        break;
    case AllocationType::debugSbaTrackingBuffer:
        resourceClass = DrmResourceClass::sbaTrackingBuffer;
        break;
    case AllocationType::kernelIsa:
        resourceClass = DrmResourceClass::isa;
        break;
    case AllocationType::debugModuleArea:
        resourceClass = DrmResourceClass::moduleHeapDebugArea;
        break;
    default:
        break;
    }

    if (resourceClass != DrmResourceClass::maxSize) {
        auto handle = 0;
        if (resourceClass == DrmResourceClass::isa) {
            auto deviceBitfiled = static_cast<uint32_t>(drmAllocation->storageInfo.subDeviceBitfield.to_ulong());
            handle = drm->registerResource(resourceClass, &deviceBitfiled, sizeof(deviceBitfiled));
        } else {
            uint64_t gpuAddress = drmAllocation->getGpuAddress();
            handle = drm->registerResource(resourceClass, &gpuAddress, sizeof(gpuAddress));
        }

        drmAllocation->addRegisteredBoBindHandle(handle);

        auto &bos = drmAllocation->getBOs();
        uint32_t boIndex = 0u;

        for (auto bo : bos) {
            if (bo) {
                bo->addBindExtHandle(handle);
                bo->markForCapture();
                if (resourceClass == DrmResourceClass::isa && drmAllocation->storageInfo.tileInstanced == true) {
                    auto cookieHandle = drm->registerIsaCookie(handle);
                    bo->addBindExtHandle(cookieHandle);
                    drmAllocation->addRegisteredBoBindHandle(cookieHandle);
                }
                auto storageInfo = drmAllocation->storageInfo;
                if (resourceClass == DrmResourceClass::sbaTrackingBuffer && drmAllocation->getOsContext()) {
                    auto deviceIndex = [=]() -> uint32_t {
                        if (storageInfo.tileInstanced == true) {
                            return boIndex;
                        }
                        auto deviceBitfield = storageInfo.subDeviceBitfield;
                        return deviceBitfield.any() ? static_cast<uint32_t>(Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()))) : 0u;
                    }();

                    auto contextId = drmAllocation->getOsContext()->getOfflineDumpContextId(deviceIndex);
                    auto externalHandle = drm->registerResource(resourceClass, &contextId, sizeof(uint64_t));

                    bo->addBindExtHandle(externalHandle);
                    drmAllocation->addRegisteredBoBindHandle(externalHandle);
                }

                bo->requireImmediateBinding(true);
            }
            boIndex++;
        }
    }
}

static_assert(sizeof(MemoryClassInstance) == sizeof(prelim_drm_i915_gem_memory_class_instance));
static_assert(offsetof(MemoryClassInstance, memoryClass) == offsetof(prelim_drm_i915_gem_memory_class_instance, memory_class));
static_assert(offsetof(MemoryClassInstance, memoryInstance) == offsetof(prelim_drm_i915_gem_memory_class_instance, memory_instance));
static_assert(sizeof(ResetStatsFault) == sizeof(prelim_drm_i915_reset_stats::fault));
} // namespace NEO
