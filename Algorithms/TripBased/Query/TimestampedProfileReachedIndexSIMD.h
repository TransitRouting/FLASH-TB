#pragma once

#include "ProfileReachedIndexSIMD.h"
#include <cassert>
#include <emmintrin.h>
#include <vector>

#include "../../../DataStructures/TripBased/Data.h"
#include "../../../Helpers/aligned_allocator.h"

namespace TripBased {

/* static constexpr const __m128i MAX_MASKS[16] = { 0x0000000000000000, */
/*     0x0000000000000000, */
/*     0x00000000000000FF, */
/*     0x0000000000000000, */
/*     0x000000000000FFFF, */
/*     0x0000000000000000, */
/*     0x0000000000FFFFFF, */
/*     0x0000000000000000, */
/*     0x00000000FFFFFFFF, */
/*     0x0000000000000000, */
/*     0x000000FFFFFFFFFF, */
/*     0x0000000000000000, */
/*     0x0000FFFFFFFFFFFF, */
/*     0x0000000000000000, */
/*     0x00FFFFFFFFFFFFFF, */
/*     0x0000000000000000, */
/*     -1, */
/*     0x0000000000000000, */
/*     -1, */
/*     0x00000000000000FF, */
/*     -1, */
/*     0x000000000000FFFF, */
/*     -1, */
/*     0x0000000000FFFFFF, */
/*     -1, */
/*     0x00000000FFFFFFFF, */
/*     -1, */
/*     0x000000FFFFFFFFFF, */
/*     -1, */
/*     0x0000FFFFFFFFFFFF, */
/*     -1, */
/*     0x00FFFFFFFFFFFFFF }; */

//! Allows to check whether we already reached a certain point in a route / trip
//! / position given a number of rounds. Lookup is fast, but updating is slow.
//! This ReachedIndex is used for the TB::ProfileQuery. It uses SIMD intrisics
//! to allow for fast updates.
class TimestampedProfileReachedIndexSIMD {
private:
    //! This union holds the values (aligned to use SIMD intrisics)
    union alignas(16) ReachedElement {
        ReachedElement() {}
        __m128i mValues;
        u_int8_t values[16];
    };

public:
    TimestampedProfileReachedIndexSIMD(const Data& data)
        : data(data),
          defaultLabels(data.numberOfTrips()),
          labels(data.numberOfTrips()),
          timestamps(data.numberOfTrips(), 0),
          timestamp(0) {
        for (TripId trip(0); trip < data.numberOfTrips(); ++trip) {
            std::fill(std::begin(defaultLabels[trip].values), std::end(defaultLabels[trip].values),
                      data.numberOfStopsInTrip(trip));
        }
    };

    inline void clear() noexcept {
        if (timestamp == 0) {
            labels = defaultLabels;
            std::fill(timestamps.begin(), timestamps.end(), 0);
        }
        ++timestamp;
    }

    inline bool alreadyReached(const TripId trip, const u_int8_t position, const uint8_t round = 1) noexcept {
        assert(data.isTrip(trip));
        assert(0 < round);
        assert(round < 16);

        return getPosition(trip, round) <= position;
    }

    inline void update(const TripId trip, const u_int8_t position, const uint8_t round = 1) noexcept {
        assert(data.isTrip(trip));
        assert(0 < round);
        assert(round < 16);

        __m128i mask = MAX_MASKS[round - 1];

        const __m128i FILTER = _mm_max_epu8(_mm_set1_epi8(position), mask);

        // Iterate over all trips either until the last trip OR if we already have a
        // trip with a position at least as good
        for (TripId tr(trip);
             tr < data.firstTripOfRoute[data.routeOfTrip[trip] + 1] && getPosition(tr, round) > position; ++tr)
            labels[tr].mValues = _mm_min_epu8(labels[tr].mValues, FILTER);
    }

    inline u_int8_t& operator()(const TripId trip, const uint8_t round = 1) noexcept {
        return getPosition(trip, round);
    }

private:
    inline u_int8_t& getPosition(const TripId trip, const uint8_t round = 1) noexcept {
        if (timestamps[trip] != timestamp) {
            labels[trip] = defaultLabels[trip];
            timestamps[trip] = timestamp;
        }
        return labels[trip].values[round - 1];
    }

    //! Returns the filter mask to use
    inline __m128i getMask(const uint8_t round = 1) const noexcept {
        assert(0 < round);
        assert(round < 16);

        return MAX_MASKS[round];
    }

    const Data& data;

    std::vector<ReachedElement, aligned_allocator<ReachedElement, alignof(ReachedElement)>> defaultLabels;
    std::vector<ReachedElement, aligned_allocator<ReachedElement, alignof(ReachedElement)>> labels;

    std::vector<u_int16_t> timestamps;
    u_int16_t timestamp;
};

} // namespace TripBased
