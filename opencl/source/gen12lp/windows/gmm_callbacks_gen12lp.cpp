/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "opencl/source/helpers/windows/gmm_callbacks_tgllp_plus.inl"

using namespace NEO;

template struct DeviceCallbacks<TGLLPFamily>;
template struct TTCallbacks<TGLLPFamily>;
