#include "pp_state.hpp"
#include <libevm/VMFace.h>

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

void pp_state::addBalance(Address const& _id, u256 const& _amount)
{
   auto receiver_id = address_to_id( _id );
   if ( receiver_id.first && !adapter.is_there_contract( receiver_id.second ) ) {
      adapter.create_contract_obj( allowed_assets );
      m_changeLog.emplace_back(Change::Create, _id);
   } else if( !adapter.is_there_account( receiver_id.second ) ) {
      throw;
   }

	u256 init_balance = balance( _id, asset_id );
   adapter.change_balance( receiver_id, asset_id, static_cast<int64_t>( _amount ) );
   m_changeLog.emplace_back(Change::Balance, _id, init_balance, asset_id);
}

void pp_state::subBalance(Address const& _id, u256 const& _amount)
{
   u256 init_balance = balance( _id, asset_id );

   auto obj_id = address_to_id( _id );

   adapter.change_balance( obj_id, asset_id, 0 - static_cast<int64_t>( _amount ) );
   m_changeLog.emplace_back(Change::Balance, _id, init_balance, asset_id);
}

void pp_state::transferBalance( Address const& _from, Address const& _to, u256 const& _value )
{
   const auto& assets = adapter.get_allowed_assets( address_to_id( _to ).second );
   if( !assets.empty() && _value != 0 && assets.count( asset_id ) == 0 ) {
      BOOST_THROW_EXCEPTION(dev::eth::ProhibitedAssetType());
   }

   const auto& sender = address_to_id( _from );
   if( sender.first == 1 ) {
      const auto& assets = adapter.get_allowed_assets( sender.second );
      if( !assets.empty() ) {
         set_allowed_assets( assets );
      }
   }

   transfers_stack.push(TransfersStruct(_from, _to, _value));
   m_changeLog.emplace_back(Change::TransferStack, _from);
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
   const auto& sender_balance = balance(_from);
   const auto& assets = adapter.get_allowed_assets( address_to_id( _to ).second );
   if( !assets.empty() && sender_balance != 0 && assets.count( asset_id ) == 0 ) {
      BOOST_THROW_EXCEPTION(dev::eth::ProhibitedAssetType());
   }

   transfers_stack.push(TransfersStruct(_from, _to, sender_balance));
	m_changeLog.emplace_back(Change::TransferStack, _from);
   addBalance(_to, sender_balance);
   subBalance(_from, sender_balance);
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
   size_t size = transfers_stack.size();
   for( size_t i = 0; i < size; i++ ) {
      transfers_stack.pop();
   }
   fee = 0;
   asset_id = 0;
   result_accounts.clear();
   allowed_assets.clear();
}

void pp_state::rollback(size_t _savepoint)
{
    while (_savepoint != m_changeLog.size())
    {
        auto& change = m_changeLog.back();
        auto& account = m_cache[change.address];

        // Public State API cannot be used here because it will add another
        // change log entry.
        switch (change.kind)
        {
        case Change::Storage:
            account.setStorage(change.key, change.value);
            break;
        case Change::StorageRoot:
            account.setStorageRoot(change.value);
            break;
        case Change::Balance: {
            u256 acc_balance = balance(change.address, change.key);
            auto obj_id = address_to_id( change.address );
            if( acc_balance > change.value )
               adapter.change_balance( obj_id, static_cast<uint64_t>(change.key), 0 - static_cast<int64_t>( acc_balance - change.value ) );
            else
               adapter.change_balance( obj_id, static_cast<uint64_t>(change.key), static_cast<int64_t>( change.value - acc_balance ) );
            break;
        }
        case Change::Nonce:
            account.setNonce(change.value);
            break;
        case Change::Create:
            m_cache.erase(change.address);
            adapter.delete_contract_obj( address_to_id( change.address ).second );
            break;
        case Change::Code:
            account.setCode(std::move(change.oldCode));
            break;
        case Change::Touch:
            account.untouch();
            m_unchangedCacheEntries.emplace_back(change.address);
            break;
		case Change::TransferStack:
            transfers_stack.pop();
            break;
        }
        m_changeLog.pop_back();
    }
}

/////////////////////////////////////////////////////////////////////////////

dev::Address pp_state::getNewAddress() const
{
   return id_to_address( adapter.get_next_contract_id(), 1 );
}

} }
