/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_thread.h"

namespace NEO {

class MockThread : public Thread {
  public:
    void join() override {}
    void detach() override {}
    void yield() override {}
};

} // namespace NEO
