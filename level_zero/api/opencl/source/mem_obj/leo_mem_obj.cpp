/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/helpers/leo_get_info_status_mapper.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"

#include <ranges>

namespace NEO {
namespace LEO {

MemObj::MemObj(Context *context, MemoryProperties &properties, cl_mem_flags flags, void *cpuPtr, bool externalHandle, MemObj::MemObjType memObjType) : properties(properties), flags(flags), context(context), cpuPtr(cpuPtr), externalHandle(externalHandle), memObjType(memObjType) {
    context->incRefInternal();
}

MemObj::~MemObj() {
    if (cpuPtr && !properties.flags.useHostPtr && !this->isSubBuffer()) {
        zeMemFree(context->getL0ContextHandle(), cpuPtr);
    }
    for (auto callback : std::ranges::reverse_view(callbacks)) {
        callback.first(this, callback.second);
    }
    context->decRefInternal();
}

cl_int MemObj::getMemObjectInfo(cl_mem_info paramName,
                                size_t paramValueSize,
                                void *paramValue,
                                size_t *paramValueSizeRet) {

    cl_int retVal = CL_INVALID_VALUE;
    size_t srcParamSize = GetInfo::invalidSourceSize;
    size_t clOffset = 0;
    void *srcParam = nullptr;
    void *hostPtr = nullptr;
    cl_context ctx = nullptr;
    cl_uint refCnt = 0;
    cl_uint mapCnt = 0;
    cl_mem clAssociatedMemObject = static_cast<cl_mem>(this->associatedMemObject);
    cl_bool usesSVMPointer = false;
    cl_bool usesCompression = false;
    auto memObjectType = this->getClObjectType();
    auto size = this->getApiSize();

    switch (paramName) {
    case CL_MEM_TYPE:
        srcParamSize = sizeof(memObjectType);
        srcParam = &memObjectType;
        break;

    case CL_MEM_FLAGS:
        srcParamSize = sizeof(flags);
        srcParam = &flags;
        break;

    case CL_MEM_SIZE:
        srcParamSize = sizeof(size);
        srcParam = &size;
        break;

    case CL_MEM_HOST_PTR:
        if (this->properties.flags.useHostPtr) {
            hostPtr = this->cpuPtr;
        }
        srcParamSize = sizeof(hostPtr);
        srcParam = &hostPtr;
        break;

    case CL_MEM_CONTEXT:
        srcParamSize = sizeof(context);
        ctx = context;
        srcParam = &ctx;
        break;

    case CL_MEM_USES_SVM_POINTER:
        usesSVMPointer = this->usesSvm;
        srcParamSize = sizeof(cl_bool);
        srcParam = &usesSVMPointer;
        break;

    case CL_MEM_OFFSET:
        clOffset = this->offset;
        srcParamSize = sizeof(clOffset);
        srcParam = &clOffset;
        break;

    case CL_MEM_ASSOCIATED_MEMOBJECT:
        srcParamSize = sizeof(clAssociatedMemObject);
        srcParam = &clAssociatedMemObject;
        break;

    case CL_MEM_MAP_COUNT:
        srcParamSize = sizeof(mapCnt);
        mapCnt = static_cast<cl_uint>(this->mapOperationHandler.size());
        srcParam = &mapCnt;
        break;

    case CL_MEM_REFERENCE_COUNT:
        refCnt = static_cast<cl_uint>(this->getReference());
        srcParamSize = sizeof(refCnt);
        srcParam = &refCnt;
        break;

    case CL_MEM_ALLOCATION_HANDLE_INTEL:
        break;
    case CL_MEM_USES_COMPRESSION_INTEL:
        usesCompression = this->isCompressionEnabled();
        srcParamSize = sizeof(usesCompression);
        srcParam = &usesCompression;
        break;

    case CL_MEM_PROPERTIES:
        srcParamSize = propertiesVector.size() * sizeof(cl_mem_properties);
        srcParam = propertiesVector.data();
        break;
    case CL_L0_MEM_OBJ_HANDLE: {
        srcParamSize = this->isImage() ? sizeof(ze_image_handle_t) : sizeof(void *);
        srcParam = this->isImage() ? reinterpret_cast<void *>(NEO::LEO::castToObject<Image>(this)->getL0HandleRef()) : castToObject<Buffer>(this)->getUsmPtrRef();
        break;
    }
    default:
        this->getOsSpecificMemObjectInfo(paramName, &srcParamSize, &srcParam);
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, srcParam, srcParamSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcParamSize, getInfoStatus);

    return retVal;
}

void MemObj::storeProperties(const cl_mem_properties *properties) {
    if (properties) {
        for (size_t i = 0; properties[i] != 0; i += 2) {
            propertiesVector.push_back(properties[i]);
            propertiesVector.push_back(properties[i + 1]);
        }
        propertiesVector.push_back(0);
    }
}

bool MemObj::readMemObjFlagsInvalid() {
    return isValueSet(flags, CL_MEM_HOST_WRITE_ONLY) || isValueSet(flags, CL_MEM_HOST_NO_ACCESS);
}

bool MemObj::writeMemObjFlagsInvalid() {
    return isValueSet(flags, CL_MEM_HOST_READ_ONLY) || isValueSet(flags, CL_MEM_HOST_NO_ACCESS);
}

bool MemObj::mapMemObjFlagsInvalid(cl_map_flags mapFlags) {
    return (writeMemObjFlagsInvalid() && (isValueSet(mapFlags, CL_MAP_WRITE_INVALIDATE_REGION) || isValueSet(mapFlags, CL_MAP_WRITE))) ||
           (readMemObjFlagsInvalid() && (isValueSet(mapFlags, CL_MAP_READ)));
}

} // namespace LEO
} // namespace NEO
