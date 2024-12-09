idf_component_register(
    SRCS "usb_hid_setup.c" "app_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio MicroCanvas2D
    )
