file( GLOB SOURCES "*.cpp" )
file( GLOB HEADERS "*.hpp" )

add_library( pp_wavm STATIC ${SOURCES} ${HEADERS} )

target_link_libraries( pp_wavm vms_base eosio_chain )

target_include_directories( pp_wavm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" )
