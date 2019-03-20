#pragma once

#include <fc/io/raw.hpp>
#include <boost/filesystem.hpp>
#include <fc/reflect/reflect.hpp>

#include <evm_adapter.hpp>
#include <libethereum/State.h>
#include <utils/json_spirit/JsonSpiritHeaders.h>

#include <regex>

namespace vms { namespace evm {

namespace js = json_spirit;
namespace fs = boost::filesystem;

using namespace dev;
using namespace dev::eth;

using LogEntriesSerializ = std::vector<std::pair<dev::Address, std::pair<dev::h256s, dev::bytes>>>;
using ExecResult = std::unordered_map<h160, std::pair<ExecutionResult, TransactionReceipt>>;

struct TransfersStruct {
    Address from;
    Address to;
    u256 value;
    TransfersStruct() {}
    TransfersStruct(Address _from, Address _to, u256 _value) :
                    from(_from), to(_to), value(_value) {}
};

std::pair<uint64_t, uint64_t> address_to_id( const Address& addr );

Address id_to_address( const uint64_t& id, const uint64_t& type );

class pp_state : public State
{

public:

   pp_state( fs::path data_dir, vms::evm::evm_adapter _adapter );

   u256 balance(Address const& _id) const override;

   u256 balance(Address const& _id, const u256& _callIdAsset) const override;

   void incNonce(Address const& _addr) override;

   void createContract(Address const& _address) override;

   void addBalance(Address const& _id, u256 const& _amount) override;

   void subBalance(Address const& _addr, u256 const& _value) override;

   void transferBalance(Address const& _from, Address const& _to, u256 const& _value) override;

   void transferBalance(Address const& _from, Address const& _to, u256 const& _value, u256 const& _idAsset) override;

   void transferBalanceSuicide(Address const& _from, Address const& _to) override;

   u256 getNonce(Address const& _addr) const override { return u256(0); }

   void setAuthor(Address _author) { author = _author; }

   u256 getFee(){ return fee; }

   void setAssetType(const uint64_t& id) { asset_id = id; }

   uint64_t getAssetType() { return asset_id; }

   std::unordered_map<Address, Account> getResultAccounts() { return result_accounts; }

   void clearResultAccount() { result_accounts.clear(); }

   Account const* getAccount(Address const& _a) const { return account(_a); } // TODO temp

   Account* getAccount(Address const& _a) { return account(_a); } // TODO temp

   void commit(CommitBehaviour _commitBehaviour) override;
    
   void saveStackSize() override { stack_size = transfers_stack.size(); }

   void revertStack() override;

   void publishContractTransfers() override;

   void clear_temporary_variables();

   dev::Address getNewAddress() const override;

private:

   vms::evm::evm_adapter adapter;

   std::unordered_map<Address, Account> result_accounts;

   uint64_t asset_id;

   Address author;

   u256 fee;

   std::stack<TransfersStruct> transfers_stack;

   size_t stack_size;

};

} }