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
    return getGdi()->shareObjects(1, resourceHandle, nullptr, SHARED_ALLOCATION_WRITE, ntHandle);
}
} // namespace NEO