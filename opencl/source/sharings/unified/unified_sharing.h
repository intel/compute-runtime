/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/sharings/sharing.h"
#include "opencl/source/sharings/unified/unified_sharing_types.h"

#include "CL/cl.h"

namespace NEO {
struct ImageInfo;
class MultiGraphicsAllocation;

class UnifiedSharingFunctions : public SharingFunctions {
  public:
    uint32_t getId() const override {
        return UnifiedSharingFunctions::sharingId;
    }
    static const uint32_t sharingId;
};

class UnifiedSharing : public SharingHandler {
  public:
    UnifiedSharing(UnifiedSharingFunctions *sharingFunctions, UnifiedSharingHandleType memoryType);

    UnifiedSharingFunctions *peekFunctionsHandler() { return sharingFunctions; }
    UnifiedSharingHandleType getExternalMemoryType() { return memoryType; }

  protected:
    void synchronizeObject(UpdateData &updateData) override;
    void releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) override;

    static std::unique_ptr<MultiGraphicsAllocation> createMultiGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description, ImageInfo *imgInfo, AllocationType allocationType, cl_int *errcodeRet);

  private:
    UnifiedSharingFunctions *sharingFunctions;
    UnifiedSharingHandleType memoryType;
};

} // namespace NEO
