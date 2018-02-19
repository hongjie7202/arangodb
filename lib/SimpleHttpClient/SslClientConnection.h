////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_SSL_CLIENT_CONNECTION_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_SSL_CLIENT_CONNECTION_H 1

#include "Basics/Common.h"

#include "Basics/socket-utils.h"
#include "SimpleHttpClient/GeneralClientConnection.h"

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

namespace arangodb {
namespace httpclient {

////////////////////////////////////////////////////////////////////////////////
/// @brief client connection
////////////////////////////////////////////////////////////////////////////////

class SslClientConnection final : public GeneralClientConnection {
 private:
  SslClientConnection(SslClientConnection const&);
  SslClientConnection& operator=(SslClientConnection const&);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new client connection
  //////////////////////////////////////////////////////////////////////////////

  SslClientConnection(Endpoint* endpoint, double, double, size_t, uint64_t);

  SslClientConnection(std::unique_ptr<Endpoint>& endpoint, double, double,
                      size_t, uint64_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroys a client connection
  //////////////////////////////////////////////////////////////////////////////

  ~SslClientConnection();

 protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal initialization method, called from ctor
  //////////////////////////////////////////////////////////////////////////////

  void init(uint64_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief connect
  //////////////////////////////////////////////////////////////////////////////

  bool connectSocket() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief disconnect
  //////////////////////////////////////////////////////////////////////////////

  void disconnectSocket() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prepare connection for read/write I/O
  //////////////////////////////////////////////////////////////////////////////

  bool prepare(double, bool) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief write data to the connection
  //////////////////////////////////////////////////////////////////////////////

  bool writeClientConnection(void const*, size_t, size_t*) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief read data from the connection
  //////////////////////////////////////////////////////////////////////////////

  bool readClientConnection(arangodb::basics::StringBuffer&,
                            bool& connectionClosed) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return whether the connection is readable
  //////////////////////////////////////////////////////////////////////////////

  bool readable() override;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return whether the socket is still workable
  //////////////////////////////////////////////////////////////////////////////

  bool checkSocket();

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying socket
  //////////////////////////////////////////////////////////////////////////////

  TRI_socket_t _socket;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying session
  //////////////////////////////////////////////////////////////////////////////

  SSL* _ssl;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief SSL context
  //////////////////////////////////////////////////////////////////////////////

  SSL_CTX* _ctx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief SSL version
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _sslProtocol;
};
}
}

#endif
