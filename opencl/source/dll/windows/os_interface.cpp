/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_interface.h"

#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

bool OSInterface::osEnableLocalMemory = true;

void OSInterface::setGmmInputArgs(void *args) {
    this->get()->getWddm()->setGmmInputArg(args);
}

} // namespace NEO
