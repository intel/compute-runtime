<!---

Copyright (C) 2018-2022 Intel Corporation

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
a general rule test shouldn't be longer then 1ms in Debug driver.

# Coding guidelines
* Favor the design of a self-explanatory code over the use of comments; if comments are needed, use double slash instead of block comments
* HW commands and structures used in NEO must be initialized with constants defines for each Gfx Family: i.e. PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl
* Any new HW command or structure must have its own static constant initializer added to any Gfx Family that is going to use it.
* One-line branches use braces
* Headers are guarded using `#pragma once`
* Do not use `TODO`s in the code
* Use `UNRECOVERABLE_IF` and `DEBUG_BREAK_IF` instead of `asserts`:
  * Use `UNRECOVERABLE_IF` when a failure is found and driver cannot proceed with normal execution. `UNRECOVERABLE_IF` is implemented in Release and Debug builds.
  * Use `DEBUG_BREAK_IF` when a failure can be handled gracefully by the driver and it can continue with normal execution. `DEBUG_BREAK_IF` is only implemented in Debug builds.

## UNRECOVERABLE_IF macro

The NEO code uses the UNRECOVERABLE macro to abort execution in the following cases:
* Driver is in an undefined state from which it cannot recover nor easily return error code
* Execution entered an unexpected path that is not supported 

The abort mechanism guarantees that the error is caught as early as possible, which makes debug and fixing easier. 
