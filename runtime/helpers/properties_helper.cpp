/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/helpers/mipmap.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/mem_obj.h"

namespace OCLRT {
TransferProperties::TransferProperties(MemObj *memObj, cl_command_type cmdType, cl_map_flags mapFlags, bool blocking,
                                       size_t *offsetPtr, size_t *sizePtr, void *ptr)
    : memObj(memObj), cmdType(cmdType), mapFlags(mapFlags), blocking(blocking), ptr(ptr) {

    // no size or offset passed for unmap operation
    if (cmdType != CL_COMMAND_UNMAP_MEM_OBJECT) {
        if (memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
            size[0] = *sizePtr;
            offset[0] = *offsetPtr;
        } else {
            size = {{sizePtr[0], sizePtr[1], sizePtr[2]}};
            offset = {{offsetPtr[0], offsetPtr[1], offsetPtr[2]}};
            if (isMipMapped(memObj)) {
                // decompose origin to coordinates and miplevel
                mipLevel = findMipLevel(memObj->peekClMemObjType(), offsetPtr);
                mipPtrOffset = getMipOffset(castToObjectOrAbort<Image>(memObj), offsetPtr);
                auto mipLevelIdx = getMipLevelOriginIdx(memObj->peekClMemObjType());
                if (mipLevelIdx < offset.size()) {
                    offset[mipLevelIdx] = 0;
                }
            }
        }
    }
}
} // namespace OCLRT
