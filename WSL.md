<!---

Copyright (C) 2021-2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# WDDM GPU Paravirtualization support for WSL2

## Introduction

This document describes ingredients required in order to use Intel&reg; Graphics Compute Runtime for oneAPI Level Zero and OpenCL&trade; Driver in WSL2 environment.

## Host OS

- Required OS: [Windows 11](https://www.microsoft.com/en-us/software-download/windows11).
- Configuration used for validation: [25H2 update](https://support.microsoft.com/en-us/topic/windows-11-version-25h2-update-history-99c7f493-df2a-4832-bd2d-6706baa0dec0).

## WSL kernel

Tested with [6.1.3.0](https://github.com/microsoft/WSL/releases/tag/2.6.3).

## Windows Host Graphics Driver

Please use the latest production drivers for:

- [all currently supported platforms (Xe+)](https://www.intel.com/content/www/us/en/download/785597/intel-arc-graphics-windows.html).
- [legacy platforms (11-14th Gen)](https://www.intel.com/content/www/us/en/download/864990/intel-11th-14th-gen-processor-graphics-windows.html).

## Guest (WSL2) Intel&reg; Graphics Compute Runtime for oneAPI Level Zero and OpenCL&trade; Driver

For the WSL2 client, please use the [latest NEO release](https://github.com/intel/compute-runtime/releases). Supported platforms list is available there in the form of release notes.

General support for WSL in NEO was enabled in [this commit](https://github.com/intel/compute-runtime/commit/fad4ee7e246839c36c3f6b0e14ea0c79d9e4758a).
