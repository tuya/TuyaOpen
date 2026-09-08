FROM ubuntu:latest

WORKDIR /app

# L511G fcelf is a 32-bit dynamically linked C++ executable.
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
    wget git lcov cmake-curses-gui build-essential ninja-build \
    python3 python3-pip python3-venv \
    libc6-i386 libstdc++6:i386 libsystemd-dev locales \
    libusb-1.0-0 libusb-1.0-0-dev && \
    locale-gen en_US.UTF-8 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ENV LANG=en_US.UTF-8

RUN mv /usr/lib/python3.12/EXTERNALLY-MANAGED /usr/lib/python3.12/EXTERNALLY-MANAGED.bak || true

RUN useradd -m -s /bin/bash builder