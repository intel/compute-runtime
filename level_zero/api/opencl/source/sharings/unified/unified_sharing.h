/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/source/sharings/sharing.h"
#include "level_zero/api/opencl/source/sharings/unified/unified_sharing_types.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {

class Context;
enum class AllocationType;

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

  private:
    UnifiedSharingFunctions *sharingFunctions;
    UnifiedSharingHandleType memoryType;
};

} // namespace LEO
} // namespace NEO
