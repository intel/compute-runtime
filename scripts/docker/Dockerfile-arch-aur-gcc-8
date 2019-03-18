FROM docker.io/archlinux/base
MAINTAINER Jacek Danecki <jacek.danecki@intel.com>

COPY neo /root/neo
COPY scripts/prepare-arch-aur-gcc-8.sh /root
COPY scripts/build-arch-dep.sh /root

RUN /root/prepare-arch-aur-gcc-8.sh
RUN cd /root/build ; cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ../neo ; \
    ninja -j `nproc`
CMD ["/bin/bash"]
