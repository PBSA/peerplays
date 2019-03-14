file( GLOB SOURCES "*.cpp" )
file( GLOB HEADERS "*.hpp" )

add_library( pp_evm STATIC ${SOURCES} ${HEADERS} )

target_link_libraries( pp_evm vms_base evmc devcore ethereum ethashseal )
#target_include_directories( pp_evm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/aleth" )
target_include_directories( pp_evm PUBLIC
                            "${CMAKE_CURRENT_SOURCE_DIR}"
                            "${CMAKE_SOURCE_DIR}/aleth"
                            "${CMAKE_SOURCE_DIR}"
                            $<TARGET_PROPERTY:devcore,INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:ethereum,INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:ethashseal,INCLUDE_DIRECTORIES> )
