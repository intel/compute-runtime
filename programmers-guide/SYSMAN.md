<!---

Copyright (C) 2024 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Level Zero Sysman Initialization

* [Introduction](#Introduction)
* [Initialization](#Initialization)
* [Support and Limitations](#Support-and-Limitations)
* [Recommendation](#Recommendation)

# Introduction

The following document describes limitations of using different initialization modes of System Resource Management Library (Sysman) in Level Zero. Implementation independent information on Level-Zero Sysman initialization are described in the Level-Zero specification [Sysman Programming Guide Section](https://spec.oneapi.io/level-zero/latest/sysman/PROG.html#sysman-programming-guide).

# Initialization

An application can initialize Level Zero Sysman in following modes:

* [zeInit](https://spec.oneapi.io/level-zero/latest/core/api.html#zeinit) with [ZES_ENABLE_SYSMAN](https://spec.oneapi.io/level-zero/latest/sysman/PROG.html#environment-variables) environment variable (also referenced as "Legacy mode" for brevity in this document).
* [zesInit](https://spec.oneapi.io/level-zero/latest/sysman/api.html#zesinit)

Psuedo code for the above can be referenced from [spec](https://spec.oneapi.io/level-zero/latest/sysman/PROG.html#sysman-programming-guide).

# Support and Limitations

Following table summarizes the effect of using the specified initialization calls in a single user process.

| Initialization  Mode                                                                      | Core <-> Sysman Device Handle Casting | Core and Sysman device handle mapping                                                                           | Sysman API's Support                                                                                                                 | Platform Support                                  |
|-------------------------------------------------------------------------------------------|---------------------------------------|-----------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------|
| Legacy mode                                                                               | Supported                             | Core <-> Sysman Device Handle Casting                                                                           | Supported till [v1.5](https://spec.oneapi.io/releases/index.html#level-zero-v1-5-0).<br> API's introduced post 1.5 are not supported | Supported up to XeHPC (PVC) and earlier platforms |
| zesInit                                                                                   | Not supported                         | [Sysman device mapping](https://spec.oneapi.io/level-zero/latest/sysman/api.html#sysmandevicemapping-functions) | All API's are supported                                                                                                              | All Platforms supported                           |
| zesInit + (zeInit W/o ZES_ENABLE_SYSMAN) Or <br> (zeInit W/o ZES_ENABLE_SYSMAN) + zesInit | Not supported                         | [Sysman device mapping](https://spec.oneapi.io/level-zero/latest/sysman/api.html#sysmandevicemapping-functions) | All API's are supported                                                                                                              | All Platforms supported                           |
| zesInit + (Legacy mode) Or <br> (Legacy mode) + zesInit                                   | Not supported                         | Not supported                                                                                                   | Not supported                                                                                                                        | Not supported                                     |

* Initialization with Legacy mode is supported only if Level Zero Core is operating on [composite device hierarchy](https://spec.oneapi.io/level-zero/latest/core/PROG.html#device-hierarchy) model.

# Recommendation

It is recommended to use zesInit initialization mode over legacy mode.