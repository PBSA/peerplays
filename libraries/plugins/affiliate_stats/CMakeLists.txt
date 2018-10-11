file(GLOB HEADERS "include/graphene/affiliate_stats/*.hpp")

add_library( graphene_affiliate_stats
             affiliate_stats_api.cpp
             affiliate_stats_plugin.cpp
           )

target_link_libraries( graphene_affiliate_stats graphene_chain graphene_app )
target_include_directories( graphene_affiliate_stats
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( affiliate_stats_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_affiliate_stats

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/affiliate_stats" )

