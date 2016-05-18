# fetch folly lib and include for Ubuntu 14.04 Trusty
git config --global user.name "ReactiveSocket"
git config --global user.email "email@address.com"
git config --global push.default simple
git clone https://bitbucket.org/reactivesocket/reactivesocket-cpp-dependencies.git
cd reactivesocket-cpp-dependencies
cd lib
echo "Copy folly libs to /usr/local/lib"
sudo cp -R *folly* /usr/local/lib/
cd .. # out of lib
cd include
echo "Copy folly includes to /usr/local/include/folly"
sudo cp -R folly /usr/local/include/
cd .. # out of include
cd .. # back to top
echo "Done fetching folly."


