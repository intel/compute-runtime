/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/helpers/windows/gmm_callbacks.inl"

using namespace NEO;

template struct DeviceCallbacks<SKLFamily>;
template struct TTCallbacks<SKLFamily>;
