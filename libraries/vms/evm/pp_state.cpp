#include "pp_state.hpp"
#include <boost/optional.hpp>

namespace vms { namespace evm {

std::pair<uint64_t, uint64_t> address_to_id( const Address& addr )
{
    std::string address( addr.hex() );
    std::string prefix( address.begin(), address.begin() + 2 );
    std::string data( address.begin() + 2, address.end() );

    uint64_t value, type;
    std::stringstream converter( data );
    converter >> std::hex >> value;
    converter = std::stringstream( prefix );
    converter >> std::hex >> type;

    return std::make_pair( type, value );
}

Address id_to_address( const uint64_t& id, const uint64_t& type )
{
    // 0 - account, 1 - contract
    assert( type == 0 || type == 1 );

    Address addr( id );
    addr[0] = type;
    return addr;
}

std::vector<std::string> get_all_matches(const std::string& str, const std::regex& regex) {
    std::smatch sm;
    std::string::const_iterator iBegin = str.begin();
    std::string::const_iterator iEnd = str.end();
    std::vector<std::string> result;
    for(; std::regex_search(iBegin, iEnd, sm, regex); iBegin = sm[0].second) {
        result.push_back(sm[0]);
    }
    return result;
}

boost::optional<uint64_t> create_object_id_type(const std::string& location)
{
   boost::optional<uint64_t> value;
   std::regex regex_id("^([0-9]+\\.[0-9]+\\.[0-9]+)");
   std::smatch sm;
   if(std::regex_search(location, sm, regex_id)){
      std::regex regex_number("[0-9]+");
      std::string id = sm.str();
      std::vector<std::string> numbers_id = get_all_matches(id, regex_number);
      if(numbers_id.size() != 3) {
         return value;
      }

      //   return object_id_type(string_to_uint64(numbers_id[0]), string_to_uint64(numbers_id[1]), string_to_uint64(numbers_id[2]));
      value = std::stoll( numbers_id[2] );
      return value;
   }
   return value;
}

pp_state::pp_state( fs::path data_dir, vms::evm::evm_adapter _adapter ) : State(u256(0), State::openDB((data_dir / "eth_db").string(), sha3(dev::rlp("")))), adapter(_adapter)
{
   setRoot(dev::sha3(dev::rlp("")));
   author = id_to_address(0, 1);

}

u256 pp_state::balance(Address const& _id) const
{
   auto obj_id = address_to_id(_id);
   if( obj_id.first ) {
      return u256( adapter.get_contract_balance( obj_id.second, asset_id ) );
   } else {
      return u256( adapter.get_account_balance( obj_id.second, asset_id ) );
   }
}

u256 pp_state::balance(Address const& _id, const u256& _callIdAsset) const
{
   auto obj_id = address_to_id(_id);
   if( obj_id.first ) {
      return u256( adapter.get_contract_balance( obj_id.second, static_cast<uint64_t>( _callIdAsset ) ) );
   } else {
     return u256( adapter.get_account_balance( obj_id.second, static_cast<uint64_t>( _callIdAsset ) ) );
   }
   return u256(0);
}

void pp_state::createContract(Address const& _address)
{
   uint64_t next_id = adapter.get_next_contract_id();
   Address addr = id_to_address( next_id, 1 );
   const_cast<Address&>(_address) = std::move(addr);

   createAccount(addr, {requireAccountStartNonce(), 0});
}

void pp_state::addBalance(Address const& _id, u256 const& _amount)
{
   if( _id == author ){
      fee = _amount;
   }

   auto receiver_id = address_to_id( _id );
   if ( receiver_id.first ) {
      if( !adapter.is_there_contract( receiver_id.second ) ) {
         adapter.create_contract_obj();
      }
      adapter.add_contract_balance( receiver_id.second, asset_id, static_cast<int64_t>( _amount ) );
   } else {
      // try {
      //    auto account = account_id_type( receiver_id )( bts_db );
      // } FC_CAPTURE_AND_RETHROW( ("transfer to invalid account") )
      if( !adapter.is_there_account( receiver_id.second ) ) {
         throw;
      }
      adapter.add_account_balance( receiver_id.second, asset_id, static_cast<int64_t>( _amount ) );
   }
}

void pp_state::subBalance(Address const& _id, u256 const& _amount)
{
   auto obj_id = address_to_id( _id );
   if( obj_id.first ) {
      adapter.sub_contract_balance( obj_id.second, asset_id, static_cast<int64_t>( _amount ) );
   } else {
      adapter.sub_account_balance( obj_id.second, asset_id, static_cast<int64_t>( _amount ) );
   }
}

void pp_state::transferBalance( Address const& _from, Address const& _to, u256 const& _value )
{
   transfers_stack.push(TransfersStruct(_from, _to, _value));
   subBalance(_from, _value);
   addBalance(_to, _value);
}

void pp_state::transferBalance(Address const& _from, Address const& _to, u256 const& _value, u256 const& _idAsset)
{
   uint64_t save_id = asset_id;
   asset_id = static_cast<uint64_t>(_idAsset);
   transferBalance(_from, _to, _value);
   asset_id = save_id;
}

void pp_state::transferBalanceSuicide(Address const& _from, Address const& _to)
{
   transfers_stack.push(TransfersStruct(_from, _to, balance(_from)));
   addBalance(_to, balance(_from));
   subBalance(_from, balance(_from));
}

void pp_state::incNonce(Address const& _addr)
{
   if (Account* a = account(_addr))
   {
      auto oldNonce = a->nonce();
      a->incNonce();
      m_changeLog.emplace_back(_addr, oldNonce);
   }
}

void pp_state::commit(CommitBehaviour _commitBehaviour)
{
   result_accounts = m_cache;
   State::commit(_commitBehaviour);
}

void pp_state::revertStack() 
{
   while( transfers_stack.size() > stack_size )
      transfers_stack.pop();
}

void pp_state::publishContractTransfers() 
{
   while( transfers_stack.size() )
   {
      TransfersStruct transfer = transfers_stack.top();
      transfers_stack.pop();
      auto from = address_to_id( transfer.from );
      if( from.first )
         adapter.publish_contract_transfer( address_to_id( transfer.from ), address_to_id( transfer.to ), asset_id, static_cast<int64_t>(transfer.value) );
   }
}

void pp_state::clear_temporary_variables() {
   stack_size = 0;
   size_t size = transfers_stack.size();
   for( size_t i = 0; i < size; i++ ) {
      transfers_stack.pop();
   }
   fee = 0;
   author = Address();
   asset_id = 0;
   result_accounts.clear();
}

/////////////////////////////////////////////////////////////////////////////

dev::Address pp_state::getNewAddress() const
{
   return id_to_address( adapter.get_next_contract_id(), 1 );
}

} }
