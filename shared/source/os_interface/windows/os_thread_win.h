/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_thread.h"

#include <thread>

namespace NEO {
class ThreadWin : public Thread {
  public:
    ThreadWin(std::thread *thread);
    void join() override;
    void detach() override;
    void yield() override;
    ~ThreadWin() override = default;

  protected:
    std::unique_ptr<std::thread> thread;
};
} // namespace NEO
