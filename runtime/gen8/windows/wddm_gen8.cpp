/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/windows/wddm_engine_mapper.h"
#include "runtime/os_interface/windows/wddm_engine_mapper.inl"

namespace OCLRT {

typedef BDWFamily Family;

template class WddmEngineMapper<BDWFamily>;
} // namespace OCLRT
