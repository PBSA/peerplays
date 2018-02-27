FROM phusion/baseimage:0.9.19
MAINTAINER PeerPlays Blockchain Standards Association

ENV LANG=en_US.UTF-8
RUN \
    apt-get update -y && \
    apt-get install -y \
      g++ \
      autoconf \
      cmake \
      git \
      libbz2-dev \
      libreadline-dev \
      libboost-all-dev \
      libcurl4-openssl-dev \
      libssl-dev \
      libncurses-dev \
      doxygen \
      libcurl4-openssl-dev \
    && \
    apt-get update -y && \
    apt-get install -y fish && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ADD . /peerplays-core
WORKDIR /peerplays-core

# Compile
RUN \
    git submodule update --init --recursive && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        . && \
    make witness_node && \
    make install && \
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
EXPOSE 2001

# default exec/config files
ADD docker/default_config.ini /etc/peerplays/config.ini
ADD docker/peerplaysentry.sh /usr/local/bin/peerplaysentry.sh
RUN chmod a+x /usr/local/bin/peerplaysentry.sh

# default execute entry
CMD /usr/local/bin/peerplaysentry.sh
