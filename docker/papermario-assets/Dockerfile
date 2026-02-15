FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH="/root/.cargo/bin:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    curl \
    git \
    rsync \
    make \
    python3 \
    python3-pip \
    python3-setuptools \
    build-essential \
    binutils-mips-linux-gnu \
    cpp-mips-linux-gnu \
    gcc-mips-linux-gnu \
    zlib1g-dev \
    libyaml-dev \
    ninja-build \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Rust toolchain + required Rust CLI tools used by papermario configure/build.
RUN curl https://sh.rustup.rs -sSf | bash -s -- -y --profile minimal --default-toolchain stable \
    && cargo install pigment64 crunch64-cli

COPY docker/papermario-assets/run-assets.sh /usr/local/bin/run-assets.sh
RUN sed -i 's/\r$//' /usr/local/bin/run-assets.sh && chmod +x /usr/local/bin/run-assets.sh

WORKDIR /work

ENTRYPOINT ["/bin/bash"]
