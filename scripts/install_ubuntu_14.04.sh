echo "Running with TRAVIS_SECURE_ENV_VARS=$TRAVIS_SECURE_ENV_VARS"

# store binary on Github
git config --global user.name "Travis Git"
git config --global user.email "benjchristensen+travisgit@gmail.com"
git config --global push.default simple
echo "Username is: $GITHUB_USERNAME"
git clone https://$GITHUB_USERNAME:$GITHUB_PASSWORD@github.com/ReactiveSocket/reactivesocket-cpp-dependencies.git
cd reactivesocket-cpp-dependencies
cd lib
touch junk
git add junk
git commit -a -m "folly binary"
git push origin
cd ..
