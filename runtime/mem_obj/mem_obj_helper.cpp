/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper.h"

namespace OCLRT {

bool MemObjHelper::checkExtraMemFlagsForBuffer(cl_mem_flags flags) {
    return false;
}

AllocationFlags MemObjHelper::getAllocationFlags(cl_mem_flags flags) {
    return AllocationFlags(); // Initialized by default constructor
}

DevicesBitfield MemObjHelper::getDevicesBitfield(cl_mem_flags flags) {
    return DevicesBitfield(0);
}

} // namespace OCLRT
