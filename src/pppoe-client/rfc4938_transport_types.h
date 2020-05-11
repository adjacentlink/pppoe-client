/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_transport_types.h
 * version: 1.0
 * date: Oct 21, 2013
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 *
 * ===========================
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *----------------------------------------------------------------------------*/


#ifndef RFC4938_TRANSPORT_TYPES_H
#define RFC4938_TRANSPORT_TYPES_H


#include "emane/transport.h"
#include "emane/flowcontrolclient.h"

#include "emane/utils/randomnumberdistribution.h"

#include "emane/controls/r2rineighbormetriccontrolmessage.h"
#include "emane/controls/r2riqueuemetriccontrolmessage.h"
#include "emane/controls/r2riselfmetriccontrolmessage.h"

#include <set>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <cstdint>


struct NeighborStats
{
    float             fSINRlast_;           // last sinr value
    float             fRRlast_;             // last recv ratio
    std::uint64_t     u64LastTxDataRate_;   // last tx data rate
    EMANE::TimePoint  tp_;                  // creation time
    std::uint16_t     u16PendingCredits_;   // pending credits

    NeighborStats () :
        fSINRlast_{-256.0},
        fRRlast_{},
        u64LastTxDataRate_{},
        tp_{EMANE::Clock::now()},
        u16PendingCredits_{}
    { }
};

struct PADGParams
{
    std::uint32_t nbrId_;
    std::uint16_t credits_;

    PADGParams (std::uint32_t nbrId,
                std::uint16_t credits) :
        nbrId_(nbrId),
        credits_(credits)
    { }
};

struct PADIParams
{
    std::uint32_t nbrId_;
    std::uint16_t scalar_;

    PADIParams (std::uint32_t nbrId,
                std::uint16_t scalar) :
        nbrId_(nbrId),
        scalar_(scalar)
    { }
};

struct PADQParams
{
    std::uint32_t nbrId_;
    UINT8_t    ro_;
    int        rlq_;
    int        res_;
    std::uint32_t   qdelay_;
    std::uint16_t   cdrs_;
    std::uint16_t   cdr_;
    std::uint16_t   mdrs_;
    std::uint16_t   mdr_;

    PADQParams (std::uint32_t nbrId,
                UINT8_t ro,
                int rlq,
                int res,
                std::uint32_t qdelay,
                std::uint16_t cdrs,
                std::uint16_t cdr,
                std::uint16_t mdrs,
                std::uint16_t mdr) :
        nbrId_(nbrId),
        ro_(ro),
        rlq_(rlq),
        res_(res),
        qdelay_(qdelay),
        cdrs_(cdrs),
        cdr_(cdr),
        mdrs_(mdrs),
        mdr_(mdr)
    { }
};

struct FDParams
{
    fd_set fdready_;
    int num_;

    FDParams (fd_set fdready, int num) :
        fdready_(fdready),
        num_(num)
    { }
};

struct DSPktParams
{
    EMANE::DownstreamPacket * pkt_;
    int credits_;

    DSPktParams (EMANE::DownstreamPacket * pkt, int credits) :
        pkt_(pkt),
        credits_(credits)
    { }
};



struct WorkItem
{
    unsigned long id_;
    const void * data_;

    WorkItem(unsigned long id, const void * data) :
        id_{id},
        data_{data}
    { }

    WorkItem() :
        id_{},
        data_{}
    { }
};


template <typename T>
class WorkerQueue
{
public:
    WorkerQueue() :
        bCancel_(false)
    { }

    virtual ~WorkerQueue() { }

    T dequeue()
    {
        std::unique_lock<std::mutex> m(qmutex_);

        while(queue_.empty() && !bCancel_)
        {
            cond_.wait(m);
        }

        if (bCancel_)
        {
            return T();
        }

        T item = queue_.front();

        queue_.pop();

        return item;
    }

    void enqueue(T &item)
    {
        std::lock_guard<std::mutex> m(qmutex_);

        queue_.push(item);

        cond_.notify_one();
    }

    void cancel()
    {
        std::lock_guard<std::mutex> m(qmutex_);

        bCancel_ = true;

        cond_.notify_one();
    }

    size_t size()
    {
        std::lock_guard<std::mutex> m(qmutex_);

        return queue_.size();
    }

private:
    bool bCancel_;

    std::queue<T> queue_;

    std::mutex qmutex_;

    std::condition_variable cond_;
};


typedef std::map<std::uint16_t, NeighborStats>  NeighborStatsMap;

typedef std::set<std::uint16_t>  NEMIdSet;

struct MetricUpdate
{
    EMANE::Controls::R2RINeighborMetricControlMessage * pNbrMetric_;
    EMANE::Controls::R2RIQueueMetricControlMessage    * pQueueMetric_;
    EMANE::Controls::R2RISelfMetricControlMessage     * pSelfMetric_;

    MetricUpdate() :
        pNbrMetric_{},
        pQueueMetric_{},
        pSelfMetric_{}
    { }

    bool isReady()
    {
        return (pNbrMetric_ && pSelfMetric_ && pQueueMetric_);
    }
};

class PPPoETransport : public EMANE::Transport
{
public:
    PPPoETransport (EMANE::NEMId id, EMANE::PlatformServiceProvider * p);

    ~PPPoETransport();

    void initialize(EMANE::Registrar & registrar) override;

    void configure(const EMANE::ConfigurationUpdate & update) override;

    void start() override;

    void postStart() override;

    void stop() override;

    void destroy() throw() override;

    void processUpstreamPacket(EMANE::UpstreamPacket & pkt, const EMANE::ControlMessages & msgs);

    void processUpstreamControl(const EMANE::ControlMessages & msgs);

    void processTimedEvent(EMANE::TimerEventId eventId,
                           const EMANE::TimePoint & expireTime,
                           const EMANE::TimePoint & scheduleTime,
                           const EMANE::TimePoint & fireTime,
                           const void * arg) override;

    void enqueueTransportData(std::uint16_t dst, std::uint16_t credits, const void * p2buffer, int buflen);

    void enableHellos(float fInterval);

    void disableHellos();

    void deleteNeighbor(std::uint16_t nbr);

    void notifyFdReady(fd_set fdready, int num);

private:
    typedef std::pair<std::uint16_t, std::uint16_t> DataRateScaledPair;

    long helloTimedEventId_;

    NEMIdSet cachedNeighbors_;

    NeighborStatsMap neighborStatistics_;

    EMANE::FlowControlClient flowControlClient_;

    std::uint64_t u64MaxSysDataRate_;

    std::thread workQueueThread_;

    WorkerQueue<WorkItem> workQueue_;

    bool bCanceled_;

    EMANE::Microseconds helloInterval_;

    EMANE::TimePoint lastCreditReportTime_;

    void processControl_i(const EMANE::ControlMessages & msgs);

    void handleMetricMessage_i(const MetricUpdate * pMsg);

    void handleTokenUpdate_i();

    DataRateScaledPair getDataRateScaleFactor_i(std::uint64_t u64DataRatebps);

    int getResources_i(const EMANE::DoubleSeconds & totalBandWidthConsumption,
                       const EMANE::DoubleSeconds & interval);

    int getRLQ_i(const std::uint16_t nbr, const float fSINRAvg, const std::uint32_t numRxFrames,
                 const std::uint32_t numMissedFrames);

    void updateNeighborCacheAndStats_i(NEMIdSet & latest);

    void sendDownstream_i(EMANE::DownstreamPacket * p, std::uint16_t credits);

    void enqueueWorkItem_i(WorkItem & item);

    void * processWorkQueue_i();

    EMANE::Utils::RandomNumberDistribution<std::mt19937,
          std::uniform_real_distribution<float>> RNDZeroToOne_;

};



#endif
