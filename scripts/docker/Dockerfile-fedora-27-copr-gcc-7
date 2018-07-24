FROM fedora:27
MAINTAINER Jacek Danecki <jacek.danecki@intel.com>

COPY neo /root/neo

RUN dnf install -y gcc-c++ cmake ninja-build git pkg-config; \
    dnf install -y 'dnf-command(copr)'; \
    dnf copr enable -y arturh/intel-opencl-staging; \
    dnf copr enable -y arturh/intel-opencl-experimental; \
    dnf copr enable -y arturh/intel-opencl-unstable; \
    dnf copr enable -y arturh/intel-opencl; \
    dnf --showduplicate list intel-igc-opencl-devel; \
    dnf install -y intel-igc-opencl-devel; \
    cd /root; git clone --depth 1 https://github.com/intel/gmmlib gmmlib; \
    mkdir /root/build; cd /root/build ; cmake -G Ninja ../neo; \
    ninja -j `nproc`
CMD ["/bin/bash"]
