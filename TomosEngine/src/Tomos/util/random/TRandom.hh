#pragma once

#include <algorithm>
#include <cstdint>
#include <random>

namespace Tomos
{
    // Value-type RNG. Not thread-safe — use a local instance per thread if needed.
    // Prefer TRandom::global() for non-reproducible helpers; own seeded instances for gameplay/procgen.
    class TRandom
    {
    public:
        TRandom();
        explicit TRandom( uint64_t p_seed );

        void     setSeed( uint64_t p_seed );
        uint64_t seed() const { return m_seed; }

        // Ints are inclusive on both ends; floats are in [min, max).
        int32_t  nextInt( int32_t p_min, int32_t p_max );
        uint32_t nextUInt( uint32_t p_min, uint32_t p_max );
        float    nextFloat();
        float    nextFloat( float p_min, float p_max );
        double   nextDouble();
        bool     nextBool( float p_probability = 0.5f );

        template<typename It>
        void shuffle( It p_first, It p_last )
        {
            std::shuffle( p_first, p_last, m_engine );
        }

        static TRandom& global();

    private:
        uint64_t         m_seed = 0;
        std::mt19937_64  m_engine;
    };
}  // namespace Tomos
