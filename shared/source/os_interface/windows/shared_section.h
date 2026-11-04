/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <cstddef>
#include <cstdint>

namespace NEO {

class SharedSection : NonCopyableAndNonMovableClass {
  public:
#pragma pack(push, 1)
    struct SharedMemoryContentTail {
        uint32_t ownerProcessId;
        GMM_RESCREATE_PARAMS gmmParams;
        char16_t sharedResourceName[33];
        uint32_t access;
        uint32_t serializedSecurityDescriptorStringSize;
    };
#pragma pack(pop)

    explicit SharedSection(void *view) : view(view) {}

    static bool isSection(HANDLE handle);

    bool initialize();

    void *getView() const { return view; }
    size_t getUsableSize() const { return usableSize; }
    const GMM_RESCREATE_PARAMS &getGmmParams() const { return gmmParams; }

  protected:
    void *view = nullptr;
    size_t usableSize = 0u;
    GMM_RESCREATE_PARAMS gmmParams = {};
};

static_assert(NonCopyableAndNonMovable<SharedSection>);

} // namespace NEO
