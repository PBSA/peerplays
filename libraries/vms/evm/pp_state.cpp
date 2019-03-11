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

   db_res = State::openDB((data_dir / "result_db").string(), sha3(rlp("")), WithExisting::Trust);
	state_results = SecureTrieDB<h160, OverlayDB>(&db_res);
   setRootResults(dev::sha3(dev::rlp("")));
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

u256 pp_state::balance(Address const& _id, const std::string& _callIdAsset) const
{
   auto idAssetTemp = create_object_id_type(_callIdAsset);
   if( idAssetTemp ) {
      auto obj_id = address_to_id(_id);
      if( obj_id.first ) {
         return u256( adapter.get_contract_balance( obj_id.second, asset_id ) );
      } else {
         return u256( adapter.get_account_balance( obj_id.second, asset_id ) );
      }
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
void pp_state::addResult( const std::string& id, std::pair<dev::eth::ExecutionResult, dev::eth::TransactionReceipt>& res)
{
   cache_results.insert(std::make_pair(right160(sha3(id)), res));
}

std::pair<ExecutionResult, TransactionReceipt> const* pp_state::getResult( const std::string& _id ) const
{
   return const_cast<pp_state*>(this)->getResult(right160(sha3(_id)), true);
}

std::pair<ExecutionResult, TransactionReceipt>* pp_state::getResult(h160 const& idResult, bool a)
{
   std::pair<ExecutionResult, TransactionReceipt>* result = nullptr;
   auto it = cache_results.find(idResult);
	if (it != cache_results.end())
   	result = &it->second;
	else
	{
		// populate basic info.
		std::string stateBack = state_results.at(idResult);
		if (stateBack.empty())
			return nullptr;

		RLP state(stateBack);
      ExecutionResult res;
      res.gasUsed = state[0].toInt<u256>();
      res.excepted = static_cast<TransactionException>(state[1].toInt<uint32_t>());
      res.newAddress = state[2].toHash<h160>();
      res.output = state[3].toBytes();
      res.codeDeposit = static_cast<CodeDeposit>(state[4].toInt<uint32_t>());
      res.gasRefunded = state[5].toInt<u256>();
      res.depositSize = state[6].toInt<unsigned>();
      res.gasForDeposit = state[7].toInt<u256>();

      LogEntries log;
      std::vector<dev::bytes> logEntries = state[10].toVector<dev::bytes>();
      for(size_t i = 0; i < logEntries.size(); i++){
         RLP stateTemp(logEntries[i]);
         log.push_back(LogEntry(stateTemp));
      }
        
      TransactionReceipt tr(state[8].toInt<uint8_t>(), state[9].toInt<u256>(), log);
      cache_results.insert(std::make_pair(idResult, std::make_pair(res, tr)));
        
      result = &cache_results.at(idResult);
	}
	return result;
}

void pp_state::commitResults()
{
   ethbit::commit(cache_results, state_results);
   cache_results.clear();
}

dev::Address pp_state::getNewAddress() const
{
   return id_to_address( adapter.get_next_contract_id(), 1 );
}

} }
