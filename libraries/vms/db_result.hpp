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

using exec_result = std::unordered_map<h160, bytes>;

class db_result {

public:

   db_result() = default;

   void init( fs::path data_dir );

   void add_result( const std::string& id, bytes res );

   void commit_cache();

   fc::optional<bytes> get_results( const std::string& id );

   void set_root( std::string const& _r ) { cache.clear(); state_db.setRoot( h256(_r) ); }

   std::string root_hash() const { return state_db.root().hex(); }

   void commit() { db.commit(); }

private:

   exec_result cache; 
   OverlayDB db;
   SecureTrieDB<h160, dev::OverlayDB> state_db;
};

} }