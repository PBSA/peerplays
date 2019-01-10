#include <sidechain/utils.hpp>

namespace sidechain {

std::vector<char> parse_hex( const std::string& str )
{
    std::vector<char> vec( str.size() / 2 );
    fc::from_hex( str, vec.data(), vec.size() );
    return vec;
}

}
