#@IgnoreInspection BashAddShebang
# libs required for Folly
sudo apt-get -y install \
    g++ \
    automake \
    autoconf \
    autoconf-archive \
    libtool \
    libboost-all-dev \
    libevent-dev \
    libdouble-conversion-dev \
    libgoogle-glog-dev \
    libgflags-dev \
    liblz4-dev \
    liblzma-dev \
    libsnappy-dev \
    make \
    zlib1g-dev \
    binutils-dev \
    libjemalloc-dev \
    libssl-dev \
    libiberty-dev

# install reactive-streams-cpp
mkdir working
cd working
git clone https://github.com/ReactiveSocket/reactive-streams-cpp
cd reactive-streams-cpp
git submodule init
git submodule update --recursive
mkdir build
cd build
cmake ..
make
sudo make install