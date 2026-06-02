message(STATUS "[voyager-ps] CMakeLists.txt processed (firmware module)")
message(STATUS "[voyager-ps] adding rgb_force_on.c unconditionally")

# Don't gate this on CONFIG_ZMK_RGB_UNDERGLOW — at the time Zephyr processes
# a module's CMakeLists, shield .conf overrides haven't propagated to CMake
# variables yet, so if(CONFIG_*) evaluates as undefined (i.e. false). ZMK's
# own app uses *_ifdef() variants which defer evaluation; we just compile
# unconditionally since this shield always enables underglow.
zephyr_library_named(voyager_ps_force_on)
zephyr_library_sources(${CMAKE_CURRENT_SOURCE_DIR}/rgb_force_on.c)
