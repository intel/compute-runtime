<!---

Copyright (C) 2018-2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

File to cover guidelines for NEO project.

# C++ usage

* use c++ style casts instead of c style casts.
* do not use default parameters
* prefer using over typedef
* avoid defines for constants, use constexpr
* prefer forward declarations in headers
* avoid includes in headers unless absolutely necessary
* use of exceptions in driver code needs strong justification
* prefer static create methods returning std::unique_ptr instead of throwing from constructor
* inside methods, use an explicit `this->` pointer for referring to non-static class members
* avoid defining global constants with internal-linkage in header files; use inline variables from C++17 instead
* avoid using Unicode characters in code
* prefer const-correctness; mark members, locals and parameters const where possible and pass non-trivial types by const reference
* manage ownership with smart pointers (std::unique_ptr / std::make_unique); avoid raw new and delete
* prefer named constants over magic numbers
* do not use `using namespace` in headers

# Naming conventions

* use snake_case for new files
* use PascalCase for class, struct, enum, and namespace names
* use camelCase for variable and function names
* prefer verbose names for variables and functions
```
bad examples : sld, elws, aws
good examples : sourceLevelDebugger, enqueuedLocalWorkGroupSize, actualWorkGroupSize
```
* follow givenWhenThen test naming pattern, indicate what is interesting in the test

bad examples :
```
TEST(CsrTests, initialize)
TEST(CQTests, simple)
TEST(CQTests, basic)
TEST(CQTests, works)
```
good examples:
```
TEST(CommandStreamReceiverTests, givenCommandStreamReceiverWhenItIsInitializedThenProperFieldsAreSet)
TEST(CommandQueueTests, givenCommandQueueWhenEnqueueIsDoneThenTaskLevelIsModifed)
TEST(CommandQueueTests, givenCommandQueueWithDefaultParamtersWhenEnqueueIsDoneThenTaskCountIncreases)
TEST(CommandQueueTests, givenCommandQueueWhenEnqueueWithBlockingFlagIsSetThenDriverWaitsUntilAllCommandsAreCompleted)
```
# Testing mindset

* Test behaviors instead of implementations, do not focus on adding a test per every function in the
class (avoid tests for setters and getters), focus on the functionality you are adding and how it changes
the driver behavior, do not bind tests to implementation.
* Make sure that test is fast, our test suite needs to complete in seconds for efficient development pace, as
a general rule test shouldn't be longer than 1ms in Debug driver.
* When the code you add is multi-threaded, cover it with multi-threaded tests placed in the dedicated
mt_tests suite; do not test multi-threaded code outside of mt_tests.
* Validate changes where they live: cover changes in shared code with shared ULTs (`shared/test/unit_test`),
changes in Level Zero code with L0 ULTs (`level_zero/core/test/unit_tests`), and changes in OpenCL code
with OCL ULTs (`opencl/test/unit_test`).
* Keep tests independent; do not rely on test execution order.
* Reuse existing mocks and fixtures instead of introducing new ones.

# Coding guidelines
* Favor the design of a self-explanatory code over the use of comments; if comments are needed, use double slash instead of block comments
* Avoid code duplication; extract common logic into shared helpers instead of copying
* Refactor existing code when it improves clarity or removes duplication introduced by your change
* Do not add code to non-test driver sources that exists solely to support unit tests
* Avoid adding host overhead to the kernel dispatch APIs (zeCommandListAppendLaunchKernel / clEnqueueNDRangeKernel); gate new non-default behavior behind early-outs so these hot paths stay fast
* HW commands and structures used in NEO must be initialized with constants defines for each Gfx Family: i.e. PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl
* Any new HW command or structure must have its own static constant initializer added to any Gfx Family that is going to use it.
* One-line branches use braces
* Headers are guarded using `#pragma once`
* Do not use `TODO`s in the code
* Do not leave dead or commented-out code
* Use `UNRECOVERABLE_IF` and `DEBUG_BREAK_IF` instead of `asserts`:
  * Use `UNRECOVERABLE_IF` when a failure is found and driver cannot proceed with normal execution. `UNRECOVERABLE_IF` is implemented in Release and Debug builds.
  * Use `DEBUG_BREAK_IF` when a failure can be handled gracefully by the driver and it can continue with normal execution. `DEBUG_BREAK_IF` is only implemented in Debug builds.

## UNRECOVERABLE_IF macro

The NEO code uses the UNRECOVERABLE macro to abort execution in the following cases:
* Driver is in an undefined state from which it cannot recover nor easily return error code
* Execution entered an unexpected path that is not supported 

The abort mechanism guarantees that the error is caught as early as possible, which makes debug and fixing easier. 

# Documentation

* Keep documentation in sync with the code; when a change introduces, removes, or
reshapes a fundamental concept, update the documentation in the same change.
* A change is considered fundamental when it affects how the driver is structured or
how its pieces interact, for example: a new subsystem or core object, a new
hardware- or OS-abstraction mechanism, a change to the object/ownership model, the
command submission or memory management flow, or a new cross-cutting feature.
* Capture cross-cutting concepts in the top-level architecture overview
(`ARCHITECTURE.md`); document feature-specific design and optimization details under
`programmers-guide/`.
* When you change behavior that an existing document describes, update that document
so it keeps matching the code; do not leave stale documentation behind.
* Keep documentation generic and free of internal-only or non-public information,
since documentation may be published; describe concepts rather than restating code.
