# syntax=docker/dockerfile:1

FROM node:22-bookworm-slim AS frontend
WORKDIR /app/web
COPY web/package.json web/package-lock.json ./
RUN npm ci
COPY web/ ./
COPY docs/openapi.yaml /app/docs/openapi.yaml
RUN npm run build

FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    libopencv-dev \
    libsqlite3-dev \
    libleptonica-dev \
    libtesseract-dev \
    pkg-config \
    qt6-base-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
COPY --from=frontend /app/web/dist ./web/dist
RUN cmake -S . -B /tmp/receiptreader-build \
      -DRECEIPTREADER_BUILD_DESKTOP=OFF \
      -DRECEIPTREADER_BUILD_TESTS=OFF \
      -DRECEIPTREADER_BUILD_WEB=OFF \
 && cmake --build /tmp/receiptreader-build --target ReceiptReaderServer --parallel

FROM ubuntu:24.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive \
    RECEIPTREADER_PORT=53000
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    imagemagick \
    libopencv-core406t64 \
    libopencv-imgcodecs406t64 \
    libopencv-imgproc406t64 \
    libqt6core6t64 \
    libsqlite3-0 \
    liblept5 \
    libtesseract5 \
    tesseract-ocr-eng \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --uid 10001 --create-home --home-dir /nonexistent receiptreader \
 && mkdir -p /app/web/dist /data \
 && chown receiptreader:receiptreader /data

WORKDIR /tmp
COPY --from=build /tmp/receiptreader-build/ReceiptReaderServer /usr/local/bin/ReceiptReaderServer
COPY --from=frontend /app/web/dist /app/web/dist

VOLUME ["/data"]
EXPOSE 53000
HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD curl --fail --silent http://127.0.0.1:${RECEIPTREADER_PORT}/api/v1/health || exit 1

USER receiptreader
CMD ["/bin/sh", "-c", "exec /usr/local/bin/ReceiptReaderServer --port \"${RECEIPTREADER_PORT}\" --db /data/receipts.db --web-root /app/web/dist"]
