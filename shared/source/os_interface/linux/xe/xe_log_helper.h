/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"

#define XELOG(...) PRINT_STRING(debugManager.flags.PrintXeLogs.get(), stderr, __VA_ARGS__)
