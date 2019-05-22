#include <wavm.hpp>

namespace vms { namespace wavm {

wavm::wavm( vms::base::adapters adapter ) : vms::base::vm_interface( adapter )
{ }

std::pair<uint64_t, bytes> wavm::exec( const bytes& data)
{ }

std::vector< uint64_t > wavm::get_attracted_contracts( ) const
{ }

void wavm::roll_back_db( const uint32_t& block_number )
{ }

std::map<uint64_t, bytes> wavm::get_contracts( const std::vector<uint64_t>& ids ) const
{ }

bytes wavm::get_code( const uint64_t& id ) const
{ }

eosio::chain::account_name wavm::id_to_wavm_name( const uint64_t& id, const uint64_t& type )
{
    assert( type == 0 || type == 1 );
    std::string prefix = (type == 0 ? "a." : "c.");
    std::string str;

    uint32_t tmp = id;
    do
    {
        uint32_t val = tmp%31;
        char c = charmap[ val ];
        str.push_back(c);
        tmp = tmp/31;
    } while(tmp);

    std::reverse(str.begin(), str.end());

    return eosio::chain::name(prefix + str);
}

std::pair<uint64_t, uint64_t> wavm::wavm_name_to_id( const eosio::chain::account_name acc_name )
{
    uint32_t type, id = 0;
    std::string str_name = acc_name.to_string();

    if( str_name[0] == 'c' )
        type = 1;
    if( str_name[0] == 'a' )
        type = 0;

    str_name.erase(0,2);
    std::reverse( str_name.begin(), str_name.end() );

    for( size_t i = 0; i < str_name.size(); i++ )
    {
        auto n = charmap.find( str_name[i] );
        id += n*pow(31, i);
    }

    return std::make_pair( type, id );
}


} }