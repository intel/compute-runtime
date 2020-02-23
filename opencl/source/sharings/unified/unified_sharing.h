/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/source/sharings/unified/unified_sharing_types.h"

#include "CL/cl.h"

#include <functional>
#include <mutex>
#include <unordered_map>

namespace NEO {

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
    void releaseResource(MemObj *memObject) override;

    static GraphicsAllocation *createGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description, GraphicsAllocation::AllocationType allocationType);

  private:
    UnifiedSharingFunctions *sharingFunctions;
    UnifiedSharingHandleType memoryType;
};

} // namespace NEO
