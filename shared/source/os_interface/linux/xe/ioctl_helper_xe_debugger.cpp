/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "uapi-eudebug/drm/xe_drm.h"
#include "uapi-eudebug/drm/xe_drm_tmp.h"

#define STRINGIFY_ME(X) return #X
#define RETURN_ME(X) return X

namespace NEO {

unsigned int IoctlHelperXe::getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::debuggerOpen:
        RETURN_ME(DRM_IOCTL_XE_EUDEBUG_CONNECT);

    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

void *IoctlHelperXe::allocateDebugMetadata() {
    drm_xe_ext_vm_set_debug_metadata *prev = nullptr;
    drm_xe_ext_vm_set_debug_metadata *xeMetadataRoot = nullptr;

    for (auto &metadata : debugMetadata) {
        auto *xeMetadata = new drm_xe_ext_vm_set_debug_metadata();
        if (!xeMetadataRoot) {
            xeMetadataRoot = xeMetadata;
        }
        xeMetadata->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
        xeMetadata->offset = metadata.offset;
        xeMetadata->len = metadata.size;
        if (metadata.isCookie) {
            xeMetadata->type = DRM_XE_VM_DEBUG_METADATA_COOKIE;
        } else {

            switch (metadata.type) {
            case DrmResourceClass::contextSaveArea:
                xeMetadata->type = DRM_XE_VM_DEBUG_METADATA_SIP_AREA;
                break;
            case DrmResourceClass::sbaTrackingBuffer:
                xeMetadata->type = DRM_XE_VM_DEBUG_METADATA_SBA_AREA;
                break;
            case DrmResourceClass::moduleHeapDebugArea:
                xeMetadata->type = DRM_XE_VM_DEBUG_METADATA_MODULE_AREA;
                break;
            default:
                UNRECOVERABLE_IF(true);
            }
        }
        PRINT_DEBUGGER_INFO_LOG("drm_xe_ext_vm_set_debug_metadata: type = %ul, offset = %ul, len = %ul\n", xeMetadata->type, xeMetadata->offset, xeMetadata->len);
        if (prev) {
            prev->base.next_extension = reinterpret_cast<unsigned long long>(xeMetadata);
        }
        prev = xeMetadata;
    }
    return xeMetadataRoot;
}

void *IoctlHelperXe::freeDebugMetadata(void *metadata) {
    xe_user_extension *ext = static_cast<xe_user_extension *>(metadata);
    xe_user_extension *prev = nullptr;
    xe_user_extension *newRoot = nullptr;
    while (ext) {
        xe_user_extension *temp = reinterpret_cast<xe_user_extension *>(ext->next_extension);
        if (ext->name == DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA) {
            if (prev) {
                prev->next_extension = ext->next_extension;
            }
            delete ext;
        } else {
            if (!newRoot) {
                newRoot = ext;
            }
            prev = ext;
        }
        ext = temp;
    }
    return newRoot;
}

void IoctlHelperXe::addDebugMetadataCookie(uint64_t cookie) {

    DebugMetadata metadata = {DrmResourceClass::cookie, cookie, 0, true};
    debugMetadata.push_back(metadata);
    return;
}

void IoctlHelperXe::addDebugMetadata(DrmResourceClass type, uint64_t *offset, uint64_t size) {

    if (type != DrmResourceClass::moduleHeapDebugArea && type != DrmResourceClass::contextSaveArea && type != DrmResourceClass::sbaTrackingBuffer) {
        return;
    }
    DebugMetadata metadata = {type, reinterpret_cast<unsigned long long>(offset), size, false};
    debugMetadata.push_back(metadata);
    return;
}

int IoctlHelperXe::getRunaloneExtProperty() {
    return DRM_XE_EXEC_QUEUE_SET_PROPERTY_RUNALONE;
}

} // namespace NEO