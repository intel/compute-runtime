/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

extern "C" {
const char *__asan_default_options() {
    return "allow_user_segv_handler=1";
}
}
