/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <unordered_set>

namespace NEO {

struct KernelObjForAuxTranslation {
    enum class Type {
        MEM_OBJ,
        GFX_ALLOC
    };

    KernelObjForAuxTranslation(Type type, void *object) : type(type), object(object) {}

    Type type;
    void *object;

    bool operator==(const KernelObjForAuxTranslation &t) const {
        return (this->object == t.object);
    }
};

struct KernelObjForAuxTranslationHash {
    std::size_t operator()(const KernelObjForAuxTranslation &kernelObj) const {
        return reinterpret_cast<size_t>(kernelObj.object);
    }
};

using KernelObjsForAuxTranslation = std::unordered_set<KernelObjForAuxTranslation, KernelObjForAuxTranslationHash>;

} // namespace NEO
