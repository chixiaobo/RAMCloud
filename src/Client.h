/* Copyright (c) 2010 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef RAMCLOUD_CLIENT_H
#define RAMCLOUD_CLIENT_H

#include "Buffer.h"
#include "ClientException.h"
#include "Common.h"
#include "Mark.h"
#include "PerfCounterType.h"
#include "Rpc.h"
#include "Status.h"
#include "Transport.h"

namespace RAMCloud {

/**
 * A base class for RPC clients.
 */
class Client {
  public:
    Client() : status(STATUS_OK), counterValue(0), perfCounter() {}
    virtual ~Client() {}

    /**
     * Cancel any performance counter request previously specified by a call to
     * selectPerfCounter.
     */
    void clearPerfCounter() {
        selectPerfCounter(PERF_COUNTER_INC, MARK_NONE, MARK_NONE);
    }

    /**
     * Arrange for a performance metric to be collected by the server
     * during each future RPC. The value of the metric can be read from
     * the "counterValue" variable after each RPC.
     *
     * \param type
     *      Specifies what to measure (elapsed time, cache misses, etc.)
     * \param begin
     *      Indicates a point during the RPC when measurement should start.
     * \param end
     *      Indicates a point during the RPC when measurement should stop.
     */
    void selectPerfCounter(PerfCounterType type, Mark begin, Mark end) {
        perfCounter.counterType = type;
        perfCounter.beginMark = begin;
        perfCounter.endMark = end;
    }

    /**
     * Completion status from the most recent RPC completed for this client.
     */
    Status status;

    /**
     * Performance metric from the response in the most recent RPC (as
     * requested by selectPerfCounter). If no metric was requested and done
     * most recent RPC, then this value is 0.
     */
    uint32_t counterValue;

  protected:

    // begin RPC proxy method helpers

    /**
     * Helper for RPC proxy methods that prepares an RPC request header.
     * You should call this before every use of #sendRecv().
     */
    template <typename Rpc>
    typename Rpc::Request&
    allocHeader(Buffer& requestBuffer) const
    {
        assert(requestBuffer.getTotalLength() == 0);
        typename Rpc::Request& requestHeader(
            *new(&requestBuffer, APPEND) typename Rpc::Request);
        memset(&requestHeader, 0, sizeof(requestHeader));
        requestHeader.common.type = Rpc::type;
        requestHeader.common.perfCounter = perfCounter;
        return requestHeader;
    }

    /**
     * Helper for RPC proxy methods that sends a request and receive a
     * response. You should call #allocHeader() before this and #checkStatus()
     * after this.
     */
    template <typename Rpc>
    const typename Rpc::Response&
    sendRecv(Transport::SessionRef& session,
             Buffer& requestBuffer, Buffer& responseBuffer)
    {
        assert(responseBuffer.getTotalLength() == 0);
        session->clientSend(&requestBuffer, &responseBuffer)->getReply();
        const typename Rpc::Response* responseHeader =
            responseBuffer.getStart<typename Rpc::Response>();
        if (responseHeader == NULL)
            throwShortResponseError(responseBuffer);
        status = responseHeader->common.status;
        counterValue = responseHeader->common.counterValue;
        return *responseHeader;
    }

    /**
     * Helper for RPC proxy methods that throws a ClientException if the status
     * is not OK. You should call this after every use of #sendRecv().
     */
    void checkStatus() const
    {
        TEST_LOG("status: %d", STATUS_OK);
        if (status != STATUS_OK)
            ClientException::throwException(HERE, status);
    }

    // end RPC proxy method helpers

    void throwShortResponseError(Buffer& response)
        __attribute__((noreturn));

    /**
     * Every RPC request will ask the server to measure this during the
     * execution of the RPC.
     */
    RpcPerfCounter perfCounter;

  private:
    friend class ClientTest;
    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // end RAMCloud

#endif  // RAMCLOUD_CLIENT_H