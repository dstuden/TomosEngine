#include "Tomos/util/random/TRandom.hh"

#include <utility>

namespace Tomos
{
    namespace
    {
        uint64_t seedFromDevice()
        {
            std::random_device rd;
            const uint64_t    high = static_cast<uint64_t>( rd() ) << 32;
            const uint64_t    low  = static_cast<uint64_t>( rd() );
            return high | low;
        }
    }  // namespace

    TRandom::TRandom() : TRandom( seedFromDevice() ) {}

    TRandom::TRandom( uint64_t p_seed ) { setSeed( p_seed ); }

    void TRandom::setSeed( uint64_t p_seed )
    {
        m_seed = p_seed;
        m_engine.seed( p_seed );
    }

    int32_t TRandom::nextInt( int32_t p_min, int32_t p_max )
    {
        if ( p_min > p_max ) std::swap( p_min, p_max );
        std::uniform_int_distribution<int32_t> dist( p_min, p_max );
        return dist( m_engine );
    }

    uint32_t TRandom::nextUInt( uint32_t p_min, uint32_t p_max )
    {
        if ( p_min > p_max ) std::swap( p_min, p_max );
        std::uniform_int_distribution<uint32_t> dist( p_min, p_max );
        return dist( m_engine );
    }

    float TRandom::nextFloat()
    {
        std::uniform_real_distribution<float> dist( 0.0f, 1.0f );
        return dist( m_engine );
    }

    float TRandom::nextFloat( float p_min, float p_max )
    {
        if ( p_min > p_max ) std::swap( p_min, p_max );
        std::uniform_real_distribution<float> dist( p_min, p_max );
        return dist( m_engine );
    }

    double TRandom::nextDouble()
    {
        std::uniform_real_distribution<double> dist( 0.0, 1.0 );
        return dist( m_engine );
    }

    bool TRandom::nextBool( float p_probability )
    {
        if ( p_probability <= 0.0f ) return false;
        if ( p_probability >= 1.0f ) return true;
        return nextFloat() < p_probability;
    }

    TRandom& TRandom::global()
    {
        static TRandom sInstance;
        return sInstance;
    }
}  // namespace Tomos
