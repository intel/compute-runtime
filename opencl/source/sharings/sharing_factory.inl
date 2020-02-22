/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/sharings/sharing_factory.h"

namespace NEO {

template <typename F, typename T>
SharingFactory::RegisterSharing<F, T>::RegisterSharing() {
    sharingContextBuilder[T::sharingId] = new F;
};
} // namespace NEO
