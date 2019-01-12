#include <sidechain/utils.hpp>

namespace sidechain {

bytes parse_hex( const std::string& str )
{
    bytes vec( str.size() / 2 );
    fc::from_hex( str, vec.data(), vec.size() );
    return vec;
}

}
