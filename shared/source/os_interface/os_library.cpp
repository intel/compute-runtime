/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
namespace NEO {

decltype(&OsLibrary::load) OsLibrary::loadFunc = OsLibrary::load;

} // namespace NEO
