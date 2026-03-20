/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/registry/built_ins_registry.h"

#include "shared/source/built_ins/built_ins.h"

namespace NEO {

RegisterEmbeddedResource::RegisterEmbeddedResource(const char *name, const char *resource, size_t resourceLength) {
    auto &storageRegistry = BuiltIn::EmbeddedStorageRegistry::getInstance();
    storageRegistry.store(name, BuiltIn::createResource(resource, resourceLength));
}

} // namespace NEO
