#pragma once

#include <graphene/app/plugin.hpp>

#include <graphene/chain/son_object.hpp>

namespace graphene { namespace peerplays_sidechain {
using namespace chain;

namespace detail
{
    class peerplays_sidechain_plugin_impl;
}

class peerplays_sidechain_plugin : public graphene::app::plugin
{
   public:
      peerplays_sidechain_plugin();
      virtual ~peerplays_sidechain_plugin();

      std::string plugin_name()const override;
      virtual void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
      virtual void plugin_startup() override;

      std::unique_ptr<detail::peerplays_sidechain_plugin_impl> my;

      son_id_type get_son_id();
      son_object get_son_object();
      bool is_active_son();
      std::map<chain::public_key_type, fc::ecc::private_key>& get_private_keys();
};

} } //graphene::peerplays_sidechain

