/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

struct SDispatchTable;
struct SCRTDispatchTable;

struct SEntryPointsTable {
    SDispatchTable *icdDispatch;
    SCRTDispatchTable *crtDispatch;
};

extern SDispatchTable icdGlobalDispatchTable;
extern SCRTDispatchTable crtGlobalDispatchTable;
extern SEntryPointsTable globalDispatchTable;
