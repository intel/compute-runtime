/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"

namespace NEO {
uint64_t IoctlHelperXe::getPrimaryContextProperties() const {
    return DRM_XE_MULTI_GROUP_CREATE;
}
uint32_t IoctlHelperXe::getExecQueueSetPropertyValue() const {
    return DRM_XE_EXEC_QUEUE_SET_PROPERTY;
}
uint32_t IoctlHelperXe::getExecQueueSetPropertyMultiGroupValue() const {
    return DRM_XE_EXEC_QUEUE_SET_PROPERTY_MULTI_GROUP;
}
uint32_t IoctlHelperXe::getExecQueueSetPropertyMultiQueuePriorityValue() const {
    return DRM_XE_EXEC_QUEUE_SET_PROPERTY_MULTI_QUEUE_PRIORITY;
}
} // namespace NEO
