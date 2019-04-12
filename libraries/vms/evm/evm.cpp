#include <evm.hpp>
#include <evm_result.hpp>

#include <fc/crypto/hex.hpp>

#include <aleth/libethcore/SealEngine.h>
#include <aleth/libethereum/ChainParams.h>
#include <aleth/libethashseal/GenesisInfo.h>
#include <aleth/libethashseal/Ethash.h>

namespace vms { namespace evm {

evm::evm( const std::string& path, adapters adapter ) : vm_interface( adapter ),
                                                        state( fs::path( path ), adapter.get<evm_adapter>() )
{
   dev::eth::Ethash::init();
   se = std::unique_ptr< SealEngineFace >( dev::eth::ChainParams(dev::eth::genesisInfo(dev::eth::Network::PeerPlaysNetwork)).createSealEngine() );
   set_state_root( dev::sha3( dev::rlp("") ).hex() );
}

std::pair<uint64_t, bytes> evm::exec( const bytes& data)
{ 
   clear_temporary_variables();

   last_block_hashes last_hashes( get_adapter().get_last_block_hashes() );
   OnOpFunc const& _onOp = OnOpFunc();
   EnvInfo ei = create_environment( last_hashes );

   eth_op eth = fc::raw::unpack<eth_op>( data );
   Transaction tx = create_eth_transaction( eth );
   auto permanence = get_adapter().evaluating_from_block() ? Permanence::Committed : Permanence::Reverted;
   size_t savepoint = state.savepoint();

   state.setAssetType( eth.asset_id_gas.instance.value );
	state.set_allowed_assets( eth.allowed_assets );

   if( !get_adapter().evaluating_from_block() ) {
      state.addBalance( tx.sender(), tx.gas() * tx.gasPrice() );
      state.addBalance( tx.sender(), tx.value(), eth.asset_id_transfer.instance.value );
   }

   std::pair< ExecutionResult, TransactionReceipt > res;
   try {
      res = state.execute(ei, *se.get(), tx, permanence, _onOp);
   }
   catch ( const dev::Exception& ex ) {
      res.first.excepted = toTransactionException(ex);
   }

   evm_result result { res.first, res.second, get_state_root() };
   bytes serialize_result = fc::raw::unsigned_pack( result );

   if( get_adapter().evaluating_from_block() ) {
      finalize( res.first.excepted );
   } else {
      state.rollback(savepoint);
   }

   return std::make_pair( static_cast<uint64_t>( state.getFee() ), serialize_result );
}

void evm::finalize( dev::eth::TransactionException& trx_excp )
{
   state.db().commit();
   state.publishContractTransfers();

   if( trx_excp != TransactionException::None ){
      se->suicideTransfer.clear();
   }

   std::unordered_map<Address, Account> attracted_contr_and_acc = state.getResultAccounts();
   transfer_suicide_balances(se->suicideTransfer);
   delete_balances( attracted_contr_and_acc );

   attracted_contracts = select_attracted_contracts(attracted_contr_and_acc);
}

void evm::roll_back_db( const uint32_t& block_number )
{
   const auto& state_root = get_adapter().get_last_valid_state_root( block_number );
   state_root.valid() ? set_state_root( state_root->str() ) : set_state_root( dev::sha3( dev::rlp("") ).hex() );
}

std::map<uint64_t, bytes> evm::get_contracts( const std::vector<uint64_t>& ids ) const
{
   std::map<uint64_t, bytes> results;
   for( const auto& id : ids ) {
      const auto& address = id_to_address( id, 1 );
      if( state.getAccount( address ) != nullptr ) {
         evm_account_info acc_info{ address, state.storageRoot( address ), state.code( address ), state.storage( address ) };
         results.insert( std::make_pair( id, fc::raw::unsigned_pack( acc_info ) ) );
      } else {
         results.insert( std::make_pair( id, bytes() ) );
      }
   }
   return results;
}

bytes evm::get_code( const uint64_t& id ) const
{
   return state.code( id_to_address( id, 1 ) );
}

std::string evm::get_state_root() const
{
   return state.rootHash().hex();
}

void evm::set_state_root( const std::string& hash )
{
   state.setRoot( h256( hash ) );
}

Transaction evm::create_eth_transaction(const eth_op& eth) const
{   
   bytes code = dev::fromHex( eth.code );

   Transaction tx;
   if( eth.receiver.valid() ) {
       dev::Address rec = id_to_address( eth.receiver->instance.value, 1 );
       tx = Transaction ( u256( eth.value ), u256( eth.gasPrice ), u256( eth.gas ), rec, dev::bytes(code.begin(), code.end()), u256( 0 ) );
   } else {
       tx = Transaction ( u256( eth.value ), u256( eth.gasPrice ), u256( eth.gas ), dev::bytes(code.begin(), code.end()), u256( 0 ) );
   }

    Address sender = id_to_address( eth.registrar.instance.value, 0 );
    tx.forceSender( sender );
    tx.setIdAsset( static_cast<uint64_t>( eth.asset_id_transfer.instance.value ) );

   return tx;
}

EnvInfo evm::create_environment( last_block_hashes const& last_hashes )
{
   BlockHeader header;
   auto current_block = get_adapter().get_current_block();
   header.setNumber( static_cast<int64_t>( current_block.block_num() ) );
   header.setAuthor( id_to_address( get_adapter().get_id_author_block(), 0 ) );
   header.setTimestamp( static_cast<int64_t>( current_block.timestamp.sec_since_epoch() ) );
   header.setGasLimit( get_adapter().get_block_gas_limit() );
   header.setGasUsed( get_adapter().get_block_gas_used() );
   if( current_block.block_num() > 1 )
      header.setParentHash( h256( current_block.previous.str() ) );
   else
      header.setParentHash( h256() );

   EnvInfo result(header, last_hashes, get_adapter().get_block_gas_used());

   return result;
};

void evm::transfer_suicide_balances(const std::vector< std::pair< Address, Address > >& suicide_transfer) {
   for(std::pair< Address, Address > transfer : suicide_transfer){
      auto from = address_to_id( transfer.first );
      auto to = address_to_id( transfer.second );

      auto balances = get_adapter().get_contract_balances( from.second );
      for( auto& balance : balances ) {
         get_adapter().publish_contract_transfer( from, to, balance.second, balance.first );

         get_adapter().change_balance( from, balance.second, 0 - balance.first );
         get_adapter().change_balance( to, balance.second, balance.first );
      }
   }
}

void evm::delete_balances( const std::unordered_map< Address, Account >& accounts )
{
   for( std::pair< Address, Account > acc : accounts ) {
      auto id = address_to_id( acc.first );
      if( id.first == 0 )
         continue;

      if(!acc.second.isAlive()){
         get_adapter().delete_contract_balances( id.second );
         get_adapter().contract_suicide( id.second );
         continue;
      }
   }
}

std::vector< uint64_t > evm::select_attracted_contracts( const std::unordered_map< Address, Account >& accounts ) {
   std::vector< uint64_t > result;
   for( auto& acc : accounts ){
      auto contract = address_to_id( acc.first );
      if( contract.first == 0 )
         continue;

      result.push_back( contract.second );
   }
   return std::move( result );
}

void evm::clear_temporary_variables()
{
   se->suicideTransfer.clear();
   state.clear_temporary_variables();
}

} }
