#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(run_aub_tests ALL)
set_target_properties(run_aub_tests PROPERTIES FOLDER aub_product_targets)

add_subdirectories()
