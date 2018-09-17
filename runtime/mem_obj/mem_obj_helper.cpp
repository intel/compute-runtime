/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper.h"

namespace OCLRT {

bool MemObjHelper::checkExtraMemFlagsForBuffer(cl_mem_flags flags) {
    return true;
}

AllocationFlags MemObjHelper::getAllocationFlags(cl_mem_flags flags) {
    return AllocationFlags(); // Initialized by default constructor
}

DeviceIndex MemObjHelper::getDeviceIndex(cl_mem_flags flags) {
    return DeviceIndex(0);
}

} // namespace OCLRT
