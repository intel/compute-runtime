/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {

template <typename CommandListDescT>
void cloneDescriptorHandler(const ze_base_desc_t *inputDesc, void *&outPnext, ze_base_desc_t *&chainedDesc) {
    auto inputTypeDesc = reinterpret_cast<const CommandListDescT *>(inputDesc);
    auto clonedTypeDesc = new CommandListDescT;
    *clonedTypeDesc = *inputTypeDesc;
    clonedTypeDesc->pNext = nullptr;
    if (outPnext == nullptr) {
        outPnext = clonedTypeDesc;
        chainedDesc = reinterpret_cast<ze_base_desc_t *>(clonedTypeDesc);
    } else {
        chainedDesc->pNext = reinterpret_cast<ze_base_desc_t *>(clonedTypeDesc);
    }
}

inline void chainNextDescriptor(const ze_base_desc_t *&desc, ze_base_desc_t *&chainedDesc) {
    desc = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);
    if (chainedDesc->pNext != nullptr) {
        chainedDesc = reinterpret_cast<ze_base_desc_t *>(const_cast<void *>(chainedDesc->pNext));
    }
}

} // namespace L0
