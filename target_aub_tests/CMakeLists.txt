#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(run_aub_tests)
set(AUB_TESTS_TARGETS_FOLDER "aub_product_targets")
set_target_properties(run_aub_tests PROPERTIES FOLDER ${AUB_TESTS_TARGETS_FOLDER})

add_subdirectories()
