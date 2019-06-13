/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_interface.h"

#include "runtime/memory_manager/memory_constants.h"

namespace NEO {

bool OSInterface::osEnableLocalMemory = true && is64bit;

} // namespace NEO
