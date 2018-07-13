# Using NEO runtime with VTune Amplifier

You can use the Intel VTune Amplifier to identify GPU "hotspots". It will show GPGPU queue, GPU usage, memory throughputs, etc.
Using this tool, you can compare how the application behaves under different configurations (LWS, GWS, driver versions, etc.) and identify bottlenecks.

## Requirements

* [Intel(R) VTune(tm) Amplifier](https://software.intel.com/en-us/intel-vtune-amplifier-xe)
* [Intel(R) SDK for OpenCL(tm) Applications](https://software.intel.com/en-us/intel-opencl/download)
* [Intel(R) Metrics Discovery Application Programming Interface](https://github.com/intel/metrics-discovery)
* Current Intel(R) OpenCL(tm) GPU driver

## Installation

Note: This is an example. Actual filenames may differ

1. Install OpenCL SDK & VTune

```
cd
tar xvf intel_sdk_for_opencl_2017_7.0.0.2568_x64.gz
tar xvf vtune_amplifier_2018_update2.tar.gz
sudo dpkg -i intel-opencl_18.26.10987_amd64.deb
cd ~/intel_sdk_for_opencl_2017_7.0.0.2568_x64/; sudo ./install_GUI.sh
cd ~/vtune_amplifier_2018_update2/; sudo ./install_GUI.sh #use offline activation with file
```

To verify that VTune was installed properly run:

```
lsmod | grep sep4
```

This should return 2 lines. Otherwise follow sepdk installation in VTune documentation.


2. Compile and install MD API - see MD API [README](https://github.com/intel/metrics-discovery/blob/master/README.md) for instructions.

## Running VTune

```
/opt/intel/vtune_amplifier_2018/bin64/amplxe-gui
```

Note: If you built Metrics Discovery with libstdc++ > 3.4.20, please use the following workaround:

```
sudo sh -c 'LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libstdc++.so.6 /opt/intel/vtune_amplifier_2018/bin64/amplxe-gui'
```