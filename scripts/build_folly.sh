if [ -z "$OSX_LOCAL" ]; then
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
else
  export CPPFLAGS="-I/usr/local/opt/openssl/include"
  export LDFLAGS="-L/usr/local/opt/openssl/lib"
fi

folly_version="2017.03.06.00"
gtest_version="1.8.0"

cachedir="$HOME/.rscache"
destination="build/third-party"

mkdir -p "$cachedir"
mkdir -p "$destination"

function fetch_and_unpack () {
    file=$1
    url=$2
    cmd=$3

    if [ ! -f "$cachedir/$file" ]; then
        (cd "$cachedir"; curl -J -L -O "$url")
    fi

    dir=$(basename "$file" .tar.gz)
    if [ ! -d "$destination/$dir" ]; then
        (cd $destination;
         echo Unpacking "$cachedir/$file"...
         tar zxf "$cachedir/$file"

         cd "$dir"
         echo "Running $cmd"
         eval "${cmd:-true}")
    fi
}

folly_compile=`cat <<EOF
( \
mv ../*googletest* folly/test/gtest \
&& cd folly \
&& autoreconf -ivf \
&& ./configure \
&& make -j 4 \
&& sudo make install \\
)
EOF
`

fetch_and_unpack "googletest-release-$gtest_version.tar.gz" "https://github.com/google/googletest/archive/release-$gtest_version.tar.gz"
fetch_and_unpack "folly-$folly_version.tar.gz" "https://github.com/facebook/folly/archive/v$folly_version.tar.gz" "$folly_compile"
