/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
namespace NEO {
class OfflineCompiler;
class OfflineLinker;
} // namespace NEO

extern int buildWithSafetyGuard(NEO::OfflineCompiler *compiler);
extern int linkWithSafetyGuard(NEO::OfflineLinker *linker);