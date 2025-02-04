/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
namespace NEO {
struct EuDebugConnect {
    uint64_t extensions;
    uint64_t pid;
    uint32_t flags;
    uint32_t version;
};

struct EuDebugEvent {
    uint32_t len;
    uint16_t type;
    uint16_t flags;
    uint64_t seqno;
    uint64_t reserved;
};

struct EuDebugEventEuAttention {
    struct EuDebugEvent base;
    uint64_t clientHandle;
    uint64_t execQueueHandle;
    uint64_t lrcHandle;
    uint32_t flags;
    uint32_t bitmaskSize;
    uint8_t bitmask[];
};

struct EuDebugEventClient {
    struct EuDebugEvent base;
    uint64_t clientHandle;
};

struct EuDebugEventVm {
    struct EuDebugEvent base;
    uint64_t clientHandle;
    uint64_t vmHandle;
};

struct EuDebugEventExecQueue {
    struct EuDebugEvent base;
    uint64_t clientHandle;
    uint64_t vmHandle;
    uint64_t execQueueHandle;
    uint32_t engineClass;
    uint32_t width;
    uint64_t lrcHandle[];
};

struct EuDebugEventExecQueuePlacements {
    struct EuDebugEvent base;
    uint64_t clientHandle;
    uint64_t vmHandle;
    uint64_t execQueueHandle;
    uint64_t lrcHandle;
    uint32_t numPlacements;
    uint32_t pad;
    uint64_t instances[];
};

struct EuDebugEventMetadata {
    struct EuDebugEvent base;
    uint64_t clientHandle;
    uint64_t metadataHandle;
    uint64_t type;
    uint64_t len;
};

struct EuDebugEventVmBind {
    struct EuDebugEvent base;
    uint64_t clientHandle;
    uint64_t vmHandle;
    uint32_t flags;
    uint32_t numBinds;
};

struct EuDebugEventVmBindOp {
    struct EuDebugEvent base;
    uint64_t vmBindRefSeqno;
    uint64_t numExtensions;
    uint64_t addr;
    uint64_t range;
};

struct EuDebugEventVmBindOpMetadata {
    struct EuDebugEvent base;
    uint64_t vmBindOpRefSeqno;
    uint64_t metadataHandle;
    uint64_t metadataCookie;
};

struct EuDebugEventVmBindUfence {
    struct EuDebugEvent base;
    uint64_t vmBindRefSeqno;
};

struct EuDebugEventPageFault {
    struct EuDebugEvent base;
    uint64_t clientHandle;
    uint64_t execQueueHandle;
    uint64_t lrcHandle;
    uint32_t flags;
    uint32_t bitmaskSize;
    uint64_t pagefaultAddress;
    uint8_t bitmask[];
};

struct EuDebugReadMetadata {
    uint64_t clientHandle;
    uint64_t metadataHandle;
    uint32_t flags;
    uint32_t reserved;
    uint64_t ptr;
    uint64_t size;
};

struct EuDebugEuControl {
    uint64_t clientHandle;
    uint32_t cmd;
    uint32_t flags;
    uint64_t seqno;
    uint64_t execQueueHandle;
    uint64_t lrcHandle;
    uint32_t reserved;
    uint32_t bitmaskSize;
    uint64_t bitmaskPtr;
};

struct EuDebugVmOpen {
    uint64_t extensions;
    uint64_t clientHandle;
    uint64_t vmHandle;
    uint64_t flags;
    uint64_t timeoutNs;
};

struct EuDebugAckEvent {
    uint32_t type;
    uint32_t flags;
    uint64_t seqno;
};

struct DebugMetadataCreate {
    uint64_t extensions;
    uint64_t type;
    uint64_t userAddr;
    uint64_t len;
    uint32_t metadataId;
};

struct DebugMetadataDestroy {
    uint64_t extensions;
    uint32_t metadataId;
};

struct XeEngineClassInstance {
    uint16_t engineClass;
    uint16_t engineInstance;
    uint16_t gtId;
    uint16_t pad;
};

struct XeUserExtension {
    uint64_t nextExtension;
    uint32_t name;
    uint32_t pad;
};

struct VmBindOpExtAttachDebug {
    struct XeUserExtension base;
    uint64_t metadataId;
    uint64_t flags;
    uint64_t cookie;
    uint64_t reserved;
};

enum class EuDebugParam {
    connect,
    euControlCmdInterruptAll,
    euControlCmdResume,
    euControlCmdStopped,
    eventBitCreate,
    eventBitDestroy,
    eventBitNeedAck,
    eventBitStateChange,
    eventTypeEuAttention = 20,
    eventTypeExecQueue,
    eventTypeExecQueuePlacements,
    eventTypeMetadata,
    eventTypeOpen,
    eventTypePagefault,
    eventTypeRead,
    eventTypeVm,
    eventTypeVmBind,
    eventTypeVmBindOp,
    eventTypeVmBindOpMetadata,
    eventTypeVmBindUfence,
    eventVmBindFlagUfence,
    execQueueSetPropertyEuDebug,
    execQueueSetPropertyValueEnable,
    execQueueSetPropertyValuePageFaultEnable,
    ioctlAckEvent,
    ioctlEuControl,
    ioctlReadEvent,
    ioctlReadMetadata,
    ioctlVmOpen,
    metadataCreate,
    metadataDestroy,
    metadataElfBinary,
    metadataModuleArea,
    metadataProgramModule,
    metadataSbaArea,
    metadataSipArea,
    vmBindOpExtensionsAttachDebug,
};
} // namespace NEO
