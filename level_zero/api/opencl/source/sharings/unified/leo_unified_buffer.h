/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/source/sharings/unified/leo_unified_sharing.h"

#include "CL/cl.h"

namespace NEO {
struct MemoryProperties;

namespace LEO {

class Buffer;
class ClDevice;
class Context;
enum class UnifiedSharingHandleType;
struct UnifiedSharingMemoryDescription;

class UnifiedBuffer : public UnifiedSharing {
    using UnifiedSharing::UnifiedSharing;

  public:
    static Buffer *createSharedUnifiedBuffer(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription description, cl_int *errcodeRet);
    static Buffer *importExternalMemoryBuffer(Context *context, MemoryProperties memoryProperties, cl_mem_flags flags, UnifiedSharingMemoryDescription description,
                                              ClDevice *importDevice, cl_int *errcodeRet);
    static ClDevice *selectExternalMemoryImportDevice(Context *context, const MemoryProperties &memoryProperties);
};

} // namespace LEO
} // namespace NEO
