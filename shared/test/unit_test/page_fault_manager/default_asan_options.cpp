/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

extern "C" {
const char *__asan_default_options() { // NOLINT(readability-identifier-naming0
    return "allow_user_segv_handler=1";
}
}
