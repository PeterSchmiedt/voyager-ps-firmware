# Add directly to the main zephyr library, NOT to a separate named library.
# Named libraries are static archives and the linker only pulls .o files
# from them when symbols are referenced. Our rgb_save_on_press.c only
# registers a ZMK listener through linker-section attributes (.event_subscription),
# with no functions called from outside — so it gets dropped at link time
# from a named library despite the __used attributes on the section.
# zephyr_sources adds to the always-whole-linked main zephyr lib.
zephyr_sources_ifdef(CONFIG_ZMK_RGB_UNDERGLOW rgb_save_on_press.c)
zephyr_include_directories(${CMAKE_SOURCE_DIR}/include)
