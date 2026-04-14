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


# Install vim
RUN apt-get install -y vim

# Install github copilot cli
RUN curl -fsSL https://gh.io/copilot-install | bash

# Create non-root user (important for GUI apps)
RUN useradd -m devuser
USER devuser
WORKDIR /home/devuser

# Make sure the .ssh directory exists for the user
RUN mkdir -p /home/devuser/.ssh
