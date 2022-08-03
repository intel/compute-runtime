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

void AppResourceHelper::copyResourceTagStr(char *dst, AllocationType type, size_t size) {
    if (DebugManager.flags.EnableResourceTags.get()) {
        auto tag = getResourceTagStr(type);
        strcpy_s(dst, size, tag);
    }
}

const char *AppResourceHelper::getResourceTagStr(AllocationType type) {
    switch (type) {
    case AllocationType::UNKNOWN:
        return "UNKNOWN";
    case AllocationType::BUFFER:
        return "BUFFER";
    case AllocationType::BUFFER_HOST_MEMORY:
        return "BFHSTMEM";
    case AllocationType::COMMAND_BUFFER:
        return "CMNDBUFF";
    case AllocationType::CONSTANT_SURFACE:
        return "CSNTSRFC";
    case AllocationType::EXTERNAL_HOST_PTR:
        return "EXHSTPTR";
    case AllocationType::FILL_PATTERN:
        return "FILPATRN";
    case AllocationType::GLOBAL_SURFACE:
        return "GLBLSRFC";
    case AllocationType::IMAGE:
        return "IMAGE";
    case AllocationType::INDIRECT_OBJECT_HEAP:
        return "INOBHEAP";
    case AllocationType::INSTRUCTION_HEAP:
        return "INSTHEAP";
    case AllocationType::INTERNAL_HEAP:
        return "INTLHEAP";
    case AllocationType::INTERNAL_HOST_MEMORY:
        return "INHSTMEM";
    case AllocationType::KERNEL_ARGS_BUFFER:
        return "KARGBUF";
    case AllocationType::KERNEL_ISA:
        return "KERNLISA";
    case AllocationType::KERNEL_ISA_INTERNAL:
        return "KRLISAIN";
    case AllocationType::LINEAR_STREAM:
        return "LINRSTRM";
    case AllocationType::MAP_ALLOCATION:
        return "MAPALLOC";
    case AllocationType::MCS:
        return "MCS";
    case AllocationType::PIPE:
        return "PIPE";
    case AllocationType::PREEMPTION:
        return "PRMPTION";
    case AllocationType::PRINTF_SURFACE:
        return "PRNTSRFC";
    case AllocationType::PRIVATE_SURFACE:
        return "PRVTSRFC";
    case AllocationType::PROFILING_TAG_BUFFER:
        return "PROFTGBF";
    case AllocationType::SCRATCH_SURFACE:
        return "SCRHSRFC";
    case AllocationType::SHARED_BUFFER:
        return "SHRDBUFF";
    case AllocationType::SHARED_CONTEXT_IMAGE:
        return "SRDCXIMG";
    case AllocationType::SHARED_IMAGE:
        return "SHERDIMG";
    case AllocationType::SHARED_RESOURCE_COPY:
        return "SRDRSCCP";
    case AllocationType::SURFACE_STATE_HEAP:
        return "SRFCSTHP";
    case AllocationType::SVM_CPU:
        return "SVM_CPU";
    case AllocationType::SVM_GPU:
        return "SVM_GPU";
    case AllocationType::SVM_ZERO_COPY:
        return "SVM0COPY";
    case AllocationType::TAG_BUFFER:
        return "TAGBUFER";
    case AllocationType::GLOBAL_FENCE:
        return "GLBLFENC";
    case AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
        return "TSPKTGBF";
    case AllocationType::WRITE_COMBINED:
        return "WRTCMBND";
    case AllocationType::RING_BUFFER:
        return "RINGBUFF";
    case AllocationType::SEMAPHORE_BUFFER:
        return "SMPHRBUF";
    case AllocationType::DEBUG_CONTEXT_SAVE_AREA:
        return "DBCXSVAR";
    case AllocationType::DEBUG_SBA_TRACKING_BUFFER:
        return "DBSBATRB";
    case AllocationType::DEBUG_MODULE_AREA:
        return "DBMDLARE";
    case AllocationType::UNIFIED_SHARED_MEMORY:
        return "USHRDMEM";
    case AllocationType::WORK_PARTITION_SURFACE:
        return "WRPRTSRF";
    case AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
        return "GPUTSDBF";
    case AllocationType::SW_TAG_BUFFER:
        return "SWTAGBF";
    default:
        return "NOTFOUND";
    }
}

} // namespace NEO
