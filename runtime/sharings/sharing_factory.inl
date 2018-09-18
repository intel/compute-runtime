/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/sharings/sharing_factory.h"

namespace OCLRT {

template <typename F, typename T>
SharingFactory::RegisterSharing<F, T>::RegisterSharing() {
    sharingContextBuilder[T::sharingId] = new F;
};
} // namespace OCLRT
