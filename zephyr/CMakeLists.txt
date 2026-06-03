# Intentionally empty. Stock ZMK's change_brt / change_hue / change_sat /
# change_spd / select_effect / on / off / toggle all already persist state
# via save_state. The only path that doesn't is set_hsb (the dispatcher for
# absolute &rgb_ug RGB_COLOR_HSB(...) bindings), but we aren't using that —
# the encoder rotation handles brightness via RGB_BRI/RGB_BRD, which goes
# through change_brt and persists natively.
