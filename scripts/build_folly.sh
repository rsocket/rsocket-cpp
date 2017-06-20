install_dir=$HOME/folly
if [ -d "$install_dir/include" ]; then
  echo "Using cache $install_dir"
  exit 0
fi

if [ -n "$OSX_LOCAL" ]; then
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
&& ./configure --prefix=$install_dir \
&& make -j 4 \
&& make install \\
)
EOF
`

fetch_and_unpack "googletest-release-$gtest_version.tar.gz" "https://github.com/google/googletest/archive/release-$gtest_version.tar.gz"
fetch_and_unpack "folly-$folly_version.tar.gz" "https://github.com/facebook/folly/archive/v$folly_version.tar.gz" "$folly_compile"
