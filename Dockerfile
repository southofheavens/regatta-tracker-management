FROM alpine:3.23.3

RUN apk add --no-cache \
    openssl-dev \
    pkgconfig \
    git \
    build-base \
    meson \
    poco-dev \
    libsodium-dev \
    gtest-dev \
    aws-sdk-cpp-dev \
    aws-crt-cpp-dev 

COPY . ./app

WORKDIR /app

RUN meson build
RUN meson compile -C build -j 3

CMD ["./build/rgt-management"]
