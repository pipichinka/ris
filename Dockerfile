FROM ghcr.io/userver-framework/ubuntu-22.04-userver-pg:latest AS builder
WORKDIR /build
COPY . .
RUN mkdir build
RUN apt-get install -y clang-format
RUN apt-get install -y libmongoc-dev
RUN cmake --preset release -B build -S .
RUN cmake --build build -j 8

FROM ghcr.io/userver-framework/ubuntu-22.04-userver-pg:latest AS runner
RUN apt-get install -y clang-format
RUN apt-get install -y libmongoc-dev
RUN groupadd -r sample && useradd -r -g sample sample
USER sample
VOLUME /ris
WORKDIR /ris
COPY --from=builder /build/build/worker_service .
COPY --from=builder /build/build/manager_service .
COPY configs/* .