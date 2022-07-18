/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/ioctl.h>

namespace NEO {

bool IoctlHelperPrelim20::isVmBindAvailable() {
    int vmBindSupported = 0;
    GetParam getParam{};
    getParam.param = PRELIM_I915_PARAM_HAS_VM_BIND;
    getParam.value = &vmBindSupported;
    int retVal = IoctlHelper::ioctl(DrmIoctl::Getparam, &getParam);
    if (retVal) {
        return false;
    }
    return vmBindSupported;
}

uint32_t IoctlHelperPrelim20::createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, std::optional<uint32_t> vmId) {
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
    if (vmId != std::nullopt) {
        vmPrivate.base.name = PRELIM_I915_GEM_CREATE_EXT_VM_PRIVATE;
        vmPrivate.vm_id = vmId.value();
        setparamRegion.base.next_extension = reinterpret_cast<uintptr_t>(&vmPrivate);
    }

    prelim_drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;
    createExt.extensions = reinterpret_cast<uintptr_t>(&setparamRegion);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Performing GEM_CREATE_EXT with { size: %lu, param: 0x%llX",
                     allocSize, regionParam.param);

    if (DebugManager.flags.PrintBOCreateDestroyResult.get()) {
        for (uint32_t i = 0; i < regionsSize; i++) {
            auto region = regions[i];
            printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, ", memory class: %d, memory instance: %d",
                             region.memory_class, region.memory_instance);
        }
        printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "%s", " }\n");
    }

    auto ret = IoctlHelper::ioctl(DrmIoctl::GemCreateExt, &createExt);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    handle = createExt.handle;
    return ret;
}

CacheRegion IoctlHelperPrelim20::closAlloc() {
    struct prelim_drm_i915_gem_clos_reserve clos = {};

    int ret = IoctlHelper::ioctl(DrmIoctl::GemClosReserve, &clos);
    if (ret != 0) {
        int err = errno;
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_CLOS_RESERVE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(true);
        return CacheRegion::None;
    }

    return static_cast<CacheRegion>(clos.clos_index);
}

uint16_t IoctlHelperPrelim20::closAllocWays(CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) {
    struct prelim_drm_i915_gem_cache_reserve cache = {};

    cache.clos_index = static_cast<uint16_t>(closIndex);
    cache.cache_level = cacheLevel;
    cache.num_ways = numWays;

    int ret = IoctlHelper::ioctl(DrmIoctl::GemCacheReserve, &cache);
    if (ret != 0) {
        int err = errno;
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_CACHE_RESERVE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        return 0;
    }

    return cache.num_ways;
}

CacheRegion IoctlHelperPrelim20::closFree(CacheRegion closIndex) {
    struct prelim_drm_i915_gem_clos_free clos = {};

    clos.clos_index = static_cast<uint16_t>(closIndex);

    int ret = IoctlHelper::ioctl(DrmIoctl::GemClosFree, &clos);
    if (ret != 0) {
        int err = errno;
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_CLOS_FREE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(true);
        return CacheRegion::None;
    }

    return closIndex;
}

int IoctlHelperPrelim20::waitUserFence(uint32_t ctxId, uint64_t address,
                                       uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) {
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

    return IoctlHelper::ioctl(DrmIoctl::GemWaitUserFence, &wait);
}

uint32_t IoctlHelperPrelim20::getAtomicAdvise(bool isNonAtomic) {
    return isNonAtomic ? PRELIM_I915_VM_ADVISE_ATOMIC_NONE : PRELIM_I915_VM_ADVISE_ATOMIC_SYSTEM;
}

uint32_t IoctlHelperPrelim20::getPreferredLocationAdvise() {
    return PRELIM_I915_VM_ADVISE_PREFERRED_LOCATION;
}

bool IoctlHelperPrelim20::setVmBoAdvise(int32_t handle, uint32_t attribute, void *region) {
    prelim_drm_i915_gem_vm_advise vmAdvise{};

    vmAdvise.handle = handle;
    vmAdvise.attribute = attribute;
    if (region != nullptr) {
        vmAdvise.region = *reinterpret_cast<prelim_drm_i915_gem_memory_class_instance *>(region);
    }

    int ret = IoctlHelper::ioctl(DrmIoctl::GemVmAdvise, &vmAdvise);
    if (ret != 0) {
        int err = errno;
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRELIM_DRM_I915_GEM_VM_ADVISE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(true);
        return false;
    }
    return true;
}

bool IoctlHelperPrelim20::setVmPrefetch(uint64_t start, uint64_t length, uint32_t region) {
    prelim_drm_i915_gem_vm_prefetch vmPrefetch{};

    vmPrefetch.length = length;
    vmPrefetch.region = region;
    vmPrefetch.start = start;

    int ret = IoctlHelper::ioctl(DrmIoctl::GemVmPrefetch, &vmPrefetch);
    if (ret != 0) {
        int err = errno;
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRELIM_DRM_I915_GEM_VM_PREFETCH) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
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

int IoctlHelperPrelim20::execBuffer(ExecBuffer *execBuffer, uint64_t completionGpuAddress, uint32_t counterValue) {
    prelim_drm_i915_gem_execbuffer_ext_user_fence fenceObject = {};
    if (completionGpuAddress != 0) {
        fenceObject.base.name = PRELIM_DRM_I915_GEM_EXECBUFFER_EXT_USER_FENCE;
        fenceObject.addr = completionGpuAddress;
        fenceObject.value = counterValue;

        auto &drmExecBuffer = *reinterpret_cast<drm_i915_gem_execbuffer2 *>(execBuffer->data);
        drmExecBuffer.flags |= I915_EXEC_USE_EXTENSIONS;
        drmExecBuffer.num_cliprects = 0;
        drmExecBuffer.cliprects_ptr = castToUint64(&fenceObject);
    }

    return IoctlHelper::ioctl(DrmIoctl::GemExecbuffer2, execBuffer);
}

bool IoctlHelperPrelim20::completionFenceExtensionSupported(const bool isVmBindAvailable) {
    return isVmBindAvailable;
}

std::unique_ptr<uint8_t[]> IoctlHelperPrelim20::prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) {
    std::unique_ptr<prelim_drm_i915_vm_bind_ext_uuid[]> extensions;
    extensions = std::make_unique<prelim_drm_i915_vm_bind_ext_uuid[]>(bindExtHandles.size());
    memset(extensions.get(), 0, sizeof(prelim_drm_i915_vm_bind_ext_uuid) * bindExtHandles.size());

    extensions[0].uuid_handle = bindExtHandles[0];
    extensions[0].base.name = PRELIM_I915_VM_BIND_EXT_UUID;

    for (size_t i = 1; i < bindExtHandles.size(); i++) {
        extensions[i - 1].base.next_extension = reinterpret_cast<uint64_t>(&extensions[i]);
        extensions[i].uuid_handle = bindExtHandles[i];
        extensions[i].base.name = PRELIM_I915_VM_BIND_EXT_UUID;
    }
    return std::unique_ptr<uint8_t[]>(reinterpret_cast<uint8_t *>(extensions.release()));
}

uint64_t IoctlHelperPrelim20::getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) {
    uint64_t flags = 0u;
    if (bindCapture) {
        flags |= PRELIM_I915_GEM_VM_BIND_CAPTURE;
    }
    if (bindImmediate) {
        flags |= PRELIM_I915_GEM_VM_BIND_IMMEDIATE;
    }
    if (bindMakeResident) {
        flags |= PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT;
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

uint32_t IoctlHelperPrelim20::queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) {
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
    auto ret = IoctlHelper::ioctl(DrmIoctl::Query, &query);
    for (auto i = 0u; i < i915Distances.size(); i++) {
        distanceInfos[i].distance = i915Distances[i].distance;
    }
    return ret;
}

std::optional<DrmParam> IoctlHelperPrelim20::getHasPageFaultParamId() {
    return DrmParam::ParamHasPageFault;
};

bool IoctlHelperPrelim20::getEuStallProperties(std::array<uint64_t, 12u> &properties, uint64_t dssBufferSize, uint64_t samplingRate,
                                               uint64_t pollPeriod, uint64_t engineInstance, uint64_t notifyNReports) {
    properties[0] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_BUF_SZ;
    properties[1] = dssBufferSize;
    properties[2] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_SAMPLE_RATE;
    properties[3] = samplingRate;
    properties[4] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_POLL_PERIOD;
    properties[5] = pollPeriod;
    properties[6] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_CLASS;
    properties[7] = prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE;
    properties[8] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_INSTANCE;
    properties[9] = engineInstance;
    properties[10] = prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_EVENT_REPORT_COUNT;
    properties[11] = notifyNReports;

    return true;
}

uint32_t IoctlHelperPrelim20::getEuStallFdParameter() {
    return PRELIM_I915_PERF_FLAG_FD_EU_STALL;
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

    auto ioctlResult = ioctlHelper.ioctl(DrmIoctl::GemContextCreateExt, &gcc);
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
    if (DebugManager.flags.AccessCountersTrigger.get() != -1) {
        trigger = static_cast<uint16_t>(DebugManager.flags.AccessCountersTrigger.get());
    }
    uint8_t granularity = PRELIM_I915_CONTEXT_ACG_2M;
    if (DebugManager.flags.AccessCountersGranularity.get() != -1) {
        granularity = static_cast<uint8_t>(DebugManager.flags.AccessCountersGranularity.get());
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

std::optional<uint64_t> IoctlHelperPrelim20::getCopyClassSaturatePCIECapability() {
    return PRELIM_I915_COPY_CLASS_CAP_SATURATE_PCIE;
}

std::optional<uint64_t> IoctlHelperPrelim20::getCopyClassSaturateLinkCapability() {
    return PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK;
}

uint32_t IoctlHelperPrelim20::getVmAdviseAtomicAttribute() {
    switch (NEO::DebugManager.flags.SetVmAdviseAtomicAttribute.get()) {
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
    auto prelimVmBind = translateVmBindParamsToPrelimStruct(vmBindParams);
    return IoctlHelper::ioctl(DrmIoctl::GemVmBind, &prelimVmBind);
}

int IoctlHelperPrelim20::vmUnbind(const VmBindParams &vmBindParams) {
    auto prelimVmBind = translateVmBindParamsToPrelimStruct(vmBindParams);
    return IoctlHelper::ioctl(DrmIoctl::GemVmUnbind, &prelimVmBind);
}

UuidRegisterResult IoctlHelperPrelim20::registerUuid(const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) {
    prelim_drm_i915_uuid_control uuidControl = {};
    memcpy_s(uuidControl.uuid, sizeof(uuidControl.uuid), uuid.c_str(), uuid.size());
    uuidControl.uuid_class = uuidClass;
    uuidControl.ptr = ptr;
    uuidControl.size = size;

    const auto retVal = IoctlHelper::ioctl(DrmIoctl::UuidRegister, &uuidControl);

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

    return IoctlHelper::ioctl(DrmIoctl::UuidUnregister, &uuidControl);
}

bool IoctlHelperPrelim20::isContextDebugSupported() {
    drm_i915_gem_context_param ctxParam = {};
    ctxParam.size = 0;
    ctxParam.param = PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS;
    ctxParam.ctx_id = 0;
    ctxParam.value = 0;

    const auto retVal = IoctlHelper::ioctl(DrmIoctl::GemContextGetparam, &ctxParam);
    return retVal == 0 && ctxParam.value == (PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP << 32);
}

int IoctlHelperPrelim20::setContextDebugFlag(uint32_t drmContextId) {
    drm_i915_gem_context_param ctxParam = {};
    ctxParam.size = 0;
    ctxParam.param = PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS;
    ctxParam.ctx_id = drmContextId;
    ctxParam.value = PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP << 32 | PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP;

    return IoctlHelper::ioctl(DrmIoctl::GemContextSetparam, &ctxParam);
}

bool IoctlHelperPrelim20::isDebugAttachAvailable() {
    return true;
}

unsigned int IoctlHelperPrelim20::getIoctlRequestValue(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::GemVmBind:
        return PRELIM_DRM_IOCTL_I915_GEM_VM_BIND;
    case DrmIoctl::GemVmUnbind:
        return PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND;
    case DrmIoctl::GemWaitUserFence:
        return PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE;
    case DrmIoctl::GemCreateExt:
        return PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT;
    case DrmIoctl::GemVmAdvise:
        return PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE;
    case DrmIoctl::GemVmPrefetch:
        return PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH;
    case DrmIoctl::UuidRegister:
        return PRELIM_DRM_IOCTL_I915_UUID_REGISTER;
    case DrmIoctl::UuidUnregister:
        return PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER;
    case DrmIoctl::DebuggerOpen:
        return PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN;
    case DrmIoctl::GemClosReserve:
        return PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE;
    case DrmIoctl::GemClosFree:
        return PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE;
    case DrmIoctl::GemCacheReserve:
        return PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE;
    default:
        return getIoctlRequestValueBase(ioctlRequest);
    }
}

int IoctlHelperPrelim20::getDrmParamValue(DrmParam drmParam) const {
    switch (drmParam) {
    case DrmParam::EngineClassCompute:
        return prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE;
    case DrmParam::ParamHasVmBind:
        return PRELIM_I915_PARAM_HAS_VM_BIND;
    case DrmParam::ParamHasPageFault:
        return PRELIM_I915_PARAM_HAS_PAGE_FAULT;
    case DrmParam::QueryHwconfigTable:
        return PRELIM_DRM_I915_QUERY_HWCONFIG_TABLE;
    case DrmParam::QueryComputeSlices:
        return PRELIM_DRM_I915_QUERY_COMPUTE_SLICES;
    default:
        return getDrmParamValueBase(drmParam);
    }
}
std::string IoctlHelperPrelim20::getDrmParamString(DrmParam drmParam) const {
    switch (drmParam) {
    case DrmParam::ParamHasVmBind:
        return "PRELIM_I915_PARAM_HAS_VM_BIND";
    case DrmParam::ParamHasPageFault:
        return "PRELIM_I915_PARAM_HAS_PAGE_FAULT";
    default:
        return getDrmParamStringBase(drmParam);
    }
}

std::string IoctlHelperPrelim20::getIoctlString(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::GemVmBind:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_BIND";
    case DrmIoctl::GemVmUnbind:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND";
    case DrmIoctl::GemWaitUserFence:
        return "PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE";
    case DrmIoctl::GemCreateExt:
        return "PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT";
    case DrmIoctl::GemVmAdvise:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE";
    case DrmIoctl::GemVmPrefetch:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH";
    case DrmIoctl::UuidRegister:
        return "PRELIM_DRM_IOCTL_I915_UUID_REGISTER";
    case DrmIoctl::UuidUnregister:
        return "PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER";
    case DrmIoctl::DebuggerOpen:
        return "PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN";
    case DrmIoctl::GemClosReserve:
        return "PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE";
    case DrmIoctl::GemClosFree:
        return "PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE";
    case DrmIoctl::GemCacheReserve:
        return "PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE";
    default:
        return getIoctlStringBase(ioctlRequest);
    }
}

static_assert(sizeof(MemoryClassInstance) == sizeof(prelim_drm_i915_gem_memory_class_instance));
static_assert(offsetof(MemoryClassInstance, memoryClass) == offsetof(prelim_drm_i915_gem_memory_class_instance, memory_class));
static_assert(offsetof(MemoryClassInstance, memoryInstance) == offsetof(prelim_drm_i915_gem_memory_class_instance, memory_instance));

} // namespace NEO
