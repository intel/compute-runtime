/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/registry/built_ins_registry.h"

namespace NEO {

namespace {
constinit RegisterEmbeddedResource *firstEmbeddedResource = nullptr;
constinit RegisterEmbeddedResource *lastEmbeddedResource = nullptr;
} // namespace

RegisterEmbeddedResource::RegisterEmbeddedResource(const char *name, const char *resource, size_t resourceLength)
    : name(name), resource(resource), resourceLength(resourceLength) {
    if (lastEmbeddedResource == nullptr) {
        firstEmbeddedResource = this;
    } else {
        lastEmbeddedResource->next = this;
    }
    lastEmbeddedResource = this;
}

const RegisterEmbeddedResource *RegisterEmbeddedResource::find(std::string_view name) {
    for (auto *embeddedResource = firstEmbeddedResource; embeddedResource != nullptr; embeddedResource = embeddedResource->next) {
        if (embeddedResource->name == name) {
            return embeddedResource;
        }
    }
    return nullptr;
}

} // namespace NEO
