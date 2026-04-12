FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Base tools
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    git \
    make \
    cmake \
    pkg-config \
    wget \
    curl \
    unzip \
    ca-certificates \
    gnupg \
    lsb-release \
    x11-apps \
    libx11-dev \
    libxext-dev \
    libxrender-dev \
    libxcb1-dev \
    libxkbcommon-x11-0 \
    libgl1-mesa-dev \
    libglu1-mesa-dev

# OpenCV
RUN apt-get install -y libopencv-dev

# Tesseract
RUN apt-get install -y \
    tesseract-ocr \
    libtesseract-dev \
    libleptonica-dev

# Qt6 (GUI support)
RUN apt-get install -y \
    qt6-base-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    qt6-qpa-plugins

# Create non-root user (important for GUI apps)
RUN useradd -m devuser
USER devuser
WORKDIR /home/devuser

# Git fail with error because the user in the container does not match the owner of the mounted directory. This is a workaround to allow git to work in the mounted directory.
# The error will be something like:
# fatal: detected dubious ownership in repository at '/home/devuser/ReceiptReader'
# Tell git to mark the repo as a safe directory
RUN git config --global --add safe.directory /home/devuser/ReceiptReader

