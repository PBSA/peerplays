#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>

namespace graphene { namespace peerplays_sidechain {

namespace detail
{


class peerplays_sidechain_plugin_impl
{
   public:
      peerplays_sidechain_plugin_impl(peerplays_sidechain_plugin& _plugin)
         : _self( _plugin )
      { }
      virtual ~peerplays_sidechain_plugin_impl();

      peerplays_sidechain_plugin& _self;
};

peerplays_sidechain_plugin_impl::~peerplays_sidechain_plugin_impl()
{
   return;
}

} // end namespace detail

peerplays_sidechain_plugin::peerplays_sidechain_plugin() :
   my( new detail::peerplays_sidechain_plugin_impl(*this) )
{
}

peerplays_sidechain_plugin::~peerplays_sidechain_plugin()
{
   return;
}

std::string peerplays_sidechain_plugin::plugin_name()const
{
   return "peerplays_sidechain";
}

void peerplays_sidechain_plugin::plugin_set_program_options(
   boost::program_options::options_description& /*cli*/,
   boost::program_options::options_description& /*cfg*/
   )
{
}

void peerplays_sidechain_plugin::plugin_initialize(const boost::program_options::variables_map& /*options*/)
{
   ilog("peerplays sidechain plugin:  plugin_initialize()");
}

void peerplays_sidechain_plugin::plugin_startup()
{
   ilog("peerplays sidechain plugin:  plugin_startup()");
}

} }
