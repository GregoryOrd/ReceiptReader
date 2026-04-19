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
    libglu1-mesa-dev \
    libopencv-dev \
    tesseract-ocr \
    libtesseract-dev \
    libleptonica-dev \
    qt6-base-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    qt6-qpa-plugins \
    qt6-charts-dev \
    protobuf-compiler \
    libgtest-dev \
    libgmock-dev \
    sqlite3 \
    libsqlite3-dev \
    vim

# Install GitHub CLI
RUN mkdir -p -m 755 /etc/apt/keyrings
RUN out=$(mktemp) \
    && wget -nv -O$out https://cli.github.com/packages/githubcli-archive-keyring.gpg \
    && cat $out | tee /etc/apt/keyrings/githubcli-archive-keyring.gpg > /dev/null
RUN chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg
RUN mkdir -p -m 755 /etc/apt/sources.list.d
RUN echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | tee /etc/apt/sources.list.d/github-cli.list > /dev/null
RUN apt update && apt install gh -y

# Install github copilot cli
RUN curl -fsSL https://gh.io/copilot-install | bash

# Create non-root user (important for GUI apps)
RUN useradd -m devuser
USER devuser
WORKDIR /home/devuser

# Make sure the .ssh directory exists for the user
RUN mkdir -p /home/devuser/.ssh
