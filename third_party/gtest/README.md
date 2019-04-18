# GoogleTest and GoogleMock libraries

Version used 1.9.0

## Generating

To create required files:

* clone version 1.9.0 of googletest (https://github.com/google/googletest)
* create output directory and run python script to create fused version

git clone https://github.com/google/googletest gtest
cd gtest/googlemock/scripts
mkdir /tmp/out
./fuse_gmock_files.py .. /tmp/out/
```
* output files are stored in /tmp/out
