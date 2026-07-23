FROM buildpack-deps:26.04 AS compile

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get install --no-install-recommends -qqUy \
      cmake gcc libssl-dev libyajl-dev libz-dev make pkg-config

# Build H2O
RUN echo "[timing] Building H2O: $(date)"
ARG H2O_VERSION=ccea64b17ade832753db933658047ede9f31a380
WORKDIR /tmp/h2o-build
RUN curl -LSs "https://github.com/h2o/h2o/archive/${H2O_VERSION}.tar.gz" | \
      tar --strip-components=1 -xz && \
    cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-flto=auto -march=native -mtune=native" \
      -DWITH_MRUBY=off -S . && \
    cmake --build build -j && cmake --install build

# Install FoundationDB C client
RUN echo "[timing] Installing FDB C client: $(date)"
ARG FDB_VERSION=7.3.79
RUN curl -LSs "https://github.com/apple/foundationdb/releases/download/${FDB_VERSION}/foundationdb-clients_${FDB_VERSION}-1_amd64.deb" -o /tmp/fdb-client.deb && \
    dpkg -i /tmp/fdb-client.deb && rm /tmp/fdb-client.deb

# Build h2o-bench-tpcc
RUN echo "[timing] Building h2o-bench-tpcc: $(date)"
WORKDIR /tmp/build
COPY CMakeLists.txt ../
COPY src ../src/
COPY sql ../sql/
RUN cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-march=native -mtune=native" \
      -DCMAKE_INSTALL_PREFIX=/opt/h2o-bench-tpcc -S .. && \
    cmake --build . -j && cmake --install .

FROM ubuntu:26.04
ARG DEBIAN_FRONTEND=noninteractive

# Install FDB C client runtime
ARG FDB_VERSION=7.3.79
RUN apt-get update -qq && \
    apt-get install --no-install-recommends -qqUy libssl3 libyajl2 && \
    curl -LSs "https://github.com/apple/foundationdb/releases/download/${FDB_VERSION}/foundationdb-clients_${FDB_VERSION}-1_amd64.deb" -o /tmp/fdb-client.deb && \
    dpkg -i /tmp/fdb-client.deb && rm /tmp/fdb-client.deb && \
    rm -rf /var/lib/apt/lists/*

COPY --from=compile /opt/h2o-bench-tpcc /opt/h2o-bench-tpcc/
ENV LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu
EXPOSE 8080

CMD ["taskset", "-c", "0", "/opt/h2o-bench-tpcc/bin/h2o-bench-tpcc", \
     "-a20", \
     "-c", "/etc/foundationdb/fdb.cluster", \
     "-w1"]
