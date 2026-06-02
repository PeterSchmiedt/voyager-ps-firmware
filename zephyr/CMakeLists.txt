message(STATUS "[voyager-ps] CMakeLists.txt processed (firmware module)")
message(STATUS "[voyager-ps] adding rgb_force_on.c unconditionally")

zephyr_library_named(voyager_ps_force_on)
zephyr_library_sources(${CMAKE_CURRENT_SOURCE_DIR}/rgb_force_on.c)

# ZMK adds its include/ to the 'app' target PRIVATELY (see ZMK app/CMakeLists.txt
# line 25: target_include_directories(app PRIVATE include)) — so external module
# libraries like ours can't see zmk/*.h. Pull it in explicitly.
# CMAKE_SOURCE_DIR is /<zmk-root>/zmk/app, which contains include/zmk/...
zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
