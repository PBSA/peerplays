file( GLOB SOURCES "*.cpp" )
file( GLOB HEADERS "include/*.hpp" )

add_library( sidechain STATIC ${SOURCES} ${HEADERS} )

target_link_libraries( sidechain ${Boost_LIBRARIES} fc graphene_chain )
target_include_directories( sidechain PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
