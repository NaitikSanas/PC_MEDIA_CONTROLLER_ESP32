#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"
#include "uCanvas_api.h"
#include "simple_menu.h"

static const char *TAG = "BController";
#include "ucanvas_slider.h"
/************* TinyUSB descriptors ****************/

#define TUSB_DESC_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

/**
 * @brief HID report descriptor
 *
 * In this example we implement Keyboard + Mouse HID device,
 * so we must define both report descriptors
 */
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))
};

/**
 * @brief String descriptor
 */
const char* hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "NTK VLAB",             // 1: Manufacturer
    "Monitor Controller",      // 2: Product
    "123456",              // 3: Serials, should use chip ID
    "HID interface",  // 4: HID
};

/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and 1 HID interface
 */
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

/********* TinyUSB HID callbacks ***************/

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
    return hid_report_descriptor;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}

/********* Application ***************/

#define MENU_POSITION_X 5
#define MENU_POSITION_Y 18
#define TEXT_OFFSET_X 2
#define SPAN_X 5
#define SPAN_Y 10

static selection_menu_obj_t menu_1;
prompt_t prompt_1;

// Define GPIO pins for encoder channels
#define ENCODER_PIN_A  GPIO_NUM_47  // Example GPIO pin for Channel A
#define ENCODER_PIN_B  GPIO_NUM_48  // Example GPIO pin for Channel B

typedef enum {
    ENCODER_DIRECTION_NONE = 0,  // No movement
    ENCODER_DIRECTION_CW,        // Clockwise
    ENCODER_DIRECTION_CCW        // Counter-clockwise
} encoder_direction_t;

// Previous state of the encoder
static int last_state_a = 0;
#define DEBOUNCE_DELAY_MS 2  // 5 ms debounce delay

encoder_direction_t read_encoder_direction() {
    static int stable_state_a = 0;
    static int stable_state_b = 0;
    static uint32_t last_debounce_time = 0;

    // Read the raw current levels
    int raw_state_a = gpio_get_level(ENCODER_PIN_A);
    int raw_state_b = gpio_get_level(ENCODER_PIN_B);

    // Debounce logic
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS; // Current time in ms
    if ((now - last_debounce_time) > DEBOUNCE_DELAY_MS) {
        stable_state_a = raw_state_a;
        stable_state_b = raw_state_b;
        last_debounce_time = now;
    }

    encoder_direction_t direction = ENCODER_DIRECTION_NONE;

    if (stable_state_a != last_state_a) {
        if (stable_state_a == stable_state_b) {
            direction = ENCODER_DIRECTION_CW;
        } else {
            direction = ENCODER_DIRECTION_CCW;
        }
    }

    last_state_a = stable_state_a;
    return direction;
}
uint8_t keycode[6] = {0};
uint8_t event_detect = false;
uint8_t selector = 0;

static void app_send_hid_demo(void)
{
    // Keyboard output: Send key 'a/A' pressed and released
    if(!gpio_get_level(45)){
        while (!gpio_get_level(45))
        {
            /* code */
            vTaskDelay(1);
        }
        selector = !selector;
        ESP_LOGI(TAG, " MODE CHANGED"); 

    }
    encoder_direction_t dir = read_encoder_direction();
    if( dir == ENCODER_DIRECTION_CCW ){
        ESP_LOGI(TAG, "ENCODER_DIRECTION_CCW");  
        if(selector == 0)keycode[0] = HID_KEY_F4;
        if(selector == 1)keycode[0] = HID_KEY_F6;  
        event_detect = true;
    }

    else if( dir == ENCODER_DIRECTION_CW ){
        ESP_LOGI(TAG, "ENCODER_DIRECTION_CW");  
        if(selector == 0)keycode[0] = HID_KEY_F3;
        if(selector == 1)keycode[0] = HID_KEY_F5;       
        event_detect = true;
    }

    if(event_detect){
        ESP_LOGI(TAG, "SENDING EVENT"); 
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
         vTaskDelay(pdMS_TO_TICKS(50));
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        event_detect = false;
    }
}
void create_menu_1_instace(void);
void app_main(void)
{
    // Initialize button that will trigger HID reports
    const gpio_config_t boot_button_config = {
        .pin_bit_mask = BIT64(48),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };
    ESP_ERROR_CHECK(gpio_config(&boot_button_config));

     const gpio_config_t boot_button_config2 = {
        .pin_bit_mask = BIT64(47),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };
    ESP_ERROR_CHECK(gpio_config(&boot_button_config2));


    const gpio_config_t boot_button_config3 = {
        .pin_bit_mask = BIT64(45),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };
    ESP_ERROR_CHECK(gpio_config(&boot_button_config3));

    ESP_LOGI(TAG, "USB initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = hid_configuration_descriptor, // HID configuration descriptor for full-speed and high-speed are the same
        .hs_configuration_descriptor = hid_configuration_descriptor,
        .qualifier_descriptor = NULL,
#else
        .configuration_descriptor = hid_configuration_descriptor,
#endif // TUD_OPT_HIGH_SPEED
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

    start_uCanvas_engine();
    uCanvas_Scene_t* scene = New_uCanvas_Scene();
    uCanvas_set_active_scene(scene);
    // New_uCanvas_2DRectangle(0,0,64,64);
    create_menu_1_instace();
    while (1) {
        // if (tud_mounted()) {
        //     static bool send_hid_data = true;
            
        //     app_send_hid_demo();
        
        // }
        vTaskDelay(1);
    }
}



uCanvas_obj_t* ui_selector;
static uint8_t selector_icon[9*9] = {   \
    1,1,1,1,1,0,0,0,0,                    \
    1,1,1,1,1,1,0,0,0,                    \
    1,1,1,1,1,1,1,0,0,                    \
    1,1,1,1,1,1,1,1,0,                    \
    1,1,1,1,1,1,1,1,1,                    \
    1,1,1,1,1,1,1,1,0,                    \
    1,1,1,1,1,1,1,0,0,                    \
    1,1,1,1,1,1,0,0,0,                    \
    1,1,1,1,1,0,0,0,0  
};

slider_t slider;

void slider_pb_cb(void){
    menu_1.is_active = true;
    if(menu_get_current_index(&menu_1)==2){
        slider.slider_step = 4;
    }
    set_slider_visiblity(&slider,INVISIBLE);
    hide_prompt(&prompt_1);
    slider.is_active = false;
}

float last_slider_value = 0.0;
float b1 = 0.0;
float b2 = 0.0;
float vol_level = 0.0;
void slider_cb(void){
    float cur_slider_pos = slider.slider_value;
    printf("slider_value %f\r\n",cur_slider_pos);
    uint8_t index = menu_get_current_index(&menu_1);
    if(index == 0){
        if(cur_slider_pos - last_slider_value == 4){
            uint8_t keycode[6] = {HID_KEY_F6};
            b1 = cur_slider_pos;
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
            vTaskDelay(pdMS_TO_TICKS(50));
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        }

        if(cur_slider_pos - last_slider_value == -4){
            b1 = cur_slider_pos;
            uint8_t keycode[6] = {HID_KEY_F5};
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
            vTaskDelay(pdMS_TO_TICKS(50));
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        }
    }
    if(index == 1){
        if(cur_slider_pos - last_slider_value == 4){
            uint8_t keycode[6] = {HID_KEY_F4};
            b2 = cur_slider_pos;
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
            vTaskDelay(pdMS_TO_TICKS(50));
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        }

        if(cur_slider_pos - last_slider_value == -4){
            b2 = cur_slider_pos;
            uint8_t keycode[6] = {HID_KEY_F3};
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
            vTaskDelay(pdMS_TO_TICKS(50));
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        }
    }

    if(index == 2){
        if(cur_slider_pos - last_slider_value == 2){
            printf("vol-up\r\n");
            uint8_t keycode[6] = { HID_KEY_F1};
            vol_level = cur_slider_pos;
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
            vTaskDelay(pdMS_TO_TICKS(50));
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        }

        if(cur_slider_pos - last_slider_value == -2){
            printf("vol-down\r\n");
            vol_level = cur_slider_pos;
            uint8_t keycode[6] = {HID_KEY_F2};
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
            vTaskDelay(pdMS_TO_TICKS(50));
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        }
    }
    last_slider_value = slider.slider_value;
}

void onItemClicked_Menu_1(void){
    printf("clicked\r\n");
    if(menu_get_current_index(&menu_1)==0){
        menu_1.is_active = false;
        show_prompt(&prompt_1);
        set_slider_position(&slider,b1);
        set_slider_visiblity(&slider,VISIBLE);
        slider.is_active = true;
    }

    if(menu_get_current_index(&menu_1)==1){
        menu_1.is_active = false;
        show_prompt(&prompt_1);
        set_slider_position(&slider,b2);
        set_slider_visiblity(&slider,VISIBLE);
        slider.is_active = true;
    }

    if(menu_get_current_index(&menu_1)==2){
        menu_1.is_active = false;
        show_prompt(&prompt_1);
        slider.slider_step = 2;
        set_slider_position(&slider,vol_level); 
        set_slider_visiblity(&slider,VISIBLE);
        slider.is_active = true;
    }
}

void onScrollUp_Menu_1(void){
    printf("[Event]:Scroll-Up : idx %d\r\n",menu_get_current_index(&menu_1));
}
void onScrollDown_Menu_1(void){
    printf("[Event]:Scroll-Down : idx %d\r\n",menu_get_current_index(&menu_1));  
}

void create_menu_1_instace(void){
    menu_1.menu_position_x = MENU_POSITION_X;
    menu_1.menu_position_y = MENU_POSITION_Y;
    menu_1.span_x = SPAN_X;
    menu_1.span_y = SPAN_Y;
    menu_1.text_offset_x = 20;

    menu_1.click_handler        = onItemClicked_Menu_1; //This function will be called when select button is pressed
    menu_1.scroll_up_handler    = onScrollUp_Menu_1;
    menu_1.scroll_down_handler  = onScrollDown_Menu_1;
    menu_1.select_btn_wait_to_release = true;

    menu_set_active_elements(&menu_1,32); //Set Active Members of menu object
    menu_set_enable_cursor_index_text(&menu_1,true);//shows cursor position in corner
    menu_add_gpio_control(&menu_1,47,48,45); //Add GPIO to control UP/DOWN/SELECT navigation
    
    /*
        The selector or cursor can be any object like sprite, rectangle, circle etc.
    */
    sprite2D_t selector_sprite;
    uCanvas_Compose_2DSprite_Obj(&selector_sprite,selector_icon,9,9);
    ui_selector = New_uCanvas_2DSprite(selector_sprite, MENU_POSITION_X,MENU_POSITION_Y);  

    /*Instantiate Menu Object*/
    create_menu(&menu_1,ui_selector);
    
    menu_set_title(&menu_1,"< PC Control Panel >"   ,20,0);
    
    New_uCanvas_2DLine(0,MENU_POSITION_Y-8,128,MENU_POSITION_Y-8);
    New_uCanvas_2DLine(0,MENU_POSITION_Y-7,128,MENU_POSITION_Y-7);

    /* Add Content in your menu object */
    menu_set_content(&menu_1,"M1 Brightness",0);
    menu_set_content(&menu_1,"M2 Brightness",1);
    menu_set_content(&menu_1,"Volume"       ,2);
    menu_set_content(&menu_1,"Source"       ,3);
    menu_set_content(&menu_1,"Next-Page"    ,4);
    
    menu_1.is_active = true;
    prompt_1.box_h = 32;
    prompt_1.box_w = 100;
    prompt_1.prompt_position_x = 10;
    prompt_1.prompt_position_y = 20;
    prompt_1.relative_text_position_x = 5;
    prompt_1.relative_text_position_y = 5;

    create_prompt(&prompt_1);
    
    slider.is_active = false;
    slider.max_value = 100.0;
    slider.min_value = 0.0;
    slider.slider_value = 0.0;
    slider.slider_step = 4.0;
    slider.position_x = 39;
    slider.position_y = 39;
    slider.relative_label_pos_x = -10;
    slider.relative_label_pos_y = -12;
    slider.show_label = true;
    slider.slider_event_handler = slider_cb;
    slider.slider_pb_event_handler = slider_pb_cb;
    slider.slider_gpio_1 = 47;
    slider.slider_gpio_2 = 48;
    slider.slider_gpio_3 = 45;
    slider.slider_thickness = 4;
    slider.slider_notch_radius = 3;
    slider.slider_length = 40;
    slider.wait_to_release = true;
    create_slider(&slider);
    set_slider_visiblity(&slider,INVISIBLE);
}
