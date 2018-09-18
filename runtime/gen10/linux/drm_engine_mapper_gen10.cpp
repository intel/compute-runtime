/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/linux/drm_engine_mapper.h"
#include "runtime/os_interface/linux/drm_engine_mapper.inl"

namespace OCLRT {

template class DrmEngineMapper<CNLFamily>;
} // namespace OCLRT
