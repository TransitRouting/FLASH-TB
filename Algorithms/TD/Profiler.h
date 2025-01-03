#pragma once

#include <iostream>
#include <vector>

#include "../../Helpers/String/String.h"
#include "../../Helpers/Timer.h"

namespace TD {

typedef enum {
    PHASE_CLEAR,
    PHASE_RUN,
    NUM_PHASES
} Phase;

constexpr const char* PhaseNames[] = {"Clear Dijkstra               ", "Work in PQ                   "};

typedef enum {
    METRIC_SEETLED_VERTICES,
    METRIC_RELAXED_TRANSFER_EDGES,
    METRIC_RELAXED_ROUTE_EDGES,
    METRIC_FOUND_SOLUTIONS,
    NUM_METRICS
} Metric;

constexpr const char* MetricNames[] = {"# Settled Vertices           ", "# Relaxed Transfer Edges     ",
                                       "# Relaxed Route Edges        ", "# Solutions                  ",
                                       "# Added Labels into bags     "};

class NoProfiler {
public:
    inline void registerPhases(const std::initializer_list<Phase>&) const noexcept {}
    inline void registerMetrics(const std::initializer_list<Metric>&) const noexcept {}

    inline void start() const noexcept {}
    inline void done() const noexcept {}

    inline void startPhase() const noexcept {}
    inline void donePhase(const Phase) const noexcept {}

    inline void countMetric(const Metric) const noexcept {}

    inline void printStatistics() const noexcept {}
};

class AggregateProfiler : public NoProfiler {
public:
    AggregateProfiler() : totalTime(0.0), phaseTime(NUM_PHASES, 0.0), metricValue(NUM_METRICS, 0), numQueries(0) {}

    inline void registerPhases(const std::initializer_list<Phase>& phaseList) noexcept {
        for (const Phase phase : phaseList) {
            phases.push_back(phase);
        }
    }

    inline void registerMetrics(const std::initializer_list<Metric>& metricList) noexcept {
        for (const Metric metric : metricList) {
            metrics.push_back(metric);
        }
    }

    inline void start() noexcept { totalTimer.restart(); }

    inline void done() noexcept {
        totalTime += totalTimer.elapsedMicroseconds();
        numQueries++;
    }

    inline void startPhase() noexcept { phaseTimer.restart(); }

    inline void donePhase(const Phase phase) noexcept { phaseTime[phase] += phaseTimer.elapsedMicroseconds(); }

    inline void countMetric(const Metric metric) noexcept { metricValue[metric]++; }

    inline double getTotalTime() const noexcept { return totalTime / numQueries; }

    inline double getPhaseTime(const Phase phase) const noexcept { return phaseTime[phase] / numQueries; }

    inline double getMetric(const Metric metric) const noexcept {
        return metricValue[metric] / static_cast<double>(numQueries);
    }

    inline void printStatistics() const noexcept {
        for (const Metric metric : metrics) {
            std::cout << MetricNames[metric] << ": "
                      << String::prettyDouble(metricValue[metric] / static_cast<double>(numQueries), 2) << std::endl;
        }
        for (const Phase phase : phases) {
            std::cout << PhaseNames[phase] << ": "
                      << String::musToString(phaseTime[phase] / static_cast<double>(numQueries)) << std::endl;
        }
        std::cout << "Total Time                   : " << String::musToString(totalTime / numQueries) << std::endl;
    }

private:
    Timer totalTimer;
    double totalTime;
    std::vector<Phase> phases;
    std::vector<Metric> metrics;
    Timer phaseTimer;
    std::vector<double> phaseTime;
    std::vector<long long> metricValue;
    size_t numQueries;
};

} // namespace TD