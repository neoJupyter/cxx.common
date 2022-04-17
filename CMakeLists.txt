cmake_minimum_required(VERSION 3.11)

project(njp.cxx)

kls_add_library_module(njp.common njpxx::common)
kls_module_source_directory(njp.common Module)
kls_public_source_directory(njp.common Published)
target_link_libraries(njp.common PUBLIC klsxx::essential klsxx::coroutine klsxx::thread klsxx::phttp)

kls_vcpkg_package(nlohmann-json)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(njp.common PUBLIC nlohmann_json nlohmann_json::nlohmann_json)
