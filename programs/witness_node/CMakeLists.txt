add_executable( witness_node main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling witness_node with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

# We have to link against graphene_debug_witness because deficiency in our API infrastructure doesn't allow plugins to be fully abstracted #246
target_link_libraries( witness_node
                       PRIVATE graphene_app graphene_account_history graphene_affiliate_stats graphene_market_history graphene_witness graphene_chain graphene_debug_witness graphene_bookie graphene_egenesis_full fc peerplays_sidechain ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
# also add dependencies to graphene_generate_genesis graphene_generate_uia_sharedrop_genesis if you want those plugins

install( TARGETS
   witness_node

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
