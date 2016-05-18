### Build folly and upload libs/includes to Bitbucket
# Note that USERNAME:PASSWORD placeholders below need to be edited

# prerequisites for folly
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
    libssl-dev
    
sudo apt-get -y install \
    libiberty-dev
    
# build and install folly
git clone https://github.com/facebook/folly.git
cd folly/folly
    
autoreconf -ivf
  ./configure
  make
  make check
  sudo make install

cd ../..

# store binary on Bitbucket (files too large for Github)
git config --global user.name "ReactiveSocket"
git config --global user.email "email@host.com"
git config --global push.default simple
git clone https://USERNAME:PASSWORD@bitbucket.org/reactivesocket/reactivesocket-cpp-dependencies.git
cd reactivesocket-cpp-dependencies
cd lib
cp /usr/local/lib/*folly* .
git add *
cd ..
cd include
cp -R /usr/local/include/folly .
git add *
cd ..
git commit -a -m "folly binary"
git push origin
cd ..
