#include <db_result.hpp>
#include <fc/crypto/hex.hpp>

namespace vms { namespace base {


void db_result::init( fs::path data_dir )
{
   db = State::openDB((data_dir / "result_db").string(), sha3(rlp("")), WithExisting::Trust);
	state_db = SecureTrieDB<h160, OverlayDB>(&db);
   set_root( sha3( rlp("") ).hex() );
}

void db_result::add_result( const std::string& id, bytes res )
{
   cache.insert( std::make_pair( right160( sha3(id) ), res ) );
}

void db_result::commit_cache()
{
   if(cache.size()) {
      for (auto const& i: cache) {
         RLPStream streamRLP(1);
         streamRLP << i.second;
         state_db.insert(i.first, &streamRLP.out());
      }
      cache.clear();
   }
}

fc::optional<bytes> db_result::get_results( const std::string& id )
{
   fc::optional<bytes> result;
   auto address = right160( sha3(id) );
   auto it = cache.find( address );
	if (it != cache.end()) {
   	result = it->second;
   } else {
      std::string state_back = state_db.at( address );
      std::cout << state_back << std::endl;
      if (state_back.empty())
			return fc::optional<bytes>();

      RLP state(state_back);
      bytes res = state[0].toBytes();

      cache.insert( std::make_pair( address, res ) );
      result = cache.at( address );
   }
   return result;
}

} }