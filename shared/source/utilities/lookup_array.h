/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"

#include <array>
#include <utility>

template <typename KeyT, typename ValueT, size_t NumElements>
struct LookupArray {
    using LookupMapArrayT = std::array<std::pair<KeyT, ValueT>, NumElements>;
    constexpr LookupArray(const LookupMapArrayT &lookupArray) : lookupArray(lookupArray){};

    constexpr ValueT lookUp(const KeyT &keyToFind) const {
        for (auto &[key, value] : lookupArray) {
            if (keyToFind == key) {
                return value;
            }
        }
        UNRECOVERABLE_IF(true);
    }

  protected:
    LookupMapArrayT lookupArray;
};
