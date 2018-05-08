# GoogleTest and GoogleMock libraries

Version used 1.7.0

## Generating

To create required files:

* clone version 1.7.0 of googlemock (https://github.com/google/googlemock)
* go to gtest folder
* clone version 1.7.0 of googletest (https://github.com/google/googlemock)
* create output directory and run python script to create fused version
```
git clone -b release-1.7.0 https://github.com/google/googlemock gmock
cd gmock
git clone -b release-1.7.0 https://github.com/google/googletest gtest
cd scripts
mkdir /tmp/out
./fuse_gmock_files.py .. /tmp/out/
```
* output files are stored in /tmp/out
