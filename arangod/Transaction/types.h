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

#ifndef ARANGOD_TRANSACTION_TYPES_H
#define ARANGOD_TRANSACTION_TYPES_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace transaction {

/// @brief type of a transaction id
struct TransactionId {
  TransactionId(uint64_t c = 0, uint64_t i = 0) : coordinator(c), identifier(i) {}
  uint64_t coordinator;
  uint64_t identifier;
};

inline bool operator==(TransactionId const& lhs, TransactionId const& rhs) {
  return (lhs.coordinator==rhs.coordinator && lhs.identifier==rhs.identifier);
}

}}

namespace std {
template<> struct hash<arangodb::transaction::TransactionId> {
  //Cantor pairing function
  inline size_t operator()(arangodb::transaction::TransactionId const& t) const {
    return 0.5*(t.coordinator+t.identifier)*(t.coordinator+t.identifier+1)+t.identifier;
  }
};
}

#endif

