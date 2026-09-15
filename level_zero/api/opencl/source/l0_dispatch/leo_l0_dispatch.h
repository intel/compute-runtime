/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace NEO {

class OsLibrary;

namespace LEO {

// Where the Level Zero calls LEO makes go.
//
// LEO is built into the Level Zero driver, so the ze* calls it makes resolve
// inside libze_intel_gpu: the driver is linked with -Wl,-Bsymbolic and the
// release version script exports only the proc address table getters. They
// never transit libze_loader, so no loader layer can observe them, and tracing
// an OpenCL application running on LEO captures the OpenCL calls and no Level
// Zero calls at all.
//
// Every routed API is declared here as a function pointer of the same name,
// holding the driver entry point. LEO calls ze* unqualified from inside this
// namespace, so name lookup finds these rather than the global API and the call
// sites need no change. Arming (EnableLEOLoaderDispatch) overwrites the ones the
// loader exports, which puts the loader layers in the path; the rest keep
// calling the driver. Unarmed, a call is one load and an indirect call, with no
// branch and nothing to test.
//
// These have to be pointers rather than forwarding functions of the same name.
// Level Zero handles are pointers to types in the global namespace, so argument
// dependent lookup would find the global API alongside a function declared
// here and every call would be ambiguous. Ordinary lookup finding a variable
// suppresses argument dependent lookup, so a pointer is what makes the
// shadowing work at all.
//
// Nothing outside LEO is affected: level_zero/api/core is untouched, the loader
// still dispatches through the DDI tables, and a pure Level Zero application
// still calls the driver entry points directly.
#define LEO_L0_ENTRY(api) extern decltype(&::api) api;
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch_entries.inl"
#undef LEO_L0_ENTRY

// Arms the dispatch when configured to, and only when the loader is verified to
// hand back native driver handles. Idempotent; runs before LEO issues its first
// Level Zero call.
void initL0Dispatch();

// The arming itself, without the one-shot guard, and whether it armed. Split
// out so unit tests can drive it more than once. Does nothing at all when the
// dispatch is not configured on, so the entries keep the driver they were
// constant initialized with.
bool armL0Dispatch();

// Puts the driver back in every entry. Used when arming gets as far as the
// loader and then has to back out, and by unit tests between cases. Deliberately
// does not release the loader - see loaderLibrary.
void disarmL0Dispatch();

// The direct zeInitDrivers that the handle check compares the loader against.
// Held as a pointer so unit tests can substitute it, the same way they
// substitute OsLibrary::loadFunc.
extern decltype(&::zeInitDrivers) directInitDrivers;

// The loader, kept for the lifetime of the process once it has been loaded, and
// deliberately never freed.
//
// Unloading it is not safe. The loader's context destructor frees every driver
// library it loaded, and this one is among them, because LEO asking it for
// drivers is what made it load them. Dropping the last reference therefore asks
// the loader to unload the library whose code is running - and on the back-out
// path that happens inside a call, with only the OpenCL ICD's separate reference
// keeping the mapping alive. It is also why the handle cannot live in anything
// whose destructor runs at exit.
//
// Exposed so that unit tests can install a mock and dispose of it, which is the
// one case where freeing is correct; nothing frees the real loader.
extern OsLibrary *loaderLibrary;

} // namespace LEO
} // namespace NEO
