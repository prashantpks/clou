FROM ubuntu:20.04

CMD bash

ARG build_type="Debug"

RUN DEBIAN_FRONTEND=noninteractive apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install --no-install-suggests --no-install-recommends \
    autoconf automake autotools-dev libtool \
    clang-12 cmake git libgoogle-perftools-dev libprotobuf-dev llvm-12-dev make pkg-config protobuf-compiler wget libboost-dev \
    apt-utils apt-transport-https ca-certificates gnupg dialog \
    lldb-12 \
    tmux \
    less emacs-nox htop valgrind psmisc \
    unzip

ENV LLVM_DIR="/usr/lib/llvm-12"
ENV CC="/usr/bin/clang-12"
ENV CXX="/usr/bin/clang++-12"
ENV LCM_DIR="$HOME/lcm"
ENV LCM_BUILD="$LCM_DIR/build"

# Set up dummy libclou.so
WORKDIR "$LCM_BUILD"
RUN mkdir -p src
RUN echo 'static int i;' > dummy.c
RUN clang-12 -shared -o src/libclou.so dummy.c
RUN rm -f dummy.c

# Configure libsodium
RUN git clone https://github.com/jedisct1/libsodium.git libsodium

ENV LIBSODIUM_CPPFLAGS="-UHAVE_INLINE_ASM -UHAVE_EMMINTRIN_H -UHAVE_C_VARARRAYS -UHAVE_ALLOCA"

WORKDIR "$LCM_BUILD/libsodium"
RUN autoreconf -i
RUN ./configure --disable-asm CFLAGS="-Wno-cpp -Xclang -load -Xclang $LCM_BUILD/src/libclou.so" CPPFLAGS="$LIBSODIUM_CPPFLAGS"
RUN mkdir lcm

# WORKDIR "$LCM_BUILD/libsodium-v4"
# RUN autoreconf -i
# RUN ./configure --disable-asm CFLAGS="-Wno-cpp -Xclang -load -Xclang $LCM_BUILD/src/libclou.so" CPPFLAGS="$LIBSODIUM_CPPFLAGS"
# RUN mkdir lcm

# WORKDIR "$LCM_BUILD/libsodium-ll"
# RUN autoreconf -i
# RUN ./configure --disable-asm CC="$LCM_DIR/scripts/mycc.sh" CFLAGS="-Wno-cpp"
# RUN make -j$(nproc)

RUN ulimit -c unlimited
RUN mkdir -p /tmp/cores

# Build lcm tool
WORKDIR "$LCM_BUILD"
ARG z3_version=4.8.13
ADD https://github.com/Z3Prover/z3/releases/download/z3-${z3_version}/z3-${z3_version}-x64-glibc-2.31.zip z3-${z3_version}.zip
RUN unzip z3-${z3_version}
ENV Z3_DIR "${LCM_BUILD}/z3-${z3_version}-x64-glibc-2.31"
# RUN ln -s ${Z3_DIR}/bin ${Z3_DIR}/lib
COPY CMakeLists.txt $LCM_DIR/

ENV CXXFLAGS -fPIC
RUN [ -d ${Z3_DIR} ]
# RUN cmake -DCMAKE_BUILD_TYPE="${build_type}" -DLLVM_DIR="$LLVM_DIR" -DCMAKE_CXX_FLAGS="-fPIC" ..
# RUN make -j$(nproc)

ENV LD_LIBRARY_PATH="${Z3_DIR}/bin:$LD_LIBRARY_PATH"
