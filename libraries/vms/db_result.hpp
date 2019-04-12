# pragma once 

#include <boost/filesystem.hpp>

#include <libethereum/SecureTrieDB.h>
#include <libdevcore/OverlayDB.h>
#include <libdevcore/Common.h>
#include <libethereum/State.h>

#include <fc/optional.hpp>


namespace vms { namespace base {

namespace fs = boost::filesystem;

using namespace dev;
using namespace dev::eth;

/**
 * @brief Describes execution results
 * 
 * exec_result is std::unordered_map where key is address and value is serialized result
 */
using exec_result = std::unordered_map<h160, bytes>;

/**
 * @class db_result
 * @brief Database for contract execution results
 * 
 * This class describes database for execution result. It is key-value storage.
 * The most recent result are stored in cache for fast access.
 */
class db_result {

public:

   db_result() = default;

   /**
    * @brief Initialize db results
    * 
    * Open database and set root for db_res
    * @param data_dir path to db
    */
   void init( fs::path data_dir );

   /**
    * @brief Add result to db_res
    * 
    * Add result from contract exec to db_res (insert it to cache)
    * @param id address (key fro db)
    * @param res serialized exec result (value for db)
    */
   void add_result( const std::string& id, bytes res );

   /**
    * @brief Save cached changes
    * 
    * Save chached changes to db and clear cache
    */
   void commit_cache();

   /**
    * @brief Get the results
    * 
    * @param id result id in string format
    * @return if result exists return it, else fc::optional will be not valid
    */
   fc::optional<bytes> get_results( const std::string& id );

   /**
    * @brief Set the root in db
    * 
    * @param _r hex root in string format
    */
   void set_root( std::string const& _r ) { cache.clear(); state_db.setRoot( h256(_r) ); }

   /**
    * @brief Get the root hash of db
    * 
    * @return hex root hash in string
    */
   std::string get_root_hash() const { return state_db.root().hex(); }

   /**
    * @brief Commit changes to db 
    */
   void commit() { db.commit(); }

private:

   /// Cache is some kind of buffer, where stored all recent results
   exec_result cache; 
   OverlayDB db;
   SecureTrieDB<h160, dev::OverlayDB> state_db;
};

} }