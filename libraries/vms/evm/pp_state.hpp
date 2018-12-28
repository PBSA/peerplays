#pragma once

#include <boost/filesystem.hpp>

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

   pp_state( fs::path data_dir, vms::evm::evm_adapter& _adapter );

   u256 balance(Address const& _id) const override;

   u256 balance(Address const& _id, const std::string& _callIdAsset) const override;

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

   bool getObjectProperty(const std::string& location, dev::bytes& result) override;

///////////////////////////////////////////////////////////////////////////////////// // DB result;
   void setRootResults(dev::h256 const& _r) { cache_results.clear(); state_results.setRoot(_r); }

   dev::h256 rootHashResults() const { return state_results.root(); }

   dev::OverlayDB const& dbResults() const { return db_res; }

   dev::OverlayDB& dbResults() { return db_res; }


   void addResult(const std::string& id, std::pair<ExecutionResult, TransactionReceipt>& res);

   std::pair<ExecutionResult, TransactionReceipt> const* getResult(const std::string& _id) const;

   std::pair<ExecutionResult, TransactionReceipt>* getResult(h160 const& idResult, bool a);

   void commitResults();

   static LogEntriesSerializ logEntriesSerialization(LogEntries const& _logs);

   static LogEntries logEntriesDeserialize(LogEntriesSerializ const& _logs);

   dev::Address getNewAddress() const override;
/////////////////////////////////////////////////////////////////////////////////////

private:

   vms::evm::evm_adapter& adapter;

   std::unordered_map<Address, Account> result_accounts;

   uint64_t asset_id;

   Address author;

   u256 fee;

   std::stack<TransfersStruct> transfers_stack;

   size_t stack_size;

///////////////////////////////////////////////////////////////////////////////////// // DB result;
   dev::OverlayDB db_res;

	dev::eth::SecureTrieDB<h160, dev::OverlayDB> state_results;

	ExecResult cache_results;

};

namespace ethbit{
    template <class DB>
    void commit(ExecResult const& cache_results, dev::eth::SecureTrieDB<h160, DB>& state_results)
    {
        if(cache_results.size()){        
            for (auto const& i: cache_results){
                RLPStream streamRLP(11);
                streamRLP << i.second.first.gasUsed << static_cast<uint32_t>(i.second.first.excepted) << i.second.first.newAddress;
                streamRLP.append(i.second.first.output);
                streamRLP << static_cast<uint32_t>(i.second.first.codeDeposit) << i.second.first.gasRefunded;
                streamRLP << i.second.first.depositSize << i.second.first.gasForDeposit;
                streamRLP << i.second.second.statusCode() << i.second.second.cumulativeGasUsed();

                const LogEntries& log = i.second.second.log();
                std::vector<dev::bytes> logEntries;
                for(size_t i = 0; i < log.size(); i++){
                    const LogEntry& logTemp = log[i];
                    RLPStream s(3);
                    s << logTemp.address << logTemp.topics << logTemp.data;
                    logEntries.push_back(s.out());
                }
                streamRLP.append(logEntries);

                state_results.insert(i.first, &streamRLP.out());
            }
        }
    }
}

class BlockChainInformations
{

public:

//    BlockChainInformations(database& _db) : btsDB(_db){}

   bool getObjectProperty(const std::string& location, dev::bytes& result);

   bool checkLocationProperty(const std::string& locationProperty);

private:

   uint64_t stringToUint64(std::string str);

   std::vector<std::string> getAllMatches(const std::string& str, const std::regex& regex);

//    fc::optional<object_id_type> parseId(const std::string& location);

   std::vector<std::string> parseLocationProperty(const std::string& locationProperty);

//    fc::optional<dev::bytes> getValue(const std::string& json, const std::vector<std::string>& locationProperty);

//    std::string getJSONObject(const object_id_type& id);

//    database& btsDB;

};

} }
