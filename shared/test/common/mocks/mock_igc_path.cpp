/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/mocks/mock_compilers.h"

#include <memory>

namespace Os {
extern const char *igcDllName;
} // namespace Os

namespace NEO {

StackVec<const char *, 32> prevIgcDllName;

bool popIgcDllName() {
    if (prevIgcDllName.empty()) {
        return false;
    }

    Os::igcDllName = *prevIgcDllName.rbegin();
    prevIgcDllName.pop_back();
    return true;
}

PopIgcDllNameGuard::~PopIgcDllNameGuard() {
    popIgcDllName();
}

[[nodiscard]] std::unique_ptr<PopIgcDllNameGuard> pushIgcDllName(const char *name) {
    prevIgcDllName.push_back(Os::igcDllName);
    Os::igcDllName = name;
    return std::make_unique<PopIgcDllNameGuard>();
}

} // namespace NEO
