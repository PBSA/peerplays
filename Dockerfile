FROM ubuntu:18.04
MAINTAINER PeerPlays Blockchain Standards Association

ENV LANG=en_US.UTF-8
ENV DEBIAN_FRONTEND=noninteractive
RUN \
    apt-get update -y && \
    apt-get install -y \
      autoconf \
      build-essential \
      ca-certificates \
      cmake \
      doxygen \
      git \
      libbz2-dev \
      libcurl4-openssl-dev \
      libncurses-dev \
      libreadline-dev \
      libssl-dev \
      libtool \
      pkg-config \
      ntp \
      wget \
    && \
    apt-get update -y && \
    apt-get install -y fish && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ADD . /peerplays-core
WORKDIR /peerplays-core

# Compile Boost
RUN \
    git submodule update --init --recursive && \
    BOOST_ROOT=$HOME/boost_1_67_0 && \
    wget -c 'http://sourceforge.net/projects/boost/files/boost/1.67.0/boost_1_67_0.tar.gz/download' -O boost_1_67_0.tar.gz &&\
    tar -zxvf boost_1_67_0.tar.gz && \
    cd boost_1_67_0/ && \
    ./bootstrap.sh "--prefix=$BOOST_ROOT" && \
    ./b2 install && \
    cd ..

# Compile Boost
RUN \
    BOOST_ROOT=$HOME/boost_1_67_0 && \
    mkdir build && \
    mkdir build/release && \
    cd build/release && \
    cmake \
        -DBOOST_ROOT="$BOOST_ROOT" \
        -DCMAKE_BUILD_TYPE=Release \
        ../.. && \
    make witness_node cli_wallet && \
    install -s programs/witness_node/witness_node programs/cli_wallet/cli_wallet /usr/local/bin && \
    #
    # Obtain version
    mkdir /etc/peerplays && \
    git rev-parse --short HEAD > /etc/peerplays/version && \
    cd / && \
    rm -rf /peerplays-core

# Home directory $HOME
WORKDIR /
RUN useradd -s /bin/bash -m -d /var/lib/peerplays peerplays
ENV HOME /var/lib/peerplays
RUN chown peerplays:peerplays -R /var/lib/peerplays

# Volume
VOLUME ["/var/lib/peerplays", "/etc/peerplays"]

# rpc service:
EXPOSE 8090
# p2p service:
EXPOSE 1776

# default exec/config files
ADD docker/default_config.ini /etc/peerplays/config.ini
ADD docker/peerplaysentry.sh /usr/local/bin/peerplaysentry.sh
RUN chmod a+x /usr/local/bin/peerplaysentry.sh

# Make Docker send SIGINT instead of SIGTERM to the daemon
STOPSIGNAL SIGINT

# default execute entry
CMD ["/usr/local/bin/peerplaysentry.sh"]
