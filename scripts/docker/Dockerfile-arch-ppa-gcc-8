FROM docker.io/base/archlinux
MAINTAINER Jacek Danecki <jacek.danecki@intel.com>

COPY neo /root/neo
COPY scripts/prepare-arch-dep-ppa.sh /root

RUN pacman -Sy --noconfirm gcc cmake git wget pkg-config ninja
RUN /root/prepare-arch-dep-ppa.sh
RUN cd /root/build ; cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DDO_NOT_RUN_AUB_TESTS=1 -DDONT_CARE_OF_VIRTUALS=1 ../neo ; ninja -j `nproc`
CMD ["/bin/bash"]
