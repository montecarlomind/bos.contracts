project(redpacket)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( redpacket redpacket redpacket.cpp )
target_include_directories( redpacket PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( redpacket ${CMAKE_SOURCE_DIR}/../ricardian )