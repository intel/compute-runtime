/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifndef _XE_DRM_H_
#include <linux/types.h>
#include <sys/ioctl.h>
namespace NEO {
namespace XeDrm {
#include "xe_drm.h"

struct prelim_drm_xe_exec_queue_ban_fault_ext { // NOLINT(readability-identifier-naming)
    struct drm_xe_user_extension base;
    __u64 addr;
    __u16 type;
    __u16 level;
    __u16 access;
    __u16 flags;
};

#define PRELIM_DRM_XE_EXEC_QUEUE_BAN_FAULT_VALID (1 << 0)
#define PRELIM_DRM_XE_EXEC_QUEUE_BAN_STATUS_BANNED (1 << 0)

// VM Get Property ioctl - upstream API for querying VM faults
#define DRM_XE_VM_GET_PROPERTY 0x0f

struct xe_vm_fault { // NOLINT(readability-identifier-naming)
    __u64 address;
    __u32 address_precision; // NOLINT(readability-identifier-naming)
#define FAULT_ACCESS_TYPE_READ 0
#define FAULT_ACCESS_TYPE_WRITE 1
#define FAULT_ACCESS_TYPE_ATOMIC 2
    __u8 access_type; // NOLINT(readability-identifier-naming)
#define FAULT_TYPE_NOT_PRESENT 0
#define FAULT_TYPE_WRITE_ACCESS 1
#define FAULT_TYPE_ATOMIC_ACCESS 2
    __u8 fault_type; // NOLINT(readability-identifier-naming)
#define FAULT_LEVEL_PTE 0
#define FAULT_LEVEL_PDE 1
#define FAULT_LEVEL_PDP 2
#define FAULT_LEVEL_PML4 3
#define FAULT_LEVEL_PML5 4
    __u8 fault_level; // NOLINT(readability-identifier-naming)
    __u8 pad;
    __u64 reserved[4];
};

struct drm_xe_vm_get_property { // NOLINT(readability-identifier-naming)
    __u64 extensions;
    __u32 vm_id; // NOLINT(readability-identifier-naming)
#define DRM_XE_VM_GET_PROPERTY_FAULTS 0
    __u32 property;
    __u32 size;
    __u32 pad;
    union {
        __u64 data;
        __u64 value;
    };
    __u64 reserved[3];
};

#define DRM_IOCTL_XE_VM_GET_PROPERTY DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_VM_GET_PROPERTY, struct drm_xe_vm_get_property)

} // namespace XeDrm
} // namespace NEO
using namespace NEO::XeDrm;

#endif
