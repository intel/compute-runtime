/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/gmm_callbacks.h"
#include "runtime/helpers/gmm_callbacks.inl"

#include "hw_cmds.h"

using namespace NEO;

template struct DeviceCallbacks<CNLFamily>;
template struct TTCallbacks<CNLFamily>;
