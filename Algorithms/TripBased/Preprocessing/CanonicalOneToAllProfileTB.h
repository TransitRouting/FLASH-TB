/**********************************************************************************

 Copyright (c) 2023 Patrick Steil

 MIT License

 Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**********************************************************************************/
#pragma once

#include "../../../DataStructures/Container/Parent.h"
#include "../../../DataStructures/Container/Set.h"
#include "../../../DataStructures/RAPTOR/Entities/ArrivalLabel.h"
#include "../../../DataStructures/RAPTOR/Entities/Journey.h"
#include "../../../DataStructures/RAPTOR/Entities/RouteSegment.h"
#include "../../../DataStructures/TripBased/Data.h"
#include "../../../Helpers/String/String.h"
#include "SplitStopEventGraph.h"

#ifdef USE_SIMD
#include "../Query/ProfileReachedIndexSIMD.h"
#else
#include "../Query/ProfileReachedIndex.h"
#endif
#include "../Query/ReachedIndex.h"

namespace TripBased {

struct TripStopIndex {
  TripStopIndex(const TripId trip = noTripId,
                const StopIndex stopIndex = StopIndex(-1),
                const int depTime = never)
      : trip(trip), stopIndex(stopIndex), depTime(depTime) {}

  TripId trip;
  StopIndex stopIndex;
  int depTime;
};

struct RouteLabel {
  RouteLabel() : numberOfTrips(0) {}
  inline StopIndex end() const noexcept {
    return StopIndex(departureTimes.size() / numberOfTrips);
  }
  u_int32_t numberOfTrips;
  std::vector<int> departureTimes;
};

class CanonicalOneToAllProfileTB {
 private:
  struct TripLabel {
    TripLabel(const StopEventId begin = noStopEvent,
              const StopEventId end = noStopEvent, const u_int32_t parent = -1)
        : begin(begin), end(end), parent(parent) {}
    StopEventId begin;
    StopEventId end;
    u_int32_t parent;
  };

  struct EdgeRange {
    EdgeRange() : begin(noEdge), end(noEdge) {}

    Edge begin;
    Edge end;
  };

  struct EdgeLabel {
    EdgeLabel(const StopEventId stopEvent = noStopEvent,
              const TripId trip = noTripId,
              const StopEventId firstEvent = noStopEvent)
        : stopEvent(stopEvent), trip(trip), firstEvent(firstEvent) {}
    StopEventId stopEvent;
    TripId trip;
    StopEventId firstEvent;
  };

  struct TargetLabel {
    TargetLabel(const long arrivalTime = INFTY,
                const long departureTime = INFTY)
        : arrivalTime(arrivalTime), departureTime(departureTime) {}

    void clear() {
      arrivalTime = INFTY;
      departureTime = INFTY;
    }

    long arrivalTime;
    long departureTime;
  };

 public:
  CanonicalOneToAllProfileTB(
      Data &data, const SplitStopEventGraph &splitEventGraph,
      std::vector<std::vector<uint8_t>> &uint8Flags,
      std::vector<std::vector<TripStopIndex>> &collectedDepTimes,
      std::vector<TripBased::RouteLabel> &routeLabels)
      : data(data),
        splitEventGraph(splitEventGraph),
        uint8Flags(uint8Flags),
        numberOfPartitions(data.raptorData.numberOfPartitions),
        transferFromSource(data.numberOfStops(), INFTY),
        lastSource(StopId(0)),
        reachedRoutes(data.numberOfRoutes()),
        queue(data.numberOfStopEvents()),
        queueSize(0),
        profileReachedIndex(data),
        runReachedIndex(data),
        targetLabels(data.raptorData.numberOfStops() * 16, TargetLabel()),
        targetLabelChanged(data.raptorData.numberOfStops() * 16, 0),
        stopsToUpdate(data.numberOfStops()),
        edgeLabels(data.stopEventGraph.numEdges()),
        sourceStop(noStop),
        collectedDepTimes(collectedDepTimes),
        parentOfTrip(16, data.numberOfTrips()),
        parentOfStop(16, data.numberOfStops()),
        tripLabelEdge(data.numberOfStopEvents(),
                      std::make_pair(noEdge, noStopEvent)),
        routeLabels(routeLabels),
        previousTripLookup(data.numberOfTrips()),
        timestamp(0) {
    assert(splitEventGraph.numberOfLocalEdges() +
               splitEventGraph.numberOfTransferEdges() ==
           data.stopEventGraph.numEdges());
    assert(splitEventGraph.numberOfLocalEdges() +
               splitEventGraph.numberOfTransferEdges() ==
           edgeLabels.size());
    // first the local transfers
    size_t edgeIndex = 0;
    for (; edgeIndex < splitEventGraph.numberOfLocalEdges(); ++edgeIndex) {
      edgeLabels[edgeIndex].stopEvent =
          StopEventId(splitEventGraph.toLocalVertex[edgeIndex] + 1);
      edgeLabels[edgeIndex].trip =
          data.tripOfStopEvent[splitEventGraph.toLocalVertex[edgeIndex]];
      edgeLabels[edgeIndex].firstEvent =
          data.firstStopEventOfTrip[edgeLabels[edgeIndex].trip];

      AssertMsg(edgeLabels[edgeIndex].stopEvent < data.numberOfStopEvents(),
                "Event " << edgeLabels[edgeIndex].stopEvent
                         << " (by local transfer) is not valid!");
      AssertMsg(data.isTrip(edgeLabels[edgeIndex].trip),
                "Trip " << edgeLabels[edgeIndex].trip
                        << " (by local transfer) is not valid!");
      AssertMsg(edgeLabels[edgeIndex].firstEvent < data.numberOfStopEvents(),
                "Firstevent " << edgeLabels[edgeIndex].firstEvent
                              << " (by local transfer) is not valid!");
    }

    const size_t offset = splitEventGraph.numberOfLocalEdges();

    // then the footpath transfers
    for (; edgeIndex < offset + splitEventGraph.numberOfTransferEdges();
         ++edgeIndex) {
      edgeLabels[edgeIndex].stopEvent =
          StopEventId(splitEventGraph.toTransferVertex[edgeIndex - offset] + 1);
      edgeLabels[edgeIndex].trip =
          data.tripOfStopEvent[splitEventGraph
                                   .toTransferVertex[edgeIndex - offset]];
      edgeLabels[edgeIndex].firstEvent =
          data.firstStopEventOfTrip[edgeLabels[edgeIndex].trip];

      AssertMsg(edgeLabels[edgeIndex].stopEvent < data.numberOfStopEvents(),
                "Event " << edgeLabels[edgeIndex].stopEvent
                         << " (by transfer-transfer) is not valid!");
      AssertMsg(data.isTrip(edgeLabels[edgeIndex].trip),
                "Trip " << edgeLabels[edgeIndex].trip
                        << " (by transfer-transfer) is not valid!");
      AssertMsg(edgeLabels[edgeIndex].firstEvent < data.numberOfStopEvents(),
                "Firstevent " << edgeLabels[edgeIndex].firstEvent
                              << " (by transfer-transfer) is not valid!");
    }

    // pre load every previous trip since we need to look this up quite often
    for (const RouteId route : data.routes()) {
      TripId firstTrip = data.firstTripOfRoute[route];
      for (const TripId trip : data.tripsOfRoute(route)) {
        previousTripLookup[trip] = TripId(trip - 1);
      }
      previousTripLookup[firstTrip] = firstTrip;
    }
  }

  inline void run(const Vertex source) noexcept {
    AssertMsg(data.isStop(StopId(source)), "Given source is not a stop!");
    run(StopId(source));
  }

  inline void run(const StopId source) noexcept {
    sourceStop = source;
    std::vector<RAPTOR::Journey> journeyOfRound;

    // reset everything
    reset();

    computeInitialAndFinalTransfers();

    // perform one EA query from 24:00:00 to get all the journeys only from the
    // second day
    performOneEAQueryAtMidnight();

    // we get the depTimes from the main file that calls this thread
    // collectDepartures();
    size_t i(0), j(0);

    while (i < collectedDepTimes[sourceStop].size()) {
      ++timestamp;

      // clear (without reset)
      clear();

      int currentDepTime = collectedDepTimes[sourceStop][i].depTime;

      // now we collect all the trips and stop sequences at a certain
      // timestamp and perform one normal query
      while (j < collectedDepTimes[sourceStop].size() &&
             currentDepTime == collectedDepTimes[sourceStop][j].depTime) {
        enqueue(collectedDepTimes[sourceStop][j].trip,
                StopIndex(collectedDepTimes[sourceStop][j].stopIndex + 1));
        ++j;
      }

      scanTrips(currentDepTime);

      // unwind and flag all Journeys
      for (const StopId target : stopsToUpdate) {
        unwindJourneys(target);
      }

      i = j;
    }
  }

  inline void performOneEAQueryAtMidnight() noexcept {
    evaluateInitialTransfers();
    scanTrips(24 * 60 * 60);
    for (const StopId target : stopsToUpdate) unwindJourneys(target);
  }

  inline void evaluateInitialTransfers() noexcept {
    reachedRoutes.clear();
    for (const RAPTOR::RouteSegment &route :
         data.raptorData.routesContainingStop(sourceStop)) {
      reachedRoutes.insert(route.routeId);
    }
    for (const Edge edge :
         data.raptorData.transferGraph.edgesFrom(sourceStop)) {
      const Vertex stop = data.raptorData.transferGraph.get(ToVertex, edge);
      for (const RAPTOR::RouteSegment &route :
           data.raptorData.routesContainingStop(StopId(stop))) {
        reachedRoutes.insert(route.routeId);
      }
    }
    reachedRoutes.sort();
    auto &valuesToLoopOver = reachedRoutes.getValues();

    for (size_t i = 0; i < valuesToLoopOver.size(); ++i) {
#ifdef ENABLE_PREFETCH
      if (i + 4 < valuesToLoopOver.size()) {
        __builtin_prefetch(&(routeLabels[valuesToLoopOver[i + 4]]));
        __builtin_prefetch(&(data.firstTripOfRoute[valuesToLoopOver[i + 4]]));
      }
#endif
      const RouteId route = valuesToLoopOver[i];
      const RouteLabel &label = routeLabels[route];
      const StopIndex endIndex = label.end();
      const TripId firstTrip = data.firstTripOfRoute[route];
      const StopId *stops = data.raptorData.stopArrayOfRoute(route);
      TripId tripIndex = noTripId;
      for (StopIndex stopIndex(0); stopIndex < endIndex; stopIndex++) {
        const int timeFromSource = transferFromSource[stops[stopIndex]];
        if (timeFromSource == INFTY) continue;
        const int stopDepartureTime = 24 * 60 * 60 + timeFromSource;
        const u_int32_t labelIndex = stopIndex * label.numberOfTrips;
        if (tripIndex >= label.numberOfTrips) {
          tripIndex = std::lower_bound(
              TripId(0), TripId(label.numberOfTrips), stopDepartureTime,
              [&](const TripId trip, const int time) {
                return label.departureTimes[labelIndex + trip] < time;
              });
          if (tripIndex >= label.numberOfTrips) continue;
        } else {
          if (label.departureTimes[labelIndex + tripIndex - 1] <
              stopDepartureTime)
            continue;
          --tripIndex;
          while ((tripIndex > 0) &&
                 (label.departureTimes[labelIndex + tripIndex - 1] >=
                  stopDepartureTime)) {
            --tripIndex;
          }
        }
        enqueue(firstTrip + tripIndex, StopIndex(stopIndex + 1));
        if (tripIndex == 0) break;
      }
    }
  }
  inline void unwindJourneys(const StopId &target) noexcept {
    int bestArrivalTime = INFTY;
    int partition = data.getPartitionCell(target);

    for (int i = 0; i < 16; ++i) {
      if (!isTargetLabelMarkedAsChanged(target, i)) {
        continue;
      }
      const TargetLabel &label = getTargetLabel(target, i);
      if (label.arrivalTime >= bestArrivalTime) continue;

      bestArrivalTime = label.arrivalTime;
      getJourneyAndUnwind(target, i, partition);
    }
  }

 private:
  inline void reset() noexcept {
    profileReachedIndex.clear();
    std::fill(targetLabels.begin(), targetLabels.end(), TargetLabel());
    clear();
  }

  inline void clear() noexcept {
    queueSize = 0;
    runReachedIndex.clear();
    std::fill(targetLabelChanged.begin(), targetLabelChanged.end(), 0);
    stopsToUpdate.clear();
  }

  inline void computeInitialAndFinalTransfers() noexcept {
    transferFromSource[lastSource] = INFTY;
    for (const Edge edge :
         data.raptorData.transferGraph.edgesFrom(lastSource)) {
      const Vertex stop = data.raptorData.transferGraph.get(ToVertex, edge);
      transferFromSource[stop] = INFTY;
    }
    transferFromSource[sourceStop] = 0;
    for (const Edge edge :
         data.raptorData.transferGraph.edgesFrom(sourceStop)) {
      const Vertex stop = data.raptorData.transferGraph.get(ToVertex, edge);
      transferFromSource[stop] =
          data.raptorData.transferGraph.get(TravelTime, edge);
    }
    lastSource = sourceStop;
  }

  inline void scanTrips(const int departureTime) noexcept {
    size_t roundBegin = 0;
    size_t roundEnd = queueSize;
    u_int8_t n = 1;
    int travelTime(-1);
    StopId stop(-1);
    Vertex transferStop(-1);

    while (roundBegin < roundEnd && n < 16) {
      std::sort(queue.begin() + roundBegin, queue.begin() + roundEnd,
                [](const auto &left, const auto &right) {
                  return std::tie(left.begin, left.end) <
                         std::tie(right.begin, right.end);
                });

      // no footpath
      for (size_t i = roundBegin; i < roundEnd; ++i) {
#ifdef ENABLE_PREFETCH
        if (i + 4 < roundEnd) {
          __builtin_prefetch(&(queue[i + 4]));
          __builtin_prefetch(&(data.arrivalEvents[queue[i + 4].begin]));
        }
#endif

        const TripLabel &label = queue[i];
        const TripId currentTripId = data.tripOfStopEvent[label.begin];
        assert(runReachedIndex(currentTripId) <= label.begin);

        for (StopEventId j = label.begin; j < label.end; ++j) {
          addArrival(data.arrivalEvents[j].stop,
                     data.arrivalEvents[j].arrivalTime, departureTime, n,
                     currentTripId, j);
        }
      }

      // with footpath
      for (size_t i = roundBegin; i < roundEnd; ++i) {
        const TripLabel &label = queue[i];

        const TripId currentTripId = data.tripOfStopEvent[label.begin];
        assert(runReachedIndex(currentTripId) <= label.begin);

        for (StopEventId j = label.begin; j < label.end; ++j) {
          assert(runReachedIndex(currentTripId) <= label.begin);

          stop = data.arrivalEvents[j].stop;
          AssertMsg(data.isStop(stop), "Stop " << stop << " is not a stop!");
          AssertMsg(data.raptorData.transferGraph.isVertex(stop),
                    "This stop is not represented in the transfergraph!\n");
          for (const Edge edge :
               data.raptorData.transferGraph.edgesFrom(stop)) {
            transferStop =
                StopId(data.raptorData.transferGraph.get(ToVertex, edge));
            AssertMsg(data.isStop(transferStop),
                      "Stop " << transferStop << " is not a stop!");

            travelTime = data.raptorData.transferGraph.get(TravelTime, edge);

            addArrival(StopId(transferStop),
                       data.arrivalEvents[j].arrivalTime + travelTime,
                       departureTime, n, currentTripId, j);
          }
        }
      }

      // transfers with no footpath
      for (size_t i = roundBegin; i < roundEnd; ++i) {
#ifdef ENABLE_PREFETCH
        if (i + 4 < roundEnd) {
          __builtin_prefetch(&(queue[i + 4]));
          __builtin_prefetch(&(data.arrivalEvents[queue[i + 4].begin]));
        }
#endif

        TripLabel &label = queue[i];
        for (StopEventId j = label.begin; j < label.end; ++j) {
          // local pruning
          if (data.arrivalEvents[j].arrivalTime >
              getTargetLabel(data.arrivalEvents[j].stop, n).arrivalTime) {
            continue;
          }

          for (auto edgeIndex = splitEventGraph.beginLocalEdgeFrom(j);
               edgeIndex < splitEventGraph.beginLocalEdgeFrom(j + 1);
               ++edgeIndex) {
            enqueue<true>(edgeIndex, i, n, j);
          }
        }
      }

      const size_t offset = splitEventGraph.numberOfLocalEdges();

      // transfers with footpath
      for (size_t i = roundBegin; i < roundEnd; ++i) {
        TripLabel &label = queue[i];
        for (StopEventId j = label.begin; j < label.end; ++j) {
          const auto &fromEvent = data.arrivalEvents[j];

          if (data.arrivalEvents[j].arrivalTime >
              getTargetLabel(data.arrivalEvents[j].stop, n).arrivalTime) {
            continue;
          }

          for (auto edgeIndex = splitEventGraph.beginTransferEdgeFrom(j);
               edgeIndex < splitEventGraph.beginTransferEdgeFrom(j + 1);
               ++edgeIndex) {
#ifdef ENABLE_PREFETCH
            if (edgeIndex + 4 < splitEventGraph.beginTransferEdgeFrom(j + 1)) {
              __builtin_prefetch(
                  &(data.arrivalEvents[splitEventGraph
                                           .toTransferVertex[edgeIndex + 4]]
                        .stop));
            }
#endif

            const auto toStopEvent =
                splitEventGraph.toTransferVertex[edgeIndex];

            assert(data.arrivalEvents[toStopEvent].stop != fromEvent.stop);
            assert(edgeIndex < splitEventGraph.transferTime.size());
            if (fromEvent.arrivalTime +
                    splitEventGraph.transferTime[edgeIndex] >
                getTargetLabel(data.arrivalEvents[toStopEvent].stop, n)
                    .arrivalTime) {
              continue;
            }

            enqueue<false>(offset + edgeIndex, i, n, j);
          }
        }
      }
      roundBegin = roundEnd;
      roundEnd = queueSize;

      ++n;
    }
  }

  inline bool discard(const TripId trip, const StopIndex index,
                      const int n = 1) {
    if (runReachedIndex.alreadyReached(trip, index)) [[likely]]
      return true;
    if (profileReachedIndex(trip, 1) < index) [[likely]]
      return true;
    if (n > 1 && profileReachedIndex.alreadyReached(trip, index, n)) [[likely]]
      return true;
    const TripId prevTrip = previousTripLookup[trip];
    return (prevTrip != trip &&
            profileReachedIndex.alreadyReached(prevTrip, index, n + 1));
  }

  inline void enqueue(const TripId trip, const StopIndex index) noexcept {
    AssertMsg(data.isTrip(trip), "Trip " << trip << " is not a valid trip!");
    if (discard(trip, index)) return;
    const StopEventId firstEvent = data.firstStopEventOfTrip[trip];
    queue[queueSize] =
        TripLabel(StopEventId(firstEvent + index),
                  StopEventId(firstEvent + runReachedIndex(trip)));
    ++queueSize;
    AssertMsg(queueSize <= queue.size(), "Queue is overfull!");

    runReachedIndex.update(trip, index);
    profileReachedIndex.update(trip, index, 1);

    parentOfTrip.setElement(
        1, trip,
        std::make_tuple(
            data.getStopOfStopEvent(StopEventId(firstEvent + index - 1)),
            noEdge, false));
  }

  template <bool IS_LOCAL_TRANSFER = true>
  inline void enqueue(const size_t edge, const size_t parent, const u_int8_t n,
                      const StopEventId fromStopEventId) noexcept {
    AssertMsg(0 < n, "trips can only be entered in rounds > 0");
    AssertMsg(edge < edgeLabels.size(),
              "Edge ( " << (edge) << " ) is not valid!");
    const EdgeLabel &label = edgeLabels[edge];

    AssertMsg(label.stopEvent < data.numberOfStopEvents(),
              "Event " << label.stopEvent << "  is not valid!");
    AssertMsg(data.isTrip(label.trip),
              "Trip " << label.trip << " is not valid!");
    AssertMsg(label.firstEvent < data.numberOfStopEvents(),
              "Firstevent " << label.firstEvent << " is not valid!");

    if (discard(label.trip, StopIndex(label.stopEvent - label.firstEvent), n))
      return;

    const StopId fromStop = data.getStopOfStopEvent(fromStopEventId);
    AssertMsg(data.isStop(fromStop), "From Stop while enqueuing is not valid!");

    queue[queueSize] = TripLabel(
        label.stopEvent,
        StopEventId(label.firstEvent + runReachedIndex(label.trip)), parent);
    ++queueSize;
    AssertMsg(queueSize <= queue.size(), "Queue is overfull!");

    runReachedIndex.update(label.trip,
                           StopIndex(label.stopEvent - label.firstEvent));
    profileReachedIndex.update(
        label.trip, StopIndex(label.stopEvent - label.firstEvent), n + 1);

    // (IS_LOCAL_TRANSFER) => edge < splitEventGraph.numLocalEdges
    AssertMsg(!IS_LOCAL_TRANSFER || edge < splitEventGraph.numLocalEdges,
              "Given ege should be a local edge!");

    // (!IS_LOCAL_TRANSFER) => edge (which contains offset) <
    // splitEventGraph.numTransferEdges
    AssertMsg(IS_LOCAL_TRANSFER || edge - (splitEventGraph.numLocalEdges) <
                                       splitEventGraph.numTransferEdges,
              "Given edge should be a transfer edge!");

    parentOfTrip.setElement(n + 1, label.trip,
                            std::make_tuple(fromStop, edge, IS_LOCAL_TRANSFER));
  }

  inline bool addArrival(const StopId stop, const int newArrivalTime,
                         const int newDepartureTime, const u_int8_t n,
                         const TripId trip, const StopEventId j) noexcept {
    AssertMsg(n < 16, "N is out of bounds!");
    TargetLabel &currentLabel = getTargetLabel(stop, n);
    bool prune = (newArrivalTime == currentLabel.arrivalTime &&
                  currentLabel.departureTime == newDepartureTime);
    prune |= (newArrivalTime > currentLabel.arrivalTime);
    prune |=
        (n > 0 && newArrivalTime >= getTargetLabel(stop, n - 1).arrivalTime);

    if (prune) [[likely]]
      return false;

    currentLabel.arrivalTime = newArrivalTime;
    currentLabel.departureTime = newDepartureTime;

    markTargetLabelAsChanged(stop, n);

    stopsToUpdate.insert(stop);

#pragma unroll GCC(4)
    for (int i = n + 1; i < 16; ++i) {
      auto &thisLabel = getTargetLabel(stop, i);
      bool skip = (thisLabel.arrivalTime <= newArrivalTime);

      thisLabel.arrivalTime = (!skip ? newArrivalTime : thisLabel.arrivalTime);
      thisLabel.departureTime =
          (!skip ? newDepartureTime : thisLabel.departureTime);
    }

    // setting the parent information
    // (trip, R, J) <=> Trip[R:J]
    const auto R = StopIndex(runReachedIndex(trip) - 1);
    const auto J = data.indexOfStopEvent[j];

    AssertMsg(R <= J, "Trip Segement invalid as R > J?");
    parentOfStop.setElement(n, stop, std::make_tuple(trip, R, J));

    return true;
  }

  inline void getJourneyAndUnwind(const StopId target, int n,
                                  const int targetCell) noexcept {
    AssertMsg(data.isStop(target), "Target is not a stop!");
    AssertMsg(0 < n, "n should be > 0!");
    AssertMsg(n < 16, "n is out of bounds!");
    AssertMsg(isTargetLabelMarkedAsChanged(target, n),
              "Checking on a 'not-updated' target!");

    StopId prevStop = target;

    while (n > 1) {
      auto [trip, left, right] = parentOfStop.getElement(n, prevStop);
      AssertMsg(data.isTrip(trip), "Trip " << trip << " is not valid!");
      AssertMsg(left <= right, "Trip Bounds are not valid!");

      auto [prevStopLocal, edge, local] = parentOfTrip.getElement(n, trip);
      assert(edge < uint8Flags.size());
      uint8Flags[edge][targetCell] = 1;

      prevStop = prevStopLocal;

      AssertMsg(data.isStop(prevStop), "Previous Stop is not valid!");
      n--;
    }
  }

  TargetLabel &getTargetLabel(const StopId stop, const int n) noexcept {
    AssertMsg(n < 16, "N is out of bounds!");

    return targetLabels[stop * 16 + n];
  }

  void markTargetLabelAsChanged(const StopId stop, const int n) noexcept {
    AssertMsg(n < 16, "N is out of bounds!");
    targetLabelChanged[stop * 16 + n] = 1;
  }

  bool isTargetLabelMarkedAsChanged(const StopId stop, const int n) noexcept {
    AssertMsg(n < 16, "N is out of bounds!");
    return static_cast<bool>(targetLabelChanged[stop * 16 + n] > 0);
  }

 private:
  Data &data;
  const SplitStopEventGraph &splitEventGraph;

  std::vector<std::vector<uint8_t>> &uint8Flags;

  int numberOfPartitions;
  std::vector<int> transferFromSource;
  StopId lastSource;

  IndexedSet<false, RouteId> reachedRoutes;

  std::vector<TripLabel> queue;
  size_t queueSize;

#ifdef USE_SIMD
  ProfileReachedIndexSIMD profileReachedIndex;
#else
  ProfileReachedIndex profileReachedIndex;
#endif

  ReachedIndex runReachedIndex;

  // for every stop and every transfer (16)
  std::vector<TargetLabel> targetLabels;
  std::vector<uint8_t> targetLabelChanged;
  /* std::vector<std::vector<TargetLabel>> targetLabels; */
  /* std::vector<std::vector<bool>> targetLabelChanged; */
  /* std::vector<TargetLabel> emptyTargetLabels; */
  /* std::vector<bool> emptyTargetLabelChanged; */

  // check which stops need to be updated during one round
  IndexedSet<false, StopId> stopsToUpdate;

  std::vector<EdgeLabel> edgeLabels;

  StopId sourceStop;

  std::vector<std::vector<TripStopIndex>> &collectedDepTimes;

  // parents
  // holds stop, edge and whether it was local or transfer egde
  Parent<std::tuple<StopId, size_t, bool>> parentOfTrip;
  Parent<std::tuple<TripId, StopIndex, StopIndex>> parentOfStop;

  // this is to get the edge and the fromStopEventId  of a triplabel faster
  std::vector<std::pair<size_t, StopEventId>> tripLabelEdge;

  std::vector<TripBased::RouteLabel> &routeLabels;
  std::vector<TripId> previousTripLookup;

  int timestamp;
};

}  // namespace TripBased
