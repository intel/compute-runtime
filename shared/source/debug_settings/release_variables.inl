/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

/*This file contains variables which are read in the release mode.
  Adding variable here requires strong business need and management approval.*/

DECLARE_DEBUG_VARIABLE(bool, MakeAllBuffersResident, false, "Make all buffers resident after creation")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideDefaultFP64Settings, -1, "-1: dont override, 0: disable, 1: enable.")