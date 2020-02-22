/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen9/hw_cmds.h"
#include "core/helpers/windows/gmm_callbacks.h"
#include "core/helpers/windows/gmm_callbacks.inl"

using namespace NEO;

template struct DeviceCallbacks<SKLFamily>;
template struct TTCallbacks<SKLFamily>;
