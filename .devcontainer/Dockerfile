# ===== NS3 Development Container =====
# Base: Ubuntu 22.04 LTS
# Target: VSCode DevContainers & GitHub Codespaces
# NS3 Version: 3.47

FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Version configuration
ENV NS3_VERSION=3.46.1
ENV NS3_DIR=/ns-3

# ===== System Dependencies =====
# Consolidated package installation for optimal layer caching
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Build essentials
    build-essential \
    g++ \
    cmake \
    ninja-build \
    git \
    # Python for ns3 scripts
    python3 \
    # Development tools
    ccache \
    clang-format \
    clang-tidy \
    gdb \
    # Download utility
    wget \
    tar \
    ca-certificates \
    # Sync tool for staging operations
    rsync \
    # LLVM toolchain
    && wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc \
    && echo "deb https://apt.llvm.org/jammy/ llvm-toolchain-jammy-20 main" > /etc/apt/sources.list.d/llvm-20.list \
    && apt-get update && apt-get install -y --no-install-recommends \
    clang-format-20 \
    && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-20 100

# ===== NS3 Installation =====
# Download and extract ns-allinone
WORKDIR /tmp
RUN wget --no-check-certificate http://www.nsnam.org/releases/ns-allinone-${NS3_VERSION}.tar.bz2 \
    && tar -xjf ns-allinone-${NS3_VERSION}.tar.bz2 \
    && mv ns-allinone-${NS3_VERSION}/ns-${NS3_VERSION} ${NS3_DIR} \
    && rm -rf ns-allinone-${NS3_VERSION}*

# Configure ns3 with debug profile
WORKDIR ${NS3_DIR}
RUN ./ns3 configure \
    --build-profile=debug \
    --enable-examples \
    --enable-tests -- \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build ns3
RUN ./ns3 build -j$(nproc)

# Add ns3 to PATH for easy access
ENV PATH="${NS3_DIR}:${PATH}"

# Add project scripts to PATH for global access
ENV PATH="/workspaces/ns3-project/scripts:${PATH}"

# Copy project files and run sync script
COPY . /workspaces/ns3-project/
RUN ls -lah /workspaces/ns3-project/ \
    && chmod +x /workspaces/ns3-project/scripts/sync.sh \
    && /workspaces/ns3-project/scripts/sync.sh --to-ns3

# Set working directory to ns3 root
WORKDIR ${NS3_DIR}

RUN ln -s /workspaces/ns3-project /ns-3/.project

RUN apt-get install -y python3-pip
RUN pip3 install matplotlib

# Default command
CMD ["/bin/bash"]
