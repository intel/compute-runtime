/*
 * Copyright (C) 2021-2025 Intel Corporation
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
    if (debugManager.flags.EnableResourceTags.get()) {
        auto tag = getResourceTagStr(type);
        strcpy_s(dst, size, tag);
    }
}

const char *AppResourceHelper::getResourceTagStr(AllocationType type) {
    switch (type) {
    case AllocationType::unknown:
        return "UNKNOWN";
    case AllocationType::buffer:
        return "BUFFER";
    case AllocationType::bufferHostMemory:
        return "BFHSTMEM";
    case AllocationType::commandBuffer:
        return "CMNDBUFF";
    case AllocationType::constantSurface:
        return "CSNTSRFC";
    case AllocationType::externalHostPtr:
        return "EXHSTPTR";
    case AllocationType::fillPattern:
        return "FILPATRN";
    case AllocationType::globalSurface:
        return "GLBLSRFC";
    case AllocationType::image:
        return "IMAGE";
    case AllocationType::indirectObjectHeap:
        return "INOBHEAP";
    case AllocationType::instructionHeap:
        return "INSTHEAP";
    case AllocationType::internalHeap:
        return "INTLHEAP";
    case AllocationType::internalHostMemory:
        return "INHSTMEM";
    case AllocationType::kernelArgsBuffer:
        return "KARGBUF";
    case AllocationType::kernelIsa:
        return "KERNLISA";
    case AllocationType::kernelIsaInternal:
        return "KRLISAIN";
    case AllocationType::linearStream:
        return "LINRSTRM";
    case AllocationType::mapAllocation:
        return "MAPALLOC";
    case AllocationType::mcs:
        return "MCS";
    case AllocationType::preemption:
        return "PRMPTION";
    case AllocationType::printfSurface:
        return "PRNTSRFC";
    case AllocationType::privateSurface:
        return "PRVTSRFC";
    case AllocationType::profilingTagBuffer:
        return "PROFTGBF";
    case AllocationType::scratchSurface:
        return "SCRHSRFC";
    case AllocationType::sharedBuffer:
        return "SHRDBUFF";
    case AllocationType::sharedImage:
        return "SHERDIMG";
    case AllocationType::sharedResourceCopy:
        return "SRDRSCCP";
    case AllocationType::surfaceStateHeap:
        return "SRFCSTHP";
    case AllocationType::svmCpu:
        return "SVM_CPU";
    case AllocationType::svmGpu:
        return "SVM_GPU";
    case AllocationType::svmZeroCopy:
        return "SVM0COPY";
    case AllocationType::syncBuffer:
        return "SYNCBUFF";
    case AllocationType::tagBuffer:
        return "TAGBUFER";
    case AllocationType::globalFence:
        return "GLBLFENC";
    case AllocationType::timestampPacketTagBuffer:
        return "TSPKTGBF";
    case AllocationType::writeCombined:
        return "WRTCMBND";
    case AllocationType::ringBuffer:
        return "RINGBUFF";
    case AllocationType::semaphoreBuffer:
        return "SMPHRBUF";
    case AllocationType::debugContextSaveArea:
        return "DBCXSVAR";
    case AllocationType::debugSbaTrackingBuffer:
        return "DBSBATRB";
    case AllocationType::debugModuleArea:
        return "DBMDLARE";
    case AllocationType::unifiedSharedMemory:
        return "USHRDMEM";
    case AllocationType::workPartitionSurface:
        return "WRPRTSRF";
    case AllocationType::gpuTimestampDeviceBuffer:
        return "GPUTSDBF";
    case AllocationType::swTagBuffer:
        return "SWTAGBF";
    case AllocationType::deferredTasksList:
        return "TSKLIST";
    case AllocationType::assertBuffer:
        return "ASSRTBUF";
    case AllocationType::syncDispatchToken:
        return "SYNCTOK";
    case AllocationType::hostFunction:
        return "HOSTFUNC";
    default:
        return "NOTFOUND";
    }
}

} // namespace NEO
