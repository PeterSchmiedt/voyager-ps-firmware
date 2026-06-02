# Source files go into the main zephyr library, not a named static archive.
# A named library only contributes its .o files to the final link when
# something outside references their symbols; our event listener and the
# function wrapper would both get silently dropped if they lived in their
# own archive.
zephyr_sources_ifdef(CONFIG_ZMK_RGB_UNDERGLOW rgb_save_on_press.c)
zephyr_sources_ifdef(CONFIG_ZMK_RGB_UNDERGLOW rgb_set_hsb_wrap.c)
zephyr_include_directories(${CMAKE_SOURCE_DIR}/include)

# Linker wrap: redirect every call to zmk_rgb_underglow_set_hsb through
# our __wrap_ function so the missing save_state() call is restored.
# See rgb_set_hsb_wrap.c for the why.
zephyr_link_libraries_ifdef(CONFIG_ZMK_RGB_UNDERGLOW
    "-Wl,--wrap=zmk_rgb_underglow_set_hsb"
)
