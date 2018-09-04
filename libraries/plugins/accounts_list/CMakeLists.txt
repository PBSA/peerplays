file(GLOB HEADERS "include/graphene/accouns_list/*.hpp")

add_library( graphene_accounts_list 
             accounts_list_plugin.cpp
           )

target_link_libraries( graphene_accounts_list graphene_chain graphene_app )
target_include_directories( graphene_accounts_list
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( accounts_list_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_accounts_list

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
