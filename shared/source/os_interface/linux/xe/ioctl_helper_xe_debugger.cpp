/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/linux/debugger_xe.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/linux/xe/xe_log_helper.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"

#include <algorithm>
#include <optional>

namespace NEO {

unsigned int IoctlHelperXe::getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::debuggerOpen:
        return euDebugInterface->getParamValue(EuDebugParam::connect);
    case DrmIoctl::metadataCreate:
        return euDebugInterface->getParamValue(EuDebugParam::metadataCreate);
    case DrmIoctl::metadataDestroy:
        return euDebugInterface->getParamValue(EuDebugParam::metadataDestroy);

    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

int IoctlHelperXe::debuggerOpenIoctl(DrmIoctl request, void *arg) {
    EuDebugConnect *connect = static_cast<EuDebugConnect *>(arg);
    auto ret = IoctlHelper::ioctl(request, arg);
    XELOG(" -> IoctlHelperXe::ioctl debuggerOpen pid=%llu r=%d\n",
          connect->pid, ret);
    return ret;
}

int IoctlHelperXe::debuggerMetadataCreateIoctl(DrmIoctl request, void *arg) {
    DebugMetadataCreate *metadata = static_cast<DebugMetadataCreate *>(arg);
    auto ret = IoctlHelper::ioctl(request, arg);
    XELOG(" -> IoctlHelperXe::ioctl metadataCreate type=%llu user_addr=%uul len=%uul\n id=%uul ret=%d, errno=%d\n",
          metadata->type, metadata->userAddr, metadata->len, metadata->metadataId, ret, errno);
    return ret;
}

int IoctlHelperXe::debuggerMetadataDestroyIoctl(DrmIoctl request, void *arg) {
    DebugMetadataDestroy *metadata = static_cast<DebugMetadataDestroy *>(arg);
    auto ret = IoctlHelper::ioctl(request, arg);
    XELOG(" -> IoctlHelperXe::ioctl metadataDestroy id=%llu r=%d\n",
          metadata->metadataId, ret);
    return ret;
}

int IoctlHelperXe::getEudebugExtProperty() {
    return euDebugInterface->getParamValue(EuDebugParam::execQueueSetPropertyEuDebug);
}

uint64_t IoctlHelperXe::getEudebugExtPropertyValue() {
    uint64_t val = euDebugInterface->getParamValue(EuDebugParam::execQueueSetPropertyValueEnable);
    if (euDebugInterface->isExecQueuePageFaultEnableSupported()) {
        val |= euDebugInterface->getParamValue(EuDebugParam::execQueueSetPropertyValuePageFaultEnable);
    }
    return val;
}

int IoctlHelperXe::getEuDebugSysFsEnable() {
    return euDebugInterface != nullptr ? 1 : 0;
}

uint32_t IoctlHelperXe::registerResource(DrmResourceClass classType, const void *data, size_t size) {
    if (euDebugInterface->getInterfaceType() != EuDebugInterfaceType::prelim) {
        return 0;
    }

    DebugMetadataCreate metadata = {};
    if (classType == DrmResourceClass::elf) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataElfBinary);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::l0ZebinModule) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataProgramModule);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::contextSaveArea) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataSipArea);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::sbaTrackingBuffer) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataSbaArea);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::moduleHeapDebugArea) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataModuleArea);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else {
        UNRECOVERABLE_IF(true);
    }
    [[maybe_unused]] auto retVal = IoctlHelperXe::ioctl(DrmIoctl::metadataCreate, &metadata);
    PRINT_DEBUGGER_INFO_LOG("DRM_XE_DEBUG_METADATA_CREATE: type=%llu user_addr=%llu len=%llu id=%llu\n",
                            metadata.type, metadata.userAddr, metadata.len, metadata.metadataId);
    DEBUG_BREAK_IF(retVal != 0);
    return metadata.metadataId;
}

void IoctlHelperXe::unregisterResource(uint32_t handle) {
    if (euDebugInterface->getInterfaceType() != EuDebugInterfaceType::prelim) {
        return;
    }
    DebugMetadataDestroy metadata = {};
    metadata.metadataId = handle;
    [[maybe_unused]] auto retVal = IoctlHelperXe::ioctl(DrmIoctl::metadataDestroy, &metadata);
    DEBUG_BREAK_IF(retVal != 0);
    PRINT_DEBUGGER_INFO_LOG("DRM_XE_DEBUG_METADATA_DESTROY: id=%llu\n", metadata.metadataId);
}

std::unique_ptr<uint8_t[]> IoctlHelperXe::prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles, uint64_t cookie) {
    if (euDebugInterface->getInterfaceType() != EuDebugInterfaceType::prelim) {
        return nullptr;
    }

    static_assert(std::is_trivially_destructible_v<VmBindOpExtAttachDebug>,
                  "Storage must be allowed to be reused without calling the destructor!");

    static_assert(alignof(VmBindOpExtAttachDebug) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                  "Alignment of a buffer returned via new[] operator must allow storing the required type!");

    XELOG(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    const auto bufferSize{sizeof(VmBindOpExtAttachDebug) * bindExtHandles.size()};
    std::unique_ptr<uint8_t[]> extensionsBuffer{new uint8_t[bufferSize]};

    auto extensions = new (extensionsBuffer.get()) VmBindOpExtAttachDebug[bindExtHandles.size()];
    std::memset(extensionsBuffer.get(), 0, bufferSize);

    extensions[0].metadataId = bindExtHandles[0];
    extensions[0].base.name = euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsAttachDebug);
    extensions[0].cookie = cookie;

    for (size_t i = 1; i < bindExtHandles.size(); i++) {
        extensions[i - 1].base.nextExtension = reinterpret_cast<uint64_t>(&extensions[i]);
        extensions[i].metadataId = bindExtHandles[i];
        extensions[i].base.name = euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsAttachDebug);
        extensions[i].cookie = cookie;
    }
    return extensionsBuffer;
}

uint64_t IoctlHelperXe::convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass resourceClass) {
    if (resourceClass == DrmResourceClass::moduleHeapDebugArea) {
        return euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataModuleArea);
    } else if (resourceClass == DrmResourceClass::contextSaveArea) {
        return euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataSipArea);
    } else if (resourceClass == DrmResourceClass::sbaTrackingBuffer) {
        return euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataSbaArea);
    } else {
        return 0;
    }
}

std::optional<std::vector<VmBindOpExtDebugData>> IoctlHelperXe::addDebugDataAndCreateBindOpVec(BufferObject *bo, uint32_t vmId, bool isAdd) {

    std::vector<VmBindOpExtDebugData> debugDataVec;
    auto resourceClass = bo->getDrmResourceClass();
    if (resourceClass == DrmResourceClass::moduleHeapDebugArea || resourceClass == DrmResourceClass::contextSaveArea || resourceClass == DrmResourceClass::sbaTrackingBuffer) {
        VmBindOpExtDebugData debugData = {};
        debugData.base.name = euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionDebugDataName);
        debugData.base.nextExtension = 0;
        debugData.base.pad = 0;
        debugData.addr = bo->peekAddress();
        debugData.range = bo->peekSize();
        debugData.flags = euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag);
        debugData.offset = 0u;
        debugData.pseudopath = convertDrmResourceClassToXeDebugPseudoPath(resourceClass);
        debugDataVec.push_back(debugData);

    } else if (resourceClass == DrmResourceClass::isa) {
        auto debugDataHandle = bo->getIsaDebugDataHandle();
        UNRECOVERABLE_IF(!debugDataHandle.has_value())
        auto &isaDebugData = drm.isaDebugDataMap[debugDataHandle.value()];
        if (isAdd) {
            IsaDebugData::DebugDataBindInfo bindInfo = {bo->peekAddress(), bo->peekSize()};
            isaDebugData.bindInfoMap[vmId].push_back(bindInfo);
            PRINT_DEBUGGER_INFO_LOG("Added ISA segment to DebugData. currentSize=%lu TotalSize=%lu handle=%lu vmID=%lu address=%lu size=%lu \n",
                                    isaDebugData.bindInfoMap[vmId].size(), isaDebugData.totalSegments, debugDataHandle.value(), vmId, bo->peekAddress(), bo->peekSize());
            UNRECOVERABLE_IF(isaDebugData.bindInfoMap[vmId].size() > isaDebugData.totalSegments);
        }

        if (isaDebugData.bindInfoMap[vmId].size() == isaDebugData.totalSegments) {
            for (auto bindInfo : isaDebugData.bindInfoMap[vmId]) {
                VmBindOpExtDebugData debugData = {};
                debugData.base.name = euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionDebugDataName);
                debugData.base.nextExtension = 0;
                debugData.base.pad = 0;
                debugData.addr = bindInfo.address;
                debugData.range = bindInfo.size;
                debugData.flags = 0;
                debugData.offset = 0u;
                memcpy(debugData.pathname, isaDebugData.elfPath, PATH_MAX);
                debugDataVec.push_back(debugData);
            }
        }

        if (!isAdd) {
            auto numErased = std::erase_if(isaDebugData.bindInfoMap[vmId], [=](IsaDebugData::DebugDataBindInfo bindInfo) {
                return (bindInfo.address == bo->peekAddress() && bindInfo.size == bo->peekSize());
            });
            UNRECOVERABLE_IF(numErased != 1);
            PRINT_DEBUGGER_INFO_LOG("Removed ISA segment from DebugData. currentSize=%lu TotalSize=%lu handle=%lu vmID=%lu address=%lu size=%lu \n",
                                    isaDebugData.bindInfoMap[vmId].size(), isaDebugData.totalSegments, debugDataHandle.value(), vmId, bo->peekAddress(), bo->peekSize());
        }
        if (debugDataVec.size() == 0) {
            return std::nullopt;
        }

    } else {
        return std::nullopt;
    }

    return debugDataVec;
}

int IoctlHelperXe::bindAddDebugData(std::vector<VmBindOpExtDebugData> debugDataVec, uint32_t vmId, VmBindExtUserFenceT *vmBindExtUserFence, bool isAdd) {

    drm_xe_vm_bind bind = {};
    bind.vm_id = vmId;
    bind.num_binds = static_cast<uint32_t>(debugDataVec.size());
    std::vector<drm_xe_vm_bind_op> bindOps;
    for (auto &debugData : debugDataVec) {
        drm_xe_vm_bind_op op = {};
        op.obj_offset = 0;
        op.range = 0;
        op.addr = 0;
        op.flags = 0;
        if (isAdd) {
            op.op = euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsAddDebugData);
        } else {
            op.op = euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsRemoveDebugData);
        }
        op.extensions = (uintptr_t)&debugData;
        if (debugData.flags == euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag)) {
            PRINT_DEBUGGER_INFO_LOG("drm_xe_vm_bind_op_ext_debug_data: add=%u addr=0x%llx range=0x%llx flags=%d offset=0x%llx pseudopath=%llu\n",
                                    isAdd, debugData.addr, debugData.range, debugData.flags, debugData.offset, debugData.pseudopath);
        } else {
            PRINT_DEBUGGER_INFO_LOG("drm_xe_vm_bind_op_ext_debug_data: add=%u addr=0x%llx range=0x%llx flags=%d offset=0x%llx pathname=%s",
                                    isAdd, debugData.addr, debugData.range, debugData.flags, debugData.offset, debugData.pathname);
        }

        bindOps.push_back(op);
    }
    if (bind.num_binds > 1) {
        bind.vector_of_binds = (uintptr_t)bindOps.data();
    } else {
        bind.bind = bindOps[0];
    }

    drm_xe_sync sync[1] = {};
    if (isAdd) {
        auto xeBindExtUserFence = reinterpret_cast<UserFenceExtension *>(vmBindExtUserFence);
        bind.num_syncs = 1u;
        sync[0].type = DRM_XE_SYNC_TYPE_USER_FENCE;
        sync[0].flags = DRM_XE_SYNC_FLAG_SIGNAL;
        sync[0].addr = xeBindExtUserFence->addr;
        sync[0].timeline_value = xeBindExtUserFence->value;
        bind.syncs = reinterpret_cast<uintptr_t>(&sync);
    } else {
        bind.num_syncs = 0u;
    }

    auto ret = IoctlHelper::ioctl(DrmIoctl::gemVmBind, &bind);
    const char *operation = isAdd ? "add debug data" : "remove debug data";
    XELOG("vm=%d obj=0x%x off=0x%llx range=0x%llx addr=0x%llx operation=%d(%s) flags=%d nsy=%d num_bind=%d pat=%hu ret=%d\n",
          bind.vm_id,
          bind.bind.obj,
          bind.bind.obj_offset,
          bind.bind.range,
          bind.bind.addr,
          bind.bind.op,
          operation,
          bind.bind.flags,
          bind.num_syncs,
          bind.num_binds,
          bind.bind.pat_index,
          ret);

    if (ret != 0) {
        XELOG("error: %s\n", operation);
        return ret;
    }

    if (isAdd) {
        return xeWaitUserFence(bind.exec_queue_id, DRM_XE_UFENCE_WAIT_OP_EQ,
                               sync[0].addr,
                               sync[0].timeline_value, -1,
                               false, NEO::InterruptId::notUsed, nullptr);

    } else {
        return 0;
    }
}

} // namespace NEO
