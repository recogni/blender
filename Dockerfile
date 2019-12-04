FROM nvidia/cuda:10.1-devel-ubuntu18.04

RUN echo "deb http://ppa.launchpad.net/apt-fast/stable/ubuntu bionic main" >/etc/apt/sources.list.d/apt-fast.list && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys A2166B8DE8BDC3367D1901C11EE2FF37CA8DA16B && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y apt-fast && \
    apt-fast install -y sudo git build-essential libopenexr-dev libopencolorio-dev opencolorio-tools

COPY / /blender/workspace

RUN cd /blender && \
    sed 's/apt-get[[:space:]][[:space:]]*install/apt-fast install/' <workspace/build_files/build_environment/install_deps.sh >install_deps.sh && \
    chmod +x install_deps.sh && \
    TERM=dumb ./install_deps.sh --no-confirm --skip-ocio --skip-openexr --skip-osl --skip-alembic --build-numpy

RUN cd /blender && \
    grep '^[[:space:]]*make' BUILD_NOTES.txt | sed 's/"[[:space:]]*$/ -D WITH_CYCLES_CUDA_BINARIES=ON -D WITH_OPENCOLORIO=ON" full/' | tee build.sh && \
    chmod +x build.sh && \
    cd workspace && \
    ../build.sh

RUN dpkg -S $(ldd /blender/build_linux_full/bin/blender | awk '{print $3}') 2>/dev/null | \
        awk '{print $1}' | sed 's/:.*$//' | sort -u >/blender/build_linux_full/bin/packages.txt

FROM nvidia/cuda:10.1-devel-ubuntu18.04

COPY --from=0 \
    /blender/build_linux_full/bin /opt/blender

RUN apt-get update && \
    apt-get install -y --no-install-recommends $(cat /opt/blender/packages.txt) && \
    rm -rf /var/lib/apt/lists/*

COPY --from=0 \
    /opt /opt

COPY --from=0 \
    /etc/ld.so.conf.d/blosc.conf \
    /etc/ld.so.conf.d/oiio.conf \
    /etc/ld.so.conf.d/openvdb.conf \
    /etc/ld.so.conf.d/osd.conf \
        /etc/ld.so.conf.d/

ENTRYPOINT ["/opt/blender/blender"]
CMD []
