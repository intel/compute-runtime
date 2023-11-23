/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"

#include "shared/source/helpers/constants.h"

namespace NEO {

bool OSInterface::osEnableLocalMemory = true;

namespace Directory {
bool returnEmptyFilesVector = false;
}

} // namespace NEO
