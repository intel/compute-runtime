FROM fedora:27
MAINTAINER Jacek Danecki <jacek.danecki@intel.com>

COPY neo /root/neo
COPY scripts/prepare-workspace.sh /root

RUN dnf install -y clang make cmake ninja-build git wget pkg-config xz ncurses-compat-libs findutils
RUN /root/prepare-workspace.sh
RUN cd /root/build ; cmake -G Ninja -DBUILD_TYPE=Release -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DDO_NOT_RUN_AUB_TESTS=1 -DDONT_CARE_OF_VIRTUALS=1 ../neo ; ninja -j `nproc`
CMD ["/bin/bash"]
