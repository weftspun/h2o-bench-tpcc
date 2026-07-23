FROM buildpack-deps:26.04 AS compile

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get install --no-install-recommends -qqUy \
      cmake gcc libpq-dev libssl-dev libyajl-dev libz-dev make pkg-config

RUN echo "[timing] Building H2O: $(date)"
ARG H2O_VERSION=ccea64b17ade832753db933658047ede9f31a380
WORKDIR /tmp/h2o-build
RUN curl -LSs "https://github.com/h2o/h2o/archive/${H2O_VERSION}.tar.gz" | \
      tar --strip-components=1 -xz && \
    cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-flto=auto -march=native -mtune=native" \
      -DWITH_MRUBY=off -S . && \
    cmake --build build -j && cmake --install build

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
RUN apt-get install --no-install-recommends -qqUy libpq5 libyajl2 libssl3 && \
    rm -rf /var/lib/apt/lists/*
COPY --from=compile /opt/h2o-bench-tpcc /opt/h2o-bench-tpcc/
ENV LD_LIBRARY_PATH=/usr/local/lib
EXPOSE 8080

CMD ["taskset", "-c", "0", "/opt/h2o-bench-tpcc/bin/h2o-bench-tpcc", \
     "-a20", \
     "-d", "dbname=tpcc host=tfb-database port=26257 sslmode=disable user=benchmarkdbuser", \
     "-w1"]
