/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifndef NEO_DISABLE_SOURCE_FILE_PATHS
#define NEO_SOURCE_FILE_PATH __FILE__
#define NEO_FUNCTION_NAME __func__
#else
#define NEO_SOURCE_FILE_PATH ""
#define NEO_FUNCTION_NAME ""
#endif
