file( GLOB SOURCES "*.cpp" )
file( GLOB HEADERS "*.hpp" )

add_subdirectory(vms_base)
add_subdirectory(wavm)

add_library( vm_executor STATIC ${SOURCES} ${HEADERS} )

target_link_libraries( vm_executor graphene_chain vms_base pp_wavm )

target_include_directories( vms_base PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" $<TARGET_PROPERTY:eosio_chain,INCLUDE_DIRECTORIES> )


