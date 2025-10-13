/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_thread_win.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace NEO {
namespace SysCalls {
extern bool isShutdownInProgressRetVal;
}
} // namespace NEO

TEST(OsThreadWinTest, givenNoShutdownInProgressThenCreateThreadSuccessfully) {
    auto thread = Thread::createFunc([](void *) -> void * { return nullptr; }, nullptr);
    EXPECT_NE(nullptr, thread);
    thread->join();
}

TEST(OsThreadWinTest, givenShutdownInProgressThenDontCreateThread) {
    VariableBackup<bool> isShutdownInProgressRetVal{&SysCalls::isShutdownInProgressRetVal};
    SysCalls::isShutdownInProgressRetVal = true;

    auto thread = Thread::createFunc([](void *) -> void * { return nullptr; }, nullptr);
    EXPECT_EQ(nullptr, thread);
}
