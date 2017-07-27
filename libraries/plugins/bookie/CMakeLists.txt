file(GLOB HEADERS "include/graphene/bookie/*.hpp")

add_library( graphene_bookie 
             bookie_plugin.cpp
             bookie_api.cpp
           )

target_link_libraries( graphene_bookie graphene_chain graphene_app )
target_include_directories( graphene_bookie
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( bookie_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_bookie

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
