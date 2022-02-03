/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/app_resource_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/app_resource_defines.h"
#include "shared/source/helpers/string.h"

namespace NEO {

void AppResourceHelper::copyResourceTagStr(char *dst, GraphicsAllocation::AllocationType type, size_t size) {
    if (DebugManager.flags.EnableResourceTags.get()) {
        auto tag = getResourceTagStr(type);
        strcpy_s(dst, size, tag);
    }
}

const char *AppResourceHelper::getResourceTagStr(GraphicsAllocation::AllocationType type) {
    switch (type) {
    case GraphicsAllocation::AllocationType::UNKNOWN:
        return "UNKNOWN";
    case GraphicsAllocation::AllocationType::BUFFER:
        return "BUFFER";
    case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
        return "BFHSTMEM";
    case GraphicsAllocation::AllocationType::COMMAND_BUFFER:
        return "CMNDBUFF";
    case GraphicsAllocation::AllocationType::CONSTANT_SURFACE:
        return "CSNTSRFC";
    case GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR:
        return "EXHSTPTR";
    case GraphicsAllocation::AllocationType::FILL_PATTERN:
        return "FILPATRN";
    case GraphicsAllocation::AllocationType::GLOBAL_SURFACE:
        return "GLBLSRFC";
    case GraphicsAllocation::AllocationType::IMAGE:
        return "IMAGE";
    case GraphicsAllocation::AllocationType::INDIRECT_OBJECT_HEAP:
        return "INOBHEAP";
    case GraphicsAllocation::AllocationType::INSTRUCTION_HEAP:
        return "INSTHEAP";
    case GraphicsAllocation::AllocationType::INTERNAL_HEAP:
        return "INTLHEAP";
    case GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY:
        return "INHSTMEM";
    case GraphicsAllocation::AllocationType::KERNEL_ISA:
        return "KERNLISA";
    case GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL:
        return "KRLISAIN";
    case GraphicsAllocation::AllocationType::LINEAR_STREAM:
        return "LINRSTRM";
    case GraphicsAllocation::AllocationType::MAP_ALLOCATION:
        return "MAPALLOC";
    case GraphicsAllocation::AllocationType::MCS:
        return "MCS";
    case GraphicsAllocation::AllocationType::PIPE:
        return "PIPE";
    case GraphicsAllocation::AllocationType::PREEMPTION:
        return "PRMPTION";
    case GraphicsAllocation::AllocationType::PRINTF_SURFACE:
        return "PRNTSRFC";
    case GraphicsAllocation::AllocationType::PRIVATE_SURFACE:
        return "PRVTSRFC";
    case GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER:
        return "PROFTGBF";
    case GraphicsAllocation::AllocationType::SCRATCH_SURFACE:
        return "SCRHSRFC";
    case GraphicsAllocation::AllocationType::SHARED_BUFFER:
        return "SHRDBUFF";
    case GraphicsAllocation::AllocationType::SHARED_CONTEXT_IMAGE:
        return "SRDCXIMG";
    case GraphicsAllocation::AllocationType::SHARED_IMAGE:
        return "SHERDIMG";
    case GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY:
        return "SRDRSCCP";
    case GraphicsAllocation::AllocationType::SURFACE_STATE_HEAP:
        return "SRFCSTHP";
    case GraphicsAllocation::AllocationType::SVM_CPU:
        return "SVM_CPU";
    case GraphicsAllocation::AllocationType::SVM_GPU:
        return "SVM_GPU";
    case GraphicsAllocation::AllocationType::SVM_ZERO_COPY:
        return "SVM0COPY";
    case GraphicsAllocation::AllocationType::TAG_BUFFER:
        return "TAGBUFER";
    case GraphicsAllocation::AllocationType::GLOBAL_FENCE:
        return "GLBLFENC";
    case GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
        return "TSPKTGBF";
    case GraphicsAllocation::AllocationType::WRITE_COMBINED:
        return "WRTCMBND";
    case GraphicsAllocation::AllocationType::RING_BUFFER:
        return "RINGBUFF";
    case GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER:
        return "SMPHRBUF";
    case GraphicsAllocation::AllocationType::DEBUG_CONTEXT_SAVE_AREA:
        return "DBCXSVAR";
    case GraphicsAllocation::AllocationType::DEBUG_SBA_TRACKING_BUFFER:
        return "DBSBATRB";
    case GraphicsAllocation::AllocationType::DEBUG_MODULE_AREA:
        return "DBMDLARE";
    case GraphicsAllocation::AllocationType::UNIFIED_SHARED_MEMORY:
        return "USHRDMEM";
    case GraphicsAllocation::AllocationType::WORK_PARTITION_SURFACE:
        return "WRPRTSRF";
    case GraphicsAllocation::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
        return "GPUTSDBF";
    case GraphicsAllocation::AllocationType::SW_TAG_BUFFER:
        return "SWTAGBF";
    default:
        return "NOTFOUND";
    }
}

} // namespace NEO
