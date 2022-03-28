FROM ubuntu:focal

RUN apt-get update && apt-get install -y \
  build-essential \
  libcjson-dev \
  libcurl4-openssl-dev \
  libsqlite3-dev \
  && rm -rf /var/lib/apt/lists/*

COPY . /nhl
WORKDIR /nhl
RUN make clean && make debug
RUN ln -s /nhl/src/nhl /usr/local/bin/nhl
