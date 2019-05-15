file(GLOB HEADERS "include/graphene/time/*.hpp")

add_library( graphene_time
             time.cpp
           )

target_link_libraries( graphene_time fc_pp )
target_include_directories( graphene_time
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" ${Boost_INCLUDE_DIR})

install( TARGETS
   graphene_time

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
