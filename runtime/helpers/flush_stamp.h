/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#pragma once

#include "runtime/helpers/completion_stamp.h"
#include "runtime/utilities/reference_tracked_object.h"
#include "runtime/utilities/stackvec.h"

namespace OCLRT {
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
    void updateAll(FlushStamp &flushStamp);
    size_t size() const;

  private:
    StackVec<FlushStampTrackingObj *, 64> flushStampsToUpdate;
};
} // namespace OCLRT
