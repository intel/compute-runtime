/*
 * Copyright (c) 2017, Intel Corporation
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
#include "runtime/helpers/flush_stamp.h"

using namespace OCLRT;

FlushStampTracker::FlushStampTracker(bool allocateStamp) {
    if (allocateStamp) {
        flushStampSharedHandle = new FlushStampTrackingObj();
        flushStampSharedHandle->incRefInternal();
    }
}

FlushStampTracker::~FlushStampTracker() {
    if (flushStampSharedHandle) {
        flushStampSharedHandle->decRefInternal();
    }
}

FlushStamp FlushStampTracker::peekStamp() const {
    return flushStampSharedHandle->flushStamp.load();
}

void FlushStampTracker::setStamp(FlushStamp stamp) {
    flushStampSharedHandle->flushStamp.store(stamp);
}

void FlushStampTracker::replaceStampObject(FlushStampTrackingObj *stampObj) {
    if (stampObj) {
        stampObj->incRefInternal();
        if (flushStampSharedHandle) {
            flushStampSharedHandle->decRefInternal();
        }
        flushStampSharedHandle = stampObj;
    }
}

void FlushStampUpdateHelper::insert(FlushStampTrackingObj *stampObj) {
    if (stampObj) {
        flushStampsToUpdate.push_back(stampObj);
        stampObj->incRefInternal();
    }
}

void FlushStampUpdateHelper::updateAll(FlushStamp &flushStamp) {
    for (const auto &stamp : flushStampsToUpdate) {
        stamp->flushStamp.store(flushStamp);
    }
}

size_t FlushStampUpdateHelper::size() const {
    return flushStampsToUpdate.size();
}

FlushStampUpdateHelper::~FlushStampUpdateHelper() {
    for (const auto &stamp : flushStampsToUpdate) {
        stamp->decRefInternal();
    }
}
