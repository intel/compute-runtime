/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
#if defined(_WIN32)
unsigned int ultIterationMaxTime = 90;
#else
unsigned int ultIterationMaxTime = 45;
#endif
const char *executionName = "OCLOC";
} // namespace NEO
