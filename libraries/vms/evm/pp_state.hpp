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

/**
 * @struct TransfersStruct
 * @brief Describes transfer in EVM
 */
struct TransfersStruct {
    Address from;
    Address to;
    u256 value;
    TransfersStruct() {}
    TransfersStruct(Address _from, Address _to, u256 _value) :
                    from(_from), to(_to), value(_value) {}
};

/**
 * @brief Get id from given Address
 * 
 * @param addr address to get id
 * @return std::pair where first - account type and second - uint64_t form of id
 */
std::pair<uint64_t, uint64_t> address_to_id( const Address& addr );

/**
 * @brief Get Address from given id
 * 
 * @param id uint64_t form of id
 * @param type account type
 * @return Address of given id
 */
Address id_to_address( const uint64_t& id, const uint64_t& type );

/**
 * @class pp_state
 * @brief Describes internal state of EVM
 * 
 * pp_state describes internal state of EVM.
 * All overrided method have description in parent class State. Only implementation changed, not behavior.
 */
class pp_state : public State
{

public:

   pp_state( fs::path data_dir, vms::evm::evm_adapter _adapter );

   u256 balance(Address const& _id) const override;

   u256 balance(Address const& _id, const u256& _callIdAsset) const override;

   void incNonce(Address const& _addr) override;

   void addBalance(Address const& _id, u256 const& _amount) override;

   void addBalance(Address const& _id, u256 const& _amount, uint64_t id_asset);

   void subBalance(Address const& _addr, u256 const& _value) override;

   void transferBalance(Address const& _from, Address const& _to, u256 const& _value) override;

   void transferBalance(Address const& _from, Address const& _to, u256 const& _value, u256 const& _idAsset) override;

   void transferBalanceSuicide(Address const& _from, Address const& _to) override;

   u256 getNonce(Address const& _addr) const override { return u256(0); }

   uint64_t getAssetType() const override { return asset_id; }

   void commit(CommitBehaviour _commitBehaviour) override;

   void rollback(size_t _savepoint) override;

   void publishContractTransfers() override;

   void setExecutionFee(const u256& feesEarned ) override { fee = feesEarned; };

   dev::Address getNewAddress() const override;

   /**
    * @brief Get the fee
    * 
    * @return u256
    */
   u256 getFee() const { return fee; }

   /**
    * @brief Set the asset type for EVM transfers
    * 
    * @param id uint64_t form if id
    */
   void setAssetType(const uint64_t& id) { asset_id = id; }

   /**
    * @brief Get the result accounts
    * 
    * @return std::unordered_map with key - address and value - account
    */
   std::unordered_map<Address, Account> getResultAccounts() const { return result_accounts; }

   /**
    * @brief Get the account of given address
    * 
    * @param _a Address
    * @return Account const*
    */
   Account const* getAccount(Address const& _a) const { return account(_a); }

   /**
    * @brief Same as `getAccount() const`
    */
   Account* getAccount(Address const& _a) { return account(_a); }

   /**
    * @brief Clear temp vars of enviroment
    */
   void clear_temporary_variables();

   /**
    * @brief Set the allowed assets for contract
    * 
    * @param _allowed_assets std::set of uint64_t form of asset id
    */
   void set_allowed_assets( const std::set<uint64_t>& _allowed_assets ) { allowed_assets = _allowed_assets; }

private:

   vms::evm::evm_adapter adapter;

   std::unordered_map<Address, Account> result_accounts;

   uint64_t asset_id;

   Address author;

   u256 fee;

   std::stack<TransfersStruct> transfers_stack;

   std::set<uint64_t> allowed_assets;

};

} }