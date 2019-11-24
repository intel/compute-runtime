/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/flush_stamp.h"

using namespace NEO;

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
    if (flushStampSharedHandle->initialized) {
        return flushStampSharedHandle->flushStamp;
    } else {
        return 0;
    }
}

void FlushStampTracker::setStamp(FlushStamp stamp) {
    if (stamp != 0) {
        flushStampSharedHandle->flushStamp = stamp;
        flushStampSharedHandle->initialized = true;
    }
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
    }
}

void FlushStampUpdateHelper::updateAll(const FlushStamp &flushStamp) {
    for (const auto &stamp : flushStampsToUpdate) {
        stamp->flushStamp = flushStamp;
        stamp->initialized = true;
    }
}

size_t FlushStampUpdateHelper::size() const {
    return flushStampsToUpdate.size();
}
