if [ "$#" -ne 4 ]; then
    echo "Illegal number of parameters - $#"
    exit 1
fi

while getopts ":c:s:" opt; do
  case $opt in
    c)
      client_lang="$OPTARG"
    ;;
    s)
      server_lang="$OPTARG"
    ;;
    \?) echo "Invalid option -$OPTARG" >&2
      exit 1
    ;;
  esac
done

if [[ "$client_lang" =~ ^(cpp|java)$ ]]; then
  echo "Valid client language: $client_lang"
else
  echo "Invalid client language: $client_lang"
  exit 1
fi

if [[ "$server_lang" =~ ^(cpp|java)$ ]]; then
  echo "Valid server language: $server_lang"
else
  echo "Invalid server language: $server_lang"
  exit 1
fi

if [ ! -s ./build/tckserver ] && [ "$server_lang" = cpp ]; then
    echo "./build/tckserver Binary not found!"
    exit 1
fi

if [ ! -s ./build/tckclient ] && [ "$client_lang" = cpp ]; then
    echo "./build/tckclient Binary not found!"
    exit 1
fi

java_server="java -cp reactivesocket-tck-drivers-0.9-SNAPSHOT.jar io/reactivesocket/tckdrivers/main/Main --server --host localhost --port 9898 --file tck-test/servertest.txt"
java_client="java -cp reactivesocket-tck-drivers-0.9-SNAPSHOT.jar io/reactivesocket/tckdrivers/main/Main --client --host localhost --port 9898 --file tck-test/clienttest.txt"

cpp_server="./build/tckserver -test_file tck-test/servertest.txt -rs_use_protocol_version 1.0"
cpp_client="./build/tckclient -test_file tck-test/clienttest.txt -rs_use_protocol_version 1.0"

server="${server_lang}_server"
client="${client_lang}_client"

# run server in the background
timeout 60 ${!server} &

# wait for the server to listen
sleep 2

# run client
timeout 60 ${!client}
ret=$?

# terminate server
kill $!

# wait for server to relinquish its socket resources
sleep 2

exit $ret
