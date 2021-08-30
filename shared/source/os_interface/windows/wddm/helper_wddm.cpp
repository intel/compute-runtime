/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

NTSTATUS Wddm::createNTHandle(const D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle) {
    OBJECT_ATTRIBUTES objAttr = {};
    objAttr.Length = sizeof(OBJECT_ATTRIBUTES);

    return getGdi()->shareObjects(1, resourceHandle, &objAttr, SHARED_ALLOCATION_WRITE, ntHandle);
}
} // namespace NEO
