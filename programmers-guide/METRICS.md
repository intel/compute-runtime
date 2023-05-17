<!---

Copyright (C) 2023 Intel Corporation

SPDX-License-Identifier: MIT

-->

# GPU Metrics collection in Level Zero

* [Introduction](#Introduction)
* [Dependencies](#Dependencies)
* [Environment Setup](#Environment-Setup)
* [EU Stall Sampling](#EU-Stall-Sampling)
* [Limitations](#Limitations)


# Introduction

Implementation independent details of Level-Zero metrics are described in the Level-Zero specification [Metrics Section](https://spec.oneapi.io/level-zero/latest/tools/PROG.html#metrics). This implementation supports Time-based and Event-based sampling. Two domains are supported, one for collecting GPU performance metrics and one for collecting EU stall sampling data (type ZET_METRIC_TYPE_IP).

# Dependencies

Metrics collection depends on:

* Intel(R) Metrics Discovery (MDAPI) - https://github.com/intel/metrics-discovery
* Intel(R) Metrics Library for MDAPI - https://github.com/intel/metrics-library

# Environment Setup

As described in Level-Zero specification [Tools Section](https://spec.oneapi.io/level-zero/latest/tools/PROG.html#environment-variables) environment variable `ZET_ENABLE_METRICS` must be set to 1.

## Linux
Additionally in Linux environment, is is required to disable the kernel module driver i915 performance stream paranoid mode. This can be done with command

```
 sudo sysctl dev.i915.perf_stream_paranoid=0
```

# EU Stall Sampling 

HW-assisted EU stall sampling allows statistically correlating Xe-Vector Engine (XVE) stall events to the executed instructions and breaks down the stall events by different stall reasons. Using the Instruction Pointer it is possible to point to the GPU kernel source code line causing the most stalls. 

# Limitations

## EU Stall Sampling 

* Only supported on Linux
* Does not support streamer markers
* The inherent nature of the samples only make sense for Time-based sampling. Therefore, Event-based sampling is not supported. 

## GPU performance metrics

* To obtain the most recent metric values using Time-based sampling, it is necessary to read all metrics reports from the hardware buffer and calculate them all. This may be costly operation if the hardware buffer is not read at frequent intervals. Therefore, it is recommended to call zetMetricStreamerReadData() at a time interval that does not require processing big number of reports. This can be calculated based on the sampling rate decided when opening the metrics streamer (zet_metric_streamer_desc_t.samplingPeriod).

## notifyEveryNReports

* Linux support for notifyEveryNReports on performance metrics will always return true when one metric report is available.
