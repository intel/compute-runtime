<!---

Copyright (C) 2020-2021 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Contribution guidelines

## Process overview

### 1. Patch creation

Start with a patch (we prefer smaller self-contained incremental changes vs. large blobs of code).
When adding new code, please also add corresponding unit level tests (ULT). Added ULTs should cover
all the decision points introduced by the commit and should fail if executed without the code changes.
Make sure it builds and passes _all_ ULT tests. For details about what compilers
and build configurations we expect, refer to instructions for
[building](https://github.com/intel/compute-runtime/blob/master/BUILD.md) the driver.
Make sure you adhere to our
[coding standard](https://github.com/intel/compute-runtime/blob/master/GUIDELINES.md);
this will be verified by clang-format and clang-tidy
(tool configuration is already included in NEO repository).

### 2. Certificate of origin

In order to get a clear contribution chain of trust we use the
[signed-off-by language](https://developercertificate.org/) used by the Linux kernel project.
Please make sure your commit message adheres to this guideline.

### 3. Commit message content

All contributions should follow specific [commit message](#commit-message-guidelines) guidelines outlined below.

### 4. Patch submission

Create a pull request on github once you are confident that your changes are complete and fulfill
the requirements above. Make sure your commit message follows these rules:
* each line has 80 character limit
* title (first line) should be self-contained (i.e. make sense without looking at the body)
* additional description can be provided in the body
* title and body need to be separated by an empty line

### 5. Initial (cursory) review

One of NEO maintainers will do an initial (brief) review of your code.
We will let you know if anything major is missing.

### 6. Verification

We'll double-check that your code meets all of our minimal quality expectations. Every commit must:
* Build under Linux - using clang (8.0) and gcc (7.x ... 9.0)
* Build under Windows (this is currently a requirement that cannot be verified externally)
* Pass ULTs for all supported platforms
* Pass clang-format check with the configuration contained within repository
* Pass clang-tidy check with the configuration contained within repository
* Pass sanity testing
(test content recommendation for the external community will be provided in the future)

When all the automated checks are confirmed to be passing, we will start actual code review process.

### 7. Code review

We'll make sure that your code fits within the architecture and design of NEO, is readable
and maintainable. Please make sure to address our questions and concerns.

### 8. Patch disposition

We reserve, upon conclusion of the code review, the right to do one of the following:
1. Merge the patch as submitted
1. Merge the patch (with modifications)
1. Reject the patch

If merged, you will be listed as patch author.
Your patch may be reverted later in case of major regression that was not detected prior to commit.

## Intel Employees

If you are an Intel Employee *and* you want to contribute to NEO as part of your regular job duties
please:
* Contact us in advance
* Make sure your github account is linked to your intel.com email address

## Commit Message Guidelines

### Introduction

The intention of the strict rules for the content and structure of the commit message is to make project history more readable.
Both Conventional Commits [specification](https://www.conventionalcommits.org/en/v1.0.0/) and Angular's [Commit Message Guidelines](https://github.com/angular/angular/blob/22b96b9/CONTRIBUTING.md#commit) inspired the rules outlined below

### Commit Message Format

Each commit message consists of a **header**, a **body** and a **footer**.  The header has a special
format that includes a **type**, a **scope** and a **subject**:

```
<type>(<scope>): <subject>
<BLANK LINE>
<body>
<BLANK LINE>
<footer>
```

The **type** is mandatory and the **scope** of the header is optional. Both **type** and **scope** should always be **lowercase**. Multiple values of **type** and **scope** are not allowed.

Provide a couple sentences of human-readable content in the **body** of the commit message. Focus on the purpose of the changes.

Metadata associated with commit should be included in the **footer** part. Currently, it is expected to contain certificate of origin ( _Signed-Off-By:_ ) and tracker reference ( _Resolves:_ / _Related-To:_ ). 

Allowed values for the **scope** are listed at the end of the [lint.yml](https://github.com/intel/compute-runtime/blob/master/.github/lint.yml) and should be extended sparingly, with extra caution.

Allowed values for the **type** are 
* build
* ci
* documentation
* feature
* fix
* performance
* refactor
* test

Use the following checklist to determine which **type** to use in your commit:

0. Revert of a prior commit is a special case - use the following commit message format:
```
Revert "<subject of commit being reverted>"

This reverts commit <ID of commit being reverted>
```
    * Example: 3d3ee8dccb71ddedbc500e3569f024b1775c505a

1. Use **type** == **documentation** when your commit modifies _only_ the human readable documentation
	* this is currently applicable to markdown files, including programming guide
	
2. Use **type** == **ci** when your commit modifies _only_ the files with metadata related to the way we test, but does not affect local build flow

3. Use **type** == **build** when your commit modifies the build flow, but does not modify the codebase of compute-runtime itself
	* example: updating the header dependencies in third_party folder

4. Use **type** == **test** when your commit modifies _only_ the files in test folders (unit_test, target_aub_tests, target_unit_tests etc.) and not the actual runtime code. Also, modification of `aub_configs` component in `manifest.yml` is considered as test-only change

5. Use **type** == **performance** when your commit is intended to improve observable performance without affecting functionality, 
    * when in doubt whether to use performance vs. fix as type, performance is usually a better fit
	
6. Use **type** == **fix** when your commit is intended to improve the functional behavior of the compute-runtime
    * when in doubt whether to use fix vs. feature as type, consider whether the feature being modified is enabled by default and already available in a posted release. If it is, use type == fix, otherwise use type == feature

7. Use **type** == **feature** when your commit is intended to add new functionality, 
    * New functionality means meaningful changes in functional behavior that would be detected by a black-box test in deterministic manner
	* For features not intended to be eventually enabled by default (e.g. debugability / logging improvements / code only enabled via debug keys) - use **scope** == **internal**

8. Use **type** == **refactor** for all other cases

Note: usage of tracker reference with the _Resolves:_ or _Related-To:_ notation is mandatory for **type** == **feature** or **scope** == **wa**, and optional for all other values.
