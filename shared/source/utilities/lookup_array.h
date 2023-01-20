/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"

#include <array>
#include <optional>
#include <utility>

template <typename KeyT, typename ValueT, size_t NumElements>
struct LookupArray {
    using LookupMapArrayT = std::array<std::pair<KeyT, ValueT>, NumElements>;
    constexpr LookupArray(const LookupMapArrayT &lookupArray) : lookupArray(lookupArray){};

    constexpr std::optional<ValueT> find(const KeyT &keyToFind) const {
        for (auto &[key, value] : lookupArray) {
            if (keyToFind == key) {
                return value;
            }
        }
        return std::nullopt;
    }

    constexpr ValueT lookUp(const KeyT &keyToFind) const {
        auto value = find(keyToFind);
        UNRECOVERABLE_IF(false == value.has_value());
        return *value;
    }

    constexpr size_t size() const {
        return NumElements;
    }

  protected:
    LookupMapArrayT lookupArray;
};
