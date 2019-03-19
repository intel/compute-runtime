FROM docker.io/ubuntu:18.04
MAINTAINER Jacek Danecki <jacek.danecki@intel.com>

COPY neo /root/neo

RUN apt-get -y update ; apt-get install -y --allow-unauthenticated gpg wget; \
    echo "deb http://ppa.launchpad.net/ocl-dev/intel-opencl/ubuntu bionic main" >> /etc/apt/sources.list; \
    echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main" >> /etc/apt/sources.list; \
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key |  apt-key add - ; \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys C3086B78CC05B8B1; \
    apt-get -y update ; apt-get install -y --allow-unauthenticated cmake git pkg-config ninja-build intel-igc-opencl-dev intel-gmmlib-dev clang-8
RUN mkdir /root/build; cd /root/build ; cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang-8 -DCMAKE_CXX_COMPILER=clang++-8 \
    -DDO_NOT_RUN_AUB_TESTS=1 -DDONT_CARE_OF_VIRTUALS=1 ../neo ; ninja -j `nproc`
CMD ["/bin/bash"]
