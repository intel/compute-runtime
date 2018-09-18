/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/kmd_notify_properties.h"

using namespace OCLRT;

void KmdNotifyHelper::updateAcLineStatus() {}

int64_t KmdNotifyHelper::getBaseTimeout(const int64_t &multiplier) const {
    return properties->delayKmdNotifyMicroseconds * multiplier;
}
