file( GLOB SOURCES "*.cpp" )
file( GLOB HEADERS "*.hpp" )

add_library( vms_base STATIC ${SOURCES} ${HEADERS} )

target_link_libraries( vms_base graphene_chain )

target_include_directories( vms_base PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" )


