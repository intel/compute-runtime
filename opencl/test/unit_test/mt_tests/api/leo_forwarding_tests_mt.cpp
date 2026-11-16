/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/api/leo_forwarding.h"

#include <atomic>
#include <memory>
#include <thread>

namespace NEO {

TEST(LeoForwardingMtTests, givenLibraryLoadInProgressWhenEnsuringLibraryIsLoadedOnAnotherThreadThenItWaitsForTheLoadAndSeesLoadedLibrary) {
    L0ForwardingState state;
    VariableBackup<L0ForwardingState *> stateBackup{&l0ForwardingState, &state};
    std::atomic<bool> threadStarted{false};
    std::atomic<bool> callCompleted{false};
    std::atomic<bool> libraryLoaded{false};

    std::unique_lock<std::mutex> lock(state.mutex);
    state.loaded = true;

    std::thread worker([&]() {
        threadStarted.store(true);
        libraryLoaded.store(ensureLeoLibraryLoaded());
        callCompleted.store(true);
    });

    while (false == threadStarted.load()) {
        std::this_thread::yield();
    }
    EXPECT_FALSE(callCompleted.load());

    state.library = std::make_unique<MockOsLibraryCustom>(nullptr, true);
    lock.unlock();
    worker.join();

    EXPECT_TRUE(callCompleted.load());
    EXPECT_TRUE(libraryLoaded.load());
}

} // namespace NEO
