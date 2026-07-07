/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.h"

namespace NEO {
namespace LEO {

template <typename F, typename T>
SharingFactory::RegisterSharing<F, T>::RegisterSharing() {
    sharingContextBuilder[T::sharingId] = new F;
};

} // namespace LEO
} // namespace NEO
