FROM docker.io/base/archlinux
MAINTAINER Jacek Danecki <jacek.danecki@intel.com>

COPY neo /root/neo
COPY scripts/prepare-arch-gcc-8.sh /root
COPY scripts/build-arch-dep.sh /root
COPY scripts/prepare-workspace.sh /root

RUN /root/prepare-arch-gcc-8.sh
RUN /root/prepare-workspace.sh
RUN cd /root/build ; cmake -G Ninja -DBUILD_TYPE=Release -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DSUPPORT_GEN8=0 \
    -DDO_NOT_RUN_AUB_TESTS=1 -DDONT_CARE_OF_VIRTUALS=1 ../neo ; ninja -j `nproc`
CMD ["/bin/bash"]
