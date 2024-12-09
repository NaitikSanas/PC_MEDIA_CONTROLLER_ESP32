#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"

void usb_hid_setup();
void usb_hid_send_key_stroke(uint8_t*keycode);