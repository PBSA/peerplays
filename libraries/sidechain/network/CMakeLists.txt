file( GLOB HEADERS "include/sidechain/network/*.hpp" )
file( GLOB SOURCES "*.cpp" )

add_library( sidechain_network STATIC ${SOURCES} ${HEADERS} )

target_link_libraries( sidechain_network PUBLIC graphene_chain fc zmq  )
target_include_directories( sidechain_network PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
