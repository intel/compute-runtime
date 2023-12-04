/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

namespace NEO {

template <size_t staticSize>
struct UmKmDataTempStorageBase {
    UmKmDataTempStorageBase() = default;
    UmKmDataTempStorageBase(size_t dynSize) {
        this->resize(dynSize);
    }

    void *data() {
        return storage.data();
    }

    void resize(size_t dynSize) {
        auto oldSize = storage.size() * sizeof(uint64_t);
        storage.resize((dynSize + sizeof(uint64_t) - 1) / sizeof(uint64_t));
        requestedSize = dynSize;
        memset(reinterpret_cast<char *>(data()) + oldSize, 0, storage.size() * sizeof(uint64_t) - oldSize);
    }

    size_t size() const {
        return requestedSize;
    }

  protected:
    static constexpr size_t staticSizeQwordsCount = (staticSize + sizeof(uint64_t) - 1) / sizeof(uint64_t);
    StackVec<uint64_t, staticSizeQwordsCount> storage;
    size_t requestedSize = 0U;
};

template <typename SrcT, size_t overestimateMul = 2, typename BaseT = UmKmDataTempStorageBase<sizeof(SrcT) * overestimateMul>>
struct UmKmDataTempStorage : BaseT {
    using BaseT::BaseT;
};

} // namespace NEO
