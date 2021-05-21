# syntax = docker/dockerfile:experimental

FROM nvidia/cuda:11.1-devel-ubuntu20.04

RUN echo "deb http://ppa.launchpad.net/apt-fast/stable/ubuntu bionic main" >/etc/apt/sources.list.d/apt-fast.list && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys A2166B8DE8BDC3367D1901C11EE2FF37CA8DA16B && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y apt-fast && \
    apt-fast install -y sudo git build-essential libopenexr-dev libopencolorio-dev opencolorio-tools libtbb-dev libembree-dev \
        'libboost-(filesystem|iostreams|locale|regex|system|thread|wave|program-options)1.71-dev'

RUN --mount=type=ssh \
    mkdir -p -m 0700 ~/.ssh && \
    ssh-keyscan github.com >~/.ssh/known_hosts && \
    git clone --branch v7.2.0 git@github.com:recogni/nvidia-optix-sdk /tmp/nvidia-optix-sdk && \
    mkdir /NVIDIA-OptiX-SDK && \
    sh /tmp/nvidia-optix-sdk/installer/NVIDIA-OptiX-SDK-*.sh --prefix=/NVIDIA-OptiX-SDK --skip-license && \
    rm -rf /tmp/nvidia-optix-sdk

COPY /build_files/build_environment/install_deps.sh /blender/workspace/build_files/build_environment/

RUN cd /blender && \
    sed 's/apt-get[[:space:]][[:space:]]*install/apt-fast install/' <workspace/build_files/build_environment/install_deps.sh >install_deps.sh && \
    chmod +x install_deps.sh && \
    TERM=dumb ./install_deps.sh --no-confirm --skip-ocio --skip-openexr --skip-osl --skip-alembic --skip-boost --build-numpy --build-python

COPY / /blender/workspace

RUN cd /blender && \
    grep '^[[:space:]]*make' BUILD_NOTES.txt | sed 's/"[[:space:]]*$/ -D WITH_CYCLES_CUDA_BINARIES=ON -D WITH_CYCLES_DEVICE_OPTIX=ON -D OPTIX_ROOT_DIR=\/NVIDIA-OptiX-SDK -D WITH_OPENCOLORIO=ON" full/' | tee build.sh && \
    chmod +x build.sh && \
    cd workspace && \
    ../build.sh

RUN dpkg -S $(ldd /blender/build_linux_full/bin/blender | fgrep '=>' | awk '{print $3"\n/usr"$3}') 2>/dev/null | \
        awk -F: '{print $1}' | sort -u >/blender/build_linux_full/bin/packages.txt

FROM nvidia/cuda:11.1-devel-ubuntu20.04

COPY --from=0 \
    /blender/build_linux_full/bin /opt/blender

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $(cat /opt/blender/packages.txt) && \
    rm -rf /var/lib/apt/lists/*

COPY --from=0 \
    /opt /opt

COPY --from=0 \
    /etc/ld.so.conf.d/*.conf \
        /etc/ld.so.conf.d/

RUN ldconfig && \
    ln -s python3.9 $(echo /opt/blender/*/python/bin)/python3 && \
    /opt/blender/*/python/bin/python3 -m ensurepip

ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES all

ENTRYPOINT ["/opt/blender/blender"]
CMD []
