#define STB_IMAGE_IMPLEMENTATION
#include "Tomos/util/image/TImageLoad.hh"

#include <stb/stb_image.h>

#include "Tomos/util/path/TPath.hh"

namespace Tomos
{
    TRgbaPixels loadRgbaFile( const std::string& p_path )
    {
        const std::string filePath = TPath::resolveString( p_path );
        int               w = 0, h = 0, ch = 0;
        stbi_uc*          raw = stbi_load( filePath.c_str(), &w, &h, &ch, 4 );
        if ( raw == nullptr ) return {};

        TRgbaPixels out;
        out.m_width  = w;
        out.m_height = h;
        out.m_data.assign( raw, raw + static_cast<size_t>( w ) * static_cast<size_t>( h ) * 4u );
        stbi_image_free( raw );
        return out;
    }
}  // namespace Tomos
