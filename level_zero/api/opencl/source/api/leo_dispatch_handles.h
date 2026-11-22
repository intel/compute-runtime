/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

struct SDispatchTable;
struct SCRTDispatchTable;

extern SDispatchTable icdGlobalDispatchTable;
extern SCRTDispatchTable crtGlobalDispatchTable;

struct SEntryPointsTable {
    SDispatchTable *icdDispatch;
    SCRTDispatchTable *crtDispatch;
};

extern SEntryPointsTable globalDispatchTable;
