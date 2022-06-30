/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/os_context_linux.h"

class MockOsContextLinux : public NEO::OsContextLinux {
  public:
    using NEO::OsContextLinux::drmContextIds;
    using NEO::OsContextLinux::OsContextLinux;
};