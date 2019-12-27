/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen9/hw_cmds.h"
#include "runtime/helpers/windows/gmm_callbacks.h"
#include "runtime/helpers/windows/gmm_callbacks.inl"

using namespace NEO;

template struct DeviceCallbacks<SKLFamily>;
template struct TTCallbacks<SKLFamily>;
