#include "src/transports/tcp/TcpConnectionAcceptor.h"
#include "src/transports/tcp/TcpConnectionFactory.h"

#include "test/DuplexConnectionTest.h"

namespace rsocket {
namespace tests {
namespace duplexconnection {

TEST(TcpDuplexConnection, MultipleSetInputOutputCalls) {
  MultipleSetInputOutputCallsTest<
      TcpConnectionAcceptor,
      TcpConnectionFactory>();
}

} // namespace duplexconnection
} // namespace tests
} // namespace rsocket