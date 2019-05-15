
add_executable( cat-parts cat-parts.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

# we only actually need Boost, but link against FC for now so we don't duplicate it.
target_link_libraries( cat-parts PRIVATE fc_pp ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
target_include_directories( cat-parts PUBLIC ${Boost_INCLUDE_DIR} )

add_executable( member_enumerator member_enumerator.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

# we only actually need Boost, but link against FC for now so we don't duplicate it.
target_link_libraries( member_enumerator PRIVATE fc_pp graphene_app graphene_net graphene_chain graphene_egenesis_brief graphene_utilities graphene_wallet ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
