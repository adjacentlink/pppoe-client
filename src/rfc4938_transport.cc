/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_transport.cc
 * version: 1.0
 * date: April 11, 2013
 *
 * Copyright (c) 2013, 2014 - Adjacent Link, LLC, Bridgewater NJ
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

#include <stdio.h>


extern "C" {
#include "rfc4938_debug.h"
#include "rfc4938_io.h"
#include "rfc4938_transport.h"
#include "rfc4938_config.h"
#include "rfc4938_parser.h"
#include "rfc4938_neighbor_manager.h"
}

#include "rfc4938_transport_types.h"
#include "rfc4938_worker_types.h"

#include "emane/application/transportbuilder.h"
#include "emane/controls/serializedcontrolmessage.h"

#define DR_KBS   0x00
#define DR_MBS   0x01
#define DR_GBS   0x10
#define DR_TBS   0x11

#include <math.h>
#include <set>
#include <map>
#include <algorithm>
#include <queue>
#include <thread>


namespace {
   std::unique_ptr<EMANE::Application::TransportManager> pTransportManager;

   PPPoETransport * pTransport{};

   EMANE::Microseconds reportDelay{100};

   EMANE::DoubleSeconds nbrInitTimeOut{};

   const char HELLO_TAG = 'H';

   const unsigned long TIMED_EVENT_TYPE_HELLO = 1;
   const unsigned long TIMED_EVENT_TYPE_PADG  = 2;
   const unsigned long TIMED_EVENT_TYPE_PADI  = 3;
   const unsigned long TIMED_EVENT_TYPE_PADQ  = 4;

   struct TimedEventType {
      unsigned long id_;
      const void * data_;

      TimedEventType(unsigned long id, const void * data) :
        id_{id},
        data_{data}
      { }

      TimedEventType() :
        id_{},
        data_{}
      { }
    };


 TimedEventType TimedEventTypeHello(TIMED_EVENT_TYPE_HELLO, nullptr);

 inline bool isAHello(const void * p, int len)
  {
    if(len == 1)
      {
        const char * m = reinterpret_cast<const char *>(p);

        if(*m == HELLO_TAG)
          {
            return true;
          }
      }

    return false;
  }
}


PPPoETransport::PPPoETransport (EMANE::NEMId id, EMANE::PlatformServiceProvider * p) :
   EMANE::Transport{id, p},
   helloTimedEventId_{},
   flowControlClient_{*this},
   u64MaxSysDataRate_{},
   workQueueThread_{},
   bCanceled_{},
   helloInterval_{},
   lastCreditReportTime_{},
   RNDZeroToOne_{0.0f, 1.0f}
 { 
   nbrInitTimeOut = EMANE::DoubleSeconds{60.0};
 }


PPPoETransport::~PPPoETransport() 
 { 
   disableHellos();
 }


void PPPoETransport::processUpstreamPacket(EMANE::UpstreamPacket & pkt, const EMANE::ControlMessages & msgs) 
 { 
   RFC4938_DEBUG_PACKET("%s:(%hu): pkt len %zd, src %hu", 
                        __func__, 
                        id_, 
                        pkt.length(), 
                        pkt.getPacketInfo().getSource()); 

   processControl_i(msgs);

   if(isAHello(pkt.get(), pkt.length()))
    {
      RFC4938_DEBUG_PACKET("%s:(%hu): consume hello from %hu", 
                           __func__, 
                           id_, 
                           pkt.getPacketInfo().getSource()); 
      return;
    }

    WorkItem item(WORK_ITEM_TYPE_UPSTREAM_PKT, new EMANE::UpstreamPacket(pkt));

    enqueueWorkItem_i(item);
 }


void PPPoETransport::processUpstreamControl(const EMANE::ControlMessages & msgs) 
 { 
    processControl_i(msgs);
 }


void PPPoETransport::processTimedEvent(EMANE::TimerEventId,
                                       const EMANE::TimePoint &, // expireTime
                                       const EMANE::TimePoint &, // scheduleTime
                                       const EMANE::TimePoint &, // fireTime
                                       const void * arg)
 { 
   const TimedEventType * p = reinterpret_cast<const TimedEventType *>(arg);

   if(p->id_ == TIMED_EVENT_TYPE_HELLO)
     {
       enqueueTransportData(EMANE::NEM_BROADCAST_MAC_ADDRESS, 0, &HELLO_TAG, sizeof(HELLO_TAG));
     }
   else if(p->id_ == TIMED_EVENT_TYPE_PADG)
     {
       WorkItem item(WORK_ITEM_TYPE_PADG_MSG, p->data_);

       enqueueWorkItem_i(item);

       delete p;
     }
   else if(p->id_ == TIMED_EVENT_TYPE_PADI)
     {
       WorkItem item(WORK_ITEM_TYPE_PADI_MSG, p->data_);

       enqueueWorkItem_i(item);

       delete p;
     }
   else if(p->id_ == TIMED_EVENT_TYPE_PADQ)
     {
       // TODO
     }
 }


void PPPoETransport::initialize(EMANE::Registrar & )
 { 
   RFC4938_DEBUG_EVENT("%s:(%hu):\n", __func__, id_); 
 }


void PPPoETransport::configure(const EMANE::ConfigurationUpdate & )
 {
   RFC4938_DEBUG_EVENT("%s:(%hu):\n", __func__, id_); 
 }


void PPPoETransport::start()
 { 
   RFC4938_DEBUG_EVENT("%s:(%hu):\n", __func__, id_); 

 }


void PPPoETransport::postStart()
 { 
   RFC4938_DEBUG_EVENT("%s:(%hu):\n", __func__, id_); 

   if(rfc4938_config_is_flow_control_enabled())
     {
       flowControlClient_.start();
     }

   workQueueThread_ = std::thread(&PPPoETransport::processWorkQueue_i, this);
 }


void PPPoETransport::stop()
 { 
   RFC4938_DEBUG_EVENT("%s:(%hu):\n", __func__, id_); 

   bCanceled_ = true;

   // unblock the worker queue
   workQueue_.cancel();

   workQueueThread_.join();

   if(rfc4938_config_is_flow_control_enabled())
     {
       flowControlClient_.stop();
     }
 }


void PPPoETransport::destroy() throw () 
 { 
   RFC4938_DEBUG_EVENT("%s:(%hu):\n", __func__, id_); 
 }


void PPPoETransport::processControl_i(const EMANE::ControlMessages & msgs)
{
  MetricUpdate metricUpdate{};

  for(const auto & pMessage : msgs)
    {
      switch(pMessage->getId())
        {
          case EMANE::Controls::FlowControlControlMessage::IDENTIFIER:
            {
              const auto pFlowControlControlMessage =
                static_cast<const EMANE::Controls::FlowControlControlMessage *>(pMessage);

              flowControlClient_.processFlowControlMessage(pFlowControlControlMessage);

              WorkItem item(WORK_ITEM_TYPE_FLOWCTRL_MSG, nullptr);

              enqueueWorkItem_i(item);
            }
          break;

          case EMANE::Controls::SerializedControlMessage::IDENTIFIER:
            {
              const auto pSerializedControlMessage =
                static_cast<const EMANE::Controls::SerializedControlMessage *>(pMessage);
        
              switch(pSerializedControlMessage->getSerializedId())
                {
                  case EMANE::Controls::FlowControlControlMessage::IDENTIFIER:
                    {
                      std::unique_ptr<EMANE::Controls::FlowControlControlMessage>
                        pMsg{EMANE::Controls::FlowControlControlMessage::create(
                                                                pSerializedControlMessage->getSerialization())};
    
                      flowControlClient_.processFlowControlMessage(pMsg.get());

                      WorkItem item(WORK_ITEM_TYPE_FLOWCTRL_MSG, nullptr);

                      enqueueWorkItem_i(item);
                    }
                  break;

                  case EMANE::Controls::R2RINeighborMetricControlMessage::IDENTIFIER:
                    {
                      metricUpdate.pNbrMetric_ = 
                        EMANE::Controls::R2RINeighborMetricControlMessage::create(
                           pSerializedControlMessage->getSerialization());
                    }
                  break;

                  case EMANE::Controls::R2RIQueueMetricControlMessage::IDENTIFIER:
                    {
                      metricUpdate.pQueueMetric_ = 
                        EMANE::Controls::R2RIQueueMetricControlMessage::create(
                           pSerializedControlMessage->getSerialization());
                    }
                  break;

                  case EMANE::Controls::R2RISelfMetricControlMessage::IDENTIFIER:
                    {
                      metricUpdate.pSelfMetric_ = 
                        EMANE::Controls::R2RISelfMetricControlMessage::create(
                           pSerializedControlMessage->getSerialization());
                    }
                  break;
              }
            break;
           }
       }
   }

  if(metricUpdate.isReady())
    {
      WorkItem item{WORK_ITEM_TYPE_METRIC_MSG, new MetricUpdate{metricUpdate}};

      enqueueWorkItem_i(item);
    }
}



/* ALL packets sent downstream MUST go through this method in order to use flow control correctly */
void PPPoETransport::enqueueTransportData(std::uint16_t dst, std::uint16_t credits, const void * p2buffer, int buflen)
 {
    DSPktParams * p = new DSPktParams(
      new EMANE::DownstreamPacket(
        EMANE::PacketInfo(id_, dst, 0, EMANE::Clock::now()), p2buffer, buflen), credits);

    WorkItem item(WORK_ITEM_TYPE_DNSTREAM_PKT, p);

    enqueueWorkItem_i(item);
 }


void PPPoETransport::deleteNeighbor(std::uint16_t nbr)
 { 
   std::uint16_t * p = new std::uint16_t;

   *p = nbr;

   WorkItem item(WORK_ITEM_TYPE_DEL_NBR, p);

   enqueueWorkItem_i(item);
 }


void PPPoETransport::enableHellos(float fInterval)
 { 
   if(helloTimedEventId_ != 0)
     {
        pPlatformService_->timerService().cancelTimedEvent(helloTimedEventId_);
     }

   if(fInterval > 0.0)
     {
       RFC4938_DEBUG_EVENT("%s:(%hu): send hello every %f sec\n", 
                           __func__, id_, fInterval);

       helloInterval_ = std::chrono::duration_cast<EMANE::Microseconds>(EMANE::DoubleSeconds(fInterval));

       helloTimedEventId_ = 
         pPlatformService_->timerService().scheduleTimedEvent(
           EMANE::Clock::now() + helloInterval_, &TimedEventTypeHello);
    }
 }


void PPPoETransport::disableHellos()
 { 
   RFC4938_DEBUG_EVENT("%s:(%hu):\n", __func__, id_); 

   if(helloTimedEventId_ != 0)
    {
       pPlatformService_->timerService().cancelTimedEvent(helloTimedEventId_);
    }

   helloTimedEventId_ = 0;
 }


void PPPoETransport::notifyFdReady(fd_set fdready, int num)
{
   WorkItem item(WORK_ITEM_TYPE_FD_READY, new FDParams(fdready, num));

   workQueue_.enqueue(item);
}



//
// internal/private methods below
// 
int PPPoETransport::getResources_i(const EMANE::DoubleSeconds & totalBandWidthConsumption, 
                                   const EMANE::DoubleSeconds & interval)
{
  float ratio{};

  if(interval.count() && totalBandWidthConsumption.count())
    {
      ratio = totalBandWidthConsumption.count() / interval.count();
    }

  int result = 100 * (1.5f - ratio);

  // clamp such that min/max = 30/100
  if(result < 30)
   {
     result = 30;
   }
  else if(result > 100)
   {
     result = 100;
   }

  RFC4938_DEBUG_EVENT("%s:(%hu): consumption %64.lf sec, interval %64.lf sec, result %d\n", 
                      __func__, 
                      id_, 
                      totalBandWidthConsumption.count(), 
                      interval.count(), 
                      result); 
  return result;
}


PPPoETransport::DataRateScaledPair PPPoETransport::getDataRateScaleFactor_i(std::uint64_t u64DataRatebps)
{
   PPPoETransport::DataRateScaledPair pair (0,0);

   // Kbps
   if(u64DataRatebps < 1e6)
     {
       // scale down
       pair.first = u64DataRatebps / 1e3;

       pair.second = DR_KBS;
     }
   // Mbps
   else if(u64DataRatebps < 1e9)
     {
       // scale down
       pair.first = u64DataRatebps / 1e6;

       pair.second = DR_MBS;
     }
   // Gbps
   else if(u64DataRatebps < 1e12)
     {
       // scale down
       pair.first = u64DataRatebps / 1e9;

       pair.second = DR_GBS;
     }
   // Tbps (really)
   else 
     {
       // scale down
       pair.first = u64DataRatebps / 1e12;

       pair.second = DR_TBS;
     }

   RFC4938_DEBUG_EVENT("%s:(%hu): datarate  %ju bps, adjusted %hu, scale %hu\n", 
                       __func__, 
                       id_, 
                       u64DataRatebps, 
                       pair.first, 
                       pair.second);

  return pair;
}


void PPPoETransport::updateNeighborCacheAndStats_i(NEMIdSet & latestNeighbors)
{
   // called from protected area
   
   NEMIdSet nbrSetToAdd, nbrSetToRemove;

   // get additions
   std::set_difference(latestNeighbors.begin(),    latestNeighbors.end(), 
                       cachedNeighbors_.begin(),   cachedNeighbors_.end(),
                       std::inserter(nbrSetToAdd,  nbrSetToAdd.begin()));

   // get deletions
   std::set_difference(cachedNeighbors_.begin(),     cachedNeighbors_.end(), 
                       latestNeighbors.begin(),      latestNeighbors.end(),
                       std::inserter(nbrSetToRemove, nbrSetToRemove.begin()));

   // let track how our sessions are doing
   for(auto nbrEntry : neighborStatistics_)
    {
      // is still a nbr
      if(latestNeighbors.find(nbrEntry.first) != latestNeighbors.end())
        {
          const rfc4938_neighbor_state_t state = rfc4938_get_neighbor_state(nbrEntry.first);

          if(state != ACTIVE)
           {
             EMANE::DoubleSeconds deltaT = 
               std::chrono::duration_cast<EMANE::DoubleSeconds>(EMANE::Clock::now()- nbrEntry.second.tp_);

             if(deltaT > nbrInitTimeOut)
              {
                RFC4938_DEBUG_EVENT("%s:(%hu): nbr %hu session is %s, inactive timeout after %6.4lf, terminate this session\n", 
                                    __func__, id_, 
                                    nbrEntry.first, 
                                    rfc4938_neighbor_status_to_string(state),  
                                    deltaT.count()); 

                // add to delete set
                nbrSetToRemove.insert(nbrEntry.first);

                // remove from latest
                latestNeighbors.erase(nbrEntry.first);
              }
             else
              {
                RFC4938_DEBUG_EVENT("%s:(%hu): nbr %hu session is %s, inactive time %6.4lf, holding until %6.4lf\n", 
                                    __func__, id_, 
                                    nbrEntry.first, 
                                    rfc4938_neighbor_status_to_string(state),  
                                    deltaT.count(), 
                                    nbrInitTimeOut.count()); 
              }
           }
        }
    }


   // setup new nbrs
   for(auto nbr : nbrSetToAdd)
     {
       // a new nbr, lets tell pppoe
       TimedEventType * p = 
         new TimedEventType{TIMED_EVENT_TYPE_PADI, 
            new PADIParams(nbr, rfc4938_config_get_credit_scalar())};
              
       pPlatformService_->timerService().scheduleTimedEvent(EMANE::Clock::now() + reportDelay, p);

       RFC4938_DEBUG_EVENT("%s:(%hu): new nbr %hu, scalar %hu, scheduled padi event\n", 
                           __func__, 
                           id_, 
                           nbr, 
                           rfc4938_config_get_credit_scalar());

       // add our nbr stats
       neighborStatistics_[nbr] = NeighborStats();
    }

   // remove old nbrs
   for(auto nbr : nbrSetToRemove)
    {
      RFC4938_DEBUG_EVENT("%s:(%hu): expunge old nbr %hu\n", __func__, id_, nbr); 

      // tell pppoe this nbr is gone
      rfc4938_parser_cli_terminate_session(nbr, CMD_SRC_TRANSPORT);

      // remove stats
      neighborStatistics_.erase(nbr);
    }

   // update nbr cache with the latest set
   cachedNeighbors_ = latestNeighbors;

   RFC4938_DEBUG_EVENT("%s:(%hu): updated cache, num nbrs is %zu\n", 
                       __func__, 
                       id_, 
                       cachedNeighbors_.size());
}



void PPPoETransport::handleTokenUpdate_i()
{
  if((rfc4938_config_get_credit_dist_mode() == CREDIT_DIST_MODE_EVEN) ||
     (rfc4938_config_get_credit_dist_mode() == CREDIT_DIST_MODE_FLAT))
    {
      // do we have any neighbors
      if(neighborStatistics_.empty() == false)
        {
          // get total credits
          const std::uint16_t u16TotalCredits = rfc4938_config_get_credit_grant();

          // credits per neighbor even or flat distribution
          const std::uint16_t u16NeighborCredits = rfc4938_config_get_credit_dist_mode() == CREDIT_DIST_MODE_EVEN ?
                                                u16TotalCredits / neighborStatistics_.size() :  u16TotalCredits;

          // for each nbr
          for(auto nbrEntry : neighborStatistics_)
            {
               TimedEventType * p = 
                 new TimedEventType{TIMED_EVENT_TYPE_PADG, 
                    new PADGParams(nbrEntry.first, u16NeighborCredits)};
              
               pPlatformService_->timerService().scheduleTimedEvent(EMANE::Clock::now() + reportDelay, p);

               RFC4938_DEBUG_EVENT("%s:(%hu): nbr %hu, credits (total %hu, dist %hu), scheduled padg event\n", 
                                   __func__, 
                                   id_, 
                                   nbrEntry.first, 
                                   u16TotalCredits, 
                                   u16NeighborCredits);
            }
        }
    }
}


void PPPoETransport::handleMetricMessage_i(const MetricUpdate * pUpdate)
{
   // sum of all nbr bandwidth consumption
   EMANE::Microseconds  totalBandWidthConsumption{};

   // the current nbrs learned from this message
   NEMIdSet currentNeighbors;

   auto NbrMetrics = pUpdate->pNbrMetric_->getNeighborMetrics();

   for(auto metric : NbrMetrics)
    {
      // sum up the bw consumption
      totalBandWidthConsumption += metric.getBandwidthConsumption();

      // check p2p mode 
      currentNeighbors.insert(rfc4938_config_get_id(metric.getId()));
    }

   // update the nbr cache
   updateNeighborCacheAndStats_i(currentNeighbors);


   // the max queue delay 
   EMANE::Microseconds maxQueueDelay{};

   auto Qmetrics = pUpdate->pQueueMetric_->getQueueMetrics();

   for(auto metric : Qmetrics)
    {
      // find the largest
      if(maxQueueDelay < metric.getAvgDelay())
       {
         maxQueueDelay = metric.getAvgDelay();
       }
    }

   // reset our overall max data rate
   u64MaxSysDataRate_ = 0;

   // max system data rate 
   auto maxDataRate = pUpdate->pSelfMetric_->getMaxDataRatebps();

   // broadcast data rate
   auto currentDataRate = pUpdate->pSelfMetric_->getBroadcastDataRatebps();

   // save max data rate
   if(u64MaxSysDataRate_ < maxDataRate)
    {
      u64MaxSysDataRate_ = maxDataRate;

      RFC4938_DEBUG_EVENT("%s:(%hu): max datarate %ju bps \n", __func__, id_, u64MaxSysDataRate_);
    }

      
   if(! NbrMetrics.empty())
    { 
      // p2p mode
      if(rfc4938_config_get_p2p_mode())
       {
         // for each nbr metric, send a PADQ message
         for(auto metric : NbrMetrics)
          {
            // lookup nbr stats
            const auto & iter = neighborStatistics_.find(metric.getId());

            // unknown nbr
            if(iter == neighborStatistics_.end())
             {
               continue;
             }

            // not up yet
            if(rfc4938_get_neighbor_state (metric.getId()) != ACTIVE)
             {
               continue;
             }

            // last data rate > 0
            if(iter->second.u64LastTxDataRate_ > 0)
             {
               currentDataRate = iter->second.u64LastTxDataRate_;
             }
            // use max
            else
             {
               currentDataRate = maxDataRate;
             }

           // save last data rate
           iter->second.u64LastTxDataRate_ = currentDataRate;

           // get current data rate adjusted value and scale
           const DataRateScaledPair u16CdrScale = getDataRateScaleFactor_i(currentDataRate);

           // get max data rate scale AND adjust input value
           const DataRateScaledPair u16MdrScale = getDataRateScaleFactor_i(maxDataRate);

           // calculate resources
           const int iResources = getResources_i(
                 std::chrono::duration_cast<EMANE::DoubleSeconds>(totalBandWidthConsumption), 
                 std::chrono::duration_cast<EMANE::DoubleSeconds>(pUpdate->pSelfMetric_->getReportInterval()));

           // calculate relative link quality
           const int iRLQ = getRLQ_i(metric.getId(),                // neighbor id
                                     metric.getSINRAvgdBm(),        // avg sinr
                                     metric.getNumRxFrames(),       // num rx frames
                                     metric.getNumMissedFrames());  // num missed frames

           rfc4938_parser_cli_padq_session (metric.getId(),               // neighbor_id 
                                            0,                            // receive_only (0 == bi-directional)
                                            iRLQ,                         // rlq 
                                            iResources,                   // resources 
                                            maxQueueDelay.count() / 1000, // latency (ms)
                                            u16CdrScale.second,           // cdr_scale
                                            u16CdrScale.first,            // cdr
                                            u16MdrScale.second,           // mdr_scale
                                            u16MdrScale.first);           // mdr
         }
       } 
      else
       {
         // get current data rate adjusted value and scale
         const DataRateScaledPair u16CdrScale = getDataRateScaleFactor_i(currentDataRate);

         // get max data rate adjusted value and scale
         const DataRateScaledPair u16MdrScale = getDataRateScaleFactor_i(maxDataRate);

         rfc4938_parser_cli_padq_session (EMANE::NEM_BROADCAST_MAC_ADDRESS,  // broadcast 
                                          0,                                 // receive_only (0 == bi-directional)
                                          100,                               // rlq 
                                          0,                                 // resources 
                                          maxQueueDelay.count() / 1000,      // latency (ms)
                                          u16CdrScale.second,                // cdr_scale
                                          u16CdrScale.first,                 // cdr
                                          u16MdrScale.second,                // mdr_scale
                                          u16MdrScale.first);                // mdr
       }
    }
}


int PPPoETransport::getRLQ_i(const std::uint16_t nbr, const float fSINRAvg, 
                           const std::uint32_t numRxFrames, const std::uint32_t numMissedFrames)
{
  // called from protected area

  // sum up num rx and missed frames
  const std::uint32_t numRxAndMissedFrames = numRxFrames + numMissedFrames;

  // the sinr to be used
  float fTheSINR{};

  // the recv ratio
  float fTheRR{};

  // lets find some history in this nbr 
  // even if this is the first report of this nbr it should 
  // be stored with initial values of (RR = 0, and SINR = -256)
  const auto & iter = neighborStatistics_.find(nbr);

  // we know nothing about this neighbor
  if(iter == neighborStatistics_.end())
   {
     return 0;
   }

  // we have no pkt info for this interval
  if(numRxAndMissedFrames == 0.0f)
   {
     // use the last sinr - 3dB
     fTheSINR = iter->second.fSINRlast_ - 3.0f;

     // use the last rr
     fTheRR = iter->second.fRRlast_;
   }
  else
   {
     // use the provided avg sinr
     fTheSINR = fSINRAvg;

     // calculate RR
     fTheRR = (float) numRxFrames / (float) numRxAndMissedFrames;
   }

  // the rlq
  int RLQ;
 
  // check sinr is above min configured value 
  if(fTheSINR > rfc4938_config_get_sinr_min())
   {
     // the configured sinr dalta
     const float fDeltaConfigSINR = rfc4938_config_get_sinr_max() - rfc4938_config_get_sinr_min();

     // the min to avg sinr delta 
     const float fDeltaSINR = fTheSINR - rfc4938_config_get_sinr_min();

     // calculate rlq
     const float val = 100.0f * (fDeltaSINR / fDeltaConfigSINR) * fTheRR;

     // clamp between 0 and 100
     if(val < 0.0f)
      {
        RLQ = 0;
      }
     else if(val > 100.f)
      {
        RLQ = 100;
      }
     else
      {
        RLQ = val;
      }
   }
  else
   {
      RLQ = 0;
   }

  // save the sinr
  iter->second.fSINRlast_ = fTheSINR;

  // save the rr
  iter->second.fRRlast_ = fTheRR;

  RFC4938_DEBUG_EVENT("%s:(%hu): nbr %hu, sinr %f, rr %f, RLQ %d\n", 
                      __func__, 
                      id_, 
                      nbr, 
                      fTheSINR, 
                      fTheRR, 
                      RLQ); 

  return RLQ;
}



void 
PPPoETransport::sendDownstream_i(EMANE::DownstreamPacket * p, std::uint16_t credits)
{
   if(rfc4938_config_is_flow_control_enabled())
     {
        // block and wait for an available flow control token
        // caller can be blocked here for a bit and should not be running on
        // the platform thread. need to run on our own thread

       flowControlClient_.removeToken();
     }

   const EMANE::NEMId dst = p->getPacketInfo().getDestination();

   RFC4938_DEBUG_PACKET("%s:(%hu): send pkt dst %hu, len %zd, credits %hu, distmode %d\n", 
                        __func__, 
                        id_, 
                        dst, 
                        p->length(), 
                        credits, 
                        rfc4938_config_get_credit_dist_mode());

   // send pkt
   sendDownstreamPacket(*p);

   if(isAHello(p->getVectorIO()[0].iov_base, p->length()))
     {
       if(helloInterval_ > EMANE::Microseconds{})
         {
           helloTimedEventId_ = 
             pPlatformService_->timerService().scheduleTimedEvent(
                EMANE::Clock::now() + helloInterval_, &TimedEventTypeHello);
         }
     }

   if((credits > 0) &&
      (u64MaxSysDataRate_ > 0) && 
      (rfc4938_config_get_credit_dist_mode() == CREDIT_DIST_MODE_DIRECT))
     {
       EMANE::DoubleSeconds duration{((p->length() * 8.0) / u64MaxSysDataRate_)};

       EMANE::TimePoint tp = EMANE::Clock::now() + std::chrono::duration_cast<EMANE::Microseconds>(duration);

       // stagger the timeouts
       if(lastCreditReportTime_ != EMANE::TimePoint{})
         {
           EMANE::Microseconds deltaT = 
            std::chrono::duration_cast<EMANE::Microseconds>(lastCreditReportTime_ - EMANE::Clock::now());

           // last report time is in the future
           if(deltaT > EMANE::Microseconds{})
             {
               // push this report forward
               tp += deltaT;
             }
         }

       // save last report time
       lastCreditReportTime_ = tp;
      
       // schedule padg timed event
       TimedEventType * p = 
         new TimedEventType{TIMED_EVENT_TYPE_PADG, 
            new PADGParams(dst, credits)};
              
       pPlatformService_->timerService().scheduleTimedEvent(tp, p);

       RFC4938_DEBUG_PACKET("%s:(%hu): dst %hu, duration %f usec, credits consumed %hu, scheduled padg event\n", 
                            __func__, 
                            id_, 
                            dst, 
                            duration.count(), 
                            credits);
    }
}



void 
PPPoETransport::enqueueWorkItem_i(WorkItem & item)
{
  workQueue_.enqueue(item);

  RFC4938_DEBUG_EVENT("%s:(%hu): push item %s, depth %zd\n", 
                      __func__, 
                      id_, 
                      workIdToString(item.id_), 
                      workQueue_.size());
}


void *
PPPoETransport::processWorkQueue_i()
{
  // all processing should run on this thread
  while(!bCanceled_) 
    {
      //  blocking call
      WorkItem item = workQueue_.dequeue();
 
      RFC4938_DEBUG_EVENT("%s:(%hu): pull item %s, depth %zd\n", 
                          __func__, 
                          id_, 
                          workIdToString(item.id_), 
                          workQueue_.size());

      if (bCanceled_ || item.id_ == WORK_ITEM_TYPE_NONE)
        {
          // done
          break;
        }

      switch(item.id_)
       {
         case WORK_ITEM_TYPE_UPSTREAM_PKT:
          {
            const EMANE::UpstreamPacket * p = reinterpret_cast<const EMANE::UpstreamPacket *>(item.data_);

            rfc4938_parser_parse_upstream_packet(p->get(), 
                                                 p->length(),
                                                 rfc4938_config_get_id(p->getPacketInfo().getSource()));

            delete p;
          } break;

         case WORK_ITEM_TYPE_METRIC_MSG:
          {
            const MetricUpdate * p = reinterpret_cast<const MetricUpdate *>(item.data_);

            handleMetricMessage_i(p);

            delete p->pNbrMetric_;
            delete p->pQueueMetric_;
            delete p->pSelfMetric_;
            delete p;
          } break;

         case WORK_ITEM_TYPE_FLOWCTRL_MSG:
          {
            handleTokenUpdate_i();
          } break;

         case WORK_ITEM_TYPE_DNSTREAM_PKT:
          {
            const DSPktParams * p = reinterpret_cast<const DSPktParams *>(item.data_);

            sendDownstream_i(p->pkt_, p->credits_);

            delete p->pkt_;
            delete p;
          } break;

         case WORK_ITEM_TYPE_PADG_MSG:
          {
            const PADGParams * p = reinterpret_cast<const PADGParams *>(item.data_);

            const auto & iter = neighborStatistics_.find(p->nbrId_);

            if(iter != neighborStatistics_.end())
             {
               if(iter->second.u16PendingCredits_ >= 
                  (rfc4938_config_get_credit_threshold() * rfc4938_config_get_credit_grant()))
                 {
                   rfc4938_parser_cli_padg_session(p->nbrId_, iter->second.u16PendingCredits_);

                   iter->second.u16PendingCredits_ = 0;
                 }
               else
                 {
                   iter->second.u16PendingCredits_ += p->credits_;

                   RFC4938_DEBUG_PACKET("%s:(%hu): dst %hu, add %hu to %hu pending credits\n", 
                                        __func__, 
                                        id_, 
                                        p->nbrId_, 
                                        p->credits_, 
                                        iter->second.u16PendingCredits_);
                 }
             }

            delete p;
          } break;

         case WORK_ITEM_TYPE_PADI_MSG:
          {
            const PADIParams * p = reinterpret_cast<const PADIParams *>(item.data_);

            rfc4938_parser_cli_initiate_session (p->nbrId_, p->scalar_);

            delete p;
          } break;

         case WORK_ITEM_TYPE_DEL_NBR:
          {
            const std::uint16_t * p = reinterpret_cast<const std::uint16_t *>(item.data_);

            // remove stats
            neighborStatistics_.erase(*p);

            // remove from cache
            cachedNeighbors_.erase(*p);

            delete p;
          } break;

         case WORK_ITEM_TYPE_FD_READY:
          {
            const FDParams * p = reinterpret_cast<const FDParams*>(item.data_);

            rfc4938_io_get_messages(p->fdready_, p->num_);

            delete p;
          } break;
       }
    }
 
  // thread terminated 
  return 0;
}


// external api functions

int rfc4938_transport_setup (const char *pzPlatformEndpoint, const char *pzTransportEndpoint, unsigned long id)
{
  RFC4938_DEBUG_EVENT("%s:(%u): begin\n",  __func__, rfc4938_config_get_node_id()); 

  EMANE::Application::TransportBuilder tb;

  auto tpair =
    tb.buildTransportWithAdapter<PPPoETransport>(
      id,
      {},
      pzPlatformEndpoint,
      pzTransportEndpoint);

  pTransport = std::get<0>(tpair);

  EMANE::Application::TransportAdapters adapters{};

  adapters.push_back(std::move(std::get<1>(tpair)));


  uuid_t tuuid{};

  uuid_generate(tuuid);

  pTransportManager = tb.buildTransportManager(tuuid, adapters, {});

  pTransportManager->start();

  pTransportManager->postStart();

  RFC4938_DEBUG_EVENT("%s:(%u): complete\n",  __func__, rfc4938_config_get_node_id()); 

  return 0;
}


void rfc4938_transport_cleanup (void)
{
  if(pTransportManager) 
    {
      pTransportManager->stop();

      pTransportManager->destroy();
    }
}


void rfc4938_transport_send(unsigned short dst, unsigned short credits, const void * p2buffer, int buflen)
{
  if(pTransport)
    {
      pTransport->enqueueTransportData(dst, credits, p2buffer, buflen);
    }
}


void rfc4938_transport_enable_hellos (float fInterval)
{
  if(fInterval > 0)
    {
      pTransport->enableHellos(fInterval);
    }
  else
    {
      pTransport->disableHellos();
    }
}


void rfc4938_transport_neighbor_terminated  (UINT32_t neighbor_id)
{
   if(pTransport)
     {
       pTransport->deleteNeighbor(neighbor_id);
     }
}


void rfc4938_transport_notify_fd_ready(fd_set fdready, int num)
{
   if(pTransport)
     {
       pTransport->notifyFdReady(fdready, num);
     }
}
