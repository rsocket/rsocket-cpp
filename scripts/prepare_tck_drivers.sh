set -x

# add common dependencies
sudo apt-get -y timeout

# add java-driver dependencies
sudo add-apt-repository -y ppa:webupd8team/java
sudo apt-get -y install oracle-java8-installer

# download the latest java-driver .jar
wget https://oss.jfrog.org/libs-snapshot/io/reactivesocket/reactivesocket-tck-drivers/0.9-SNAPSHOT/reactivesocket-tck-drivers-0.9-SNAPSHOT.jar
