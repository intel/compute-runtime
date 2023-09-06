/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/sharings/sharing.h"
#include "opencl/source/sharings/unified/unified_sharing_types.h"

namespace NEO {
struct ImageInfo;

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

    static GraphicsAllocation *createGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description, AllocationType allocationType);
    static GraphicsAllocation *createGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description, ImageInfo *imgInfo, AllocationType allocationType);

  private:
    UnifiedSharingFunctions *sharingFunctions;
    UnifiedSharingHandleType memoryType;
};

} // namespace NEO
