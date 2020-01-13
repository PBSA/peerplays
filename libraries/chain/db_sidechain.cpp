#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

void database::initialize_db_sidechain()
{
    recreate_primary_wallet = false;
}

} }
