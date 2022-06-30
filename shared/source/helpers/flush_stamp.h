/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/utilities/reference_tracked_object.h"
#include "shared/source/utilities/stackvec.h"

namespace NEO {
struct FlushStampTrackingObj : public ReferenceTrackedObject<FlushStampTrackingObj> {
    FlushStamp flushStamp = 0;
    std::atomic<bool> initialized{false};
};

class FlushStampTracker {
  public:
    FlushStampTracker() = delete;
    FlushStampTracker(bool allocateStamp);
    ~FlushStampTracker();

    FlushStamp peekStamp() const;
    void setStamp(FlushStamp stamp);
    void replaceStampObject(FlushStampTrackingObj *stampObj);

    // Temporary. Method will be removed
    FlushStampTrackingObj *getStampReference() {
        return flushStampSharedHandle;
    }

  protected:
    FlushStampTrackingObj *flushStampSharedHandle = nullptr;
};

class FlushStampUpdateHelper {
  public:
    void insert(FlushStampTrackingObj *stampObj);
    void updateAll(const FlushStamp &flushStamp);
    size_t size() const;

  private:
    StackVec<FlushStampTrackingObj *, 64> flushStampsToUpdate;
};
} // namespace NEO
