/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
namespace NEO {

struct RegisterRead {
    uint64_t offset;
    uint64_t value;
};

struct ExecObject {
    uint8_t data[56];
};

struct ExecBuffer {
    uint8_t data[64];
};

struct GemCreate {
    uint64_t size;
    uint32_t handle;
};

struct GemUserPtr {
    uint64_t userPtr;
    uint64_t userSize;
    uint32_t flags;
    uint32_t handle;
};

struct GemSetTiling {
    uint32_t handle;
    uint32_t tilingMode;
    uint32_t stride;
    uint32_t swizzleMode;
};

struct GemGetTiling {
    bool isTilingDisabled() const;

    uint32_t handle;
    uint32_t tilingMode;
    uint32_t swizzleMode;
    uint32_t physSwizzleMode;
};

struct QueryItem {
    uint64_t queryId;
    int32_t length;
    uint32_t flags;
    uint64_t dataPtr;
};

struct EngineClassInstance {
    uint16_t engineClass;
    uint16_t engineInstance;
};

struct GemContextParamSseu {
    EngineClassInstance engine;
    uint32_t flags;
    uint64_t sliceMask;
    uint64_t subsliceMask;
    uint16_t minEusPerSubslice;
    uint16_t maxEusPerSubslice;
};

struct QueryTopologyInfo {
    uint16_t flags;
    uint16_t maxSlices;
    uint16_t maxSubslices;
    uint16_t maxEusPerSubslice;
    uint16_t subsliceOffset;
    uint16_t subsliceStride;
    uint16_t euOffset;
    uint16_t euStride;
    uint8_t data[];
};

struct MemoryClassInstance {
    uint16_t memoryClass;
    uint16_t memoryInstance;
};

struct GemMmapOffset {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
    uint64_t flags;
    uint64_t extensions;
};

struct GemMmap {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
    uint64_t size;
    uint64_t addrPtr;
    uint64_t flags;
};

struct GemSetDomain {
    uint32_t handle;
    uint32_t readDomains;
    uint32_t writeDomain;
};

struct GemContextParam {
    uint32_t contextId;
    uint32_t size;
    uint64_t param;
    uint64_t value;
};

struct DrmUserExtension {
    uint64_t nextExtension;
    uint32_t name;
    uint32_t flags;
    uint32_t reserved[4];
};

struct GemContextCreateExtSetParam {
    DrmUserExtension base;
    GemContextParam param;
};

struct GemContextCreateExt {
    uint32_t contextId;
    uint32_t flags;
    uint64_t extensions;
};

} // namespace NEO
