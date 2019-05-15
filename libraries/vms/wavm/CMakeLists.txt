file( GLOB SOURCES "*.cpp" )
file( GLOB HEADERS "*.hpp" )

add_library( pp_wavm STATIC ${SOURCES} ${HEADERS} )

set_target_properties( pp_wavm PROPERTIES LINK_LIBRARIES $<TARGET_FILE:eosio_chain> LINKER_LANGUAGE CXX )
add_dependencies( pp_wavm eosio_chain )

target_link_libraries( pp_wavm vms_base )

target_include_directories( pp_wavm PUBLIC 
                            "${CMAKE_CURRENT_SOURCE_DIR}"
                            "${CMAKE_SOURCE_DIR}/eos"
                            "${CMAKE_SOURCE_DIR}"
                            $<TARGET_PROPERTY:eosio_chain,INCLUDE_DIRECTORIES>
                            )
