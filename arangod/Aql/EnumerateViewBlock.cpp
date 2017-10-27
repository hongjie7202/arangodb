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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "EnumerateViewBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

namespace {

template<typename T>
inline T const& getPlanNode(arangodb::aql::ExecutionBlock const& block) noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return dynamic_cast<T const&>(*block.getPlanNode());
#else
  return static_cast<T const&>(*block.getPlanNode());
#endif
}

class ViewExpressionContext final : public arangodb::aql::ExpressionContext {
 public:
  ViewExpressionContext(
      arangodb::aql::ExecutionBlock* block,
      arangodb::aql::AqlItemBlock const* data,
      size_t pos) noexcept
    : _data(data), _block(block), _pos(pos) {
    TRI_ASSERT(block && data);
  }

  virtual size_t numRegisters() const override {
    return _data->getNrRegs();
  }

  virtual arangodb::aql::AqlValue const& getRegisterValue(size_t i) const {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual arangodb::aql::Variable const* getVariable(size_t i) const {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual arangodb::aql::AqlValue getVariableValue(
      arangodb::aql::Variable const* variable,
      bool doCopy,
      bool& mustDestroy) const {
    mustDestroy = false;
    auto const reg = _block->getRegister(variable);

    if (reg == arangodb::aql::ExecutionNode::MaxRegisterId) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Internal error"); // FIXME
      return arangodb::aql::AqlValue();
    }

    auto& value = _data->getValueReference(_pos, reg);

    if (doCopy) {
      mustDestroy = true;
      return value.clone();
    }

    return value;
  }

  arangodb::aql::AqlItemBlock const* _data;
  arangodb::aql::ExecutionBlock* _block;
  size_t _pos{};
}; // ViewExecutionContext

}

// FIXME:
//  - LRU for prepared queries
//  - detect cases whithout dependecies

using namespace arangodb;
using namespace arangodb::aql;

EnumerateViewBlock::EnumerateViewBlock(
    ExecutionEngine* engine,
    EnumerateViewNode const* en)
  : ExecutionBlock(engine, en),
    _iter(nullptr),
    _mmdr(new ManagedDocumentResult), // TODO
    _hasMore(true) { // has more data initially
}

EnumerateViewBlock::~EnumerateViewBlock() {}

int EnumerateViewBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  const int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _hasMore = true; // has more data initially

  return TRI_ERROR_NO_ERROR;
  DEBUG_END_BLOCK();
}

void EnumerateViewBlock::refreshIterator() {
  TRI_ASSERT(!_buffer.empty());

  ViewExpressionContext ctx(this, _buffer.front(), _pos);
  _iter = ::getPlanNode<EnumerateViewNode>(*this).iterator(*_trx, ctx);

  if (!_iter) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL); // FIXME runtime error
  }
}

AqlItemBlock* EnumerateViewBlock::getSome(size_t, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin();

  if (_done) {
    traceGetSomeEnd(nullptr);
    return nullptr;
  }

  bool needMore;
  AqlItemBlock* cur = nullptr;
  size_t send = 0;
  std::unique_ptr<AqlItemBlock> res;
  auto& planNode = ::getPlanNode<EnumerateViewNode>(*this);

  do {
    do {
      needMore = false;

      if (_buffer.empty()) {
        size_t const toFetch = (std::min)(DefaultBatchSize(), atMost);
        if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
          _done = true;
          return nullptr;
        }
        _pos = 0;  // this is in the first block
        refreshIterator();
      }

      // If we get here, we do have _buffer.front()
      cur = _buffer.front();

      if (!_hasMore) {
        needMore = true;
        _hasMore = true;

        // we have exhausted this cursor
        // re-initialize fetching of documents
        refreshIterator();

        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          returnBlock(cur);
          _pos = 0;
        }
      }
    } while (needMore);

    TRI_ASSERT(cur != nullptr);

    size_t const curRegs = cur->getNrRegs();
    RegisterId const nrRegs = planNode.getRegisterPlan()->nrRegs[planNode.getDepth()];

    res.reset(requestBlock(atMost, nrRegs));
    // automatically freed if we throw
    TRI_ASSERT(curRegs <= res->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, res.get(), _pos);

    auto cb = [&](LocalDocumentId const& tkn) {
      if (_iter->readDocument(tkn, *_mmdr)) {
        // The result is in the first variable of this depth,
        // we do not need to do a lookup in
        // getPlanNode()->_registerPlan->varInfo,
        // but can just take cur->getNrRegs() as registerId:
        uint8_t const* vpack = _mmdr->vpack();
        if (_mmdr->canUseInExternal()) {
          res->setValue(send, static_cast<arangodb::aql::RegisterId>(curRegs),
                        AqlValue(AqlValueHintDocumentNoCopy(vpack)));
        } else {
          res->setValue(send, static_cast<arangodb::aql::RegisterId>(curRegs),
                        AqlValue(AqlValueHintCopy(vpack)));
        }
      }

      if (send > 0) {
        // re-use already copied AQLValues
        res->copyValuesFromFirstRow(send, static_cast<RegisterId>(curRegs));
      }
      ++send;
    };

    throwIfKilled();  // check if we were aborted

    TRI_IF_FAILURE("EnumerateViewBlock::moreDocuments") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    _hasMore = _iter->next(cb, atMost);

    // If the collection is actually empty we cannot forward an empty block
  } while (send == 0);

  // TODO stats?
  // _engine->_stats.scannedFull += static_cast<int64_t>(send);
  TRI_ASSERT(res != nullptr);

  if (send < atMost) {
    // The collection did not have enough results
    res->shrink(send);
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());

  traceGetSomeEnd(res.get());

  return res.release();

  DEBUG_END_BLOCK();
}

size_t EnumerateViewBlock::skipSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  size_t skipped = 0;
  TRI_ASSERT(_iter != nullptr);

  if (_done) {
    return skipped;
  }

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!getBlock(toFetch, toFetch)) {
        _done = true;
        return skipped;
      }
      _pos = 0;  // this is in the first block
      _iter->reset();
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();
    uint64_t skippedHere = 0;

    _iter->skip(atMost - skipped, skippedHere);

    skipped += skippedHere;

    if (skipped < atLeast) {
      // not skipped enough re-initialize fetching of documents
      _iter->reset();
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        returnBlock(cur);
        _pos = 0;
      }
    }
  }

  // _engine->_stats.scannedFull += static_cast<int64_t>(skipped);
  // TODO stats?
  // We skipped atLeast documents
  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}
