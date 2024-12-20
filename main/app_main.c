#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"
#include "uCanvas_api.h"
#include "simple_menu.h"
#include "ucanvas_slider.h"
#include "uCanvas_User_IO.h"
#include "usb_hid_setup.h"
/********* static Defines ***************/
static const char *TAG = "BController";


#define MENU_POSITION_X 5
#define MENU_POSITION_Y 18
#define TEXT_OFFSET_X 2
#define SPAN_X 5
#define SPAN_Y 10

static selection_menu_obj_t main_menu;
prompt_t prompt_1;

/********* Application ***************/

void create_main_menu_instace(void);
void app_main(void)
{
    usb_hid_setup();

    //Here We start making UI and do Event Handling 
    start_uCanvas_engine();
    uCanvas_Scene_t* scene = New_uCanvas_Scene();
    uCanvas_set_active_scene(scene);

    uCanvas_Init_PushButton(47);
    uCanvas_Init_PushButton(48);
    uCanvas_Init_PushButton(45);

    create_main_menu_instace();
    while (1) {
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
/*
    When OK button is pressed when Slider is in ACTIVE State, We exit the prompt by setting main_menu 
    in ACTIVE State and store hide the slider and prompt object.
*/
void slider_pb_cb(void){
    main_menu.is_active = true;
    if(menu_get_current_index(&main_menu)==2){
        slider.slider_step = 4;
    }
    set_slider_visiblity(&slider,INVISIBLE);
    hide_prompt(&prompt_1);
    slider.is_active = false;
}

float last_slider_value = 0.0; //Last Value of slider to detect change
float b1 = 0.0; //Slider Value of Monitor 1 Brightness
float b2 = 0.0; //Slider Value of Monitor 2 Brightness
float vol_level = 0.0; //Slider Value of Volume

/*
    Based on menu_get_current_index(&main_menu), We configure slider to send the brightness UP/DOWN and 
    Volume UP/Down Command.
*/
void slider_cb(void){
    float cur_slider_pos = slider.slider_value;
    printf("slider_value %f\r\n",cur_slider_pos);
    uint8_t index = menu_get_current_index(&main_menu);
    //At Index 0 there is M1 - Brightness Control
    if(index == 0){
        if(cur_slider_pos - last_slider_value == 4){
            uint8_t keycode[6] = {HID_KEY_F6};
            b1 = cur_slider_pos;
            usb_hid_send_key_stroke(keycode);
        }

        if(cur_slider_pos - last_slider_value == -4){
            b1 = cur_slider_pos;
            uint8_t keycode[6] = {HID_KEY_F5};
            usb_hid_send_key_stroke(keycode);
        }
    }
    //At Index 0 there is M2 - Brightness Control
    if(index == 1){
        if(cur_slider_pos - last_slider_value == 4){
            uint8_t keycode[6] = {HID_KEY_F4};
            b2 = cur_slider_pos;
            usb_hid_send_key_stroke(keycode);
        }

        if(cur_slider_pos - last_slider_value == -4){
            b2 = cur_slider_pos;
            uint8_t keycode[6] = {HID_KEY_F3};
            usb_hid_send_key_stroke(keycode);
        }
    }
    //At Index 2 there is Volume Control
    if(index == 2){
        if(cur_slider_pos - last_slider_value == 2){
            printf("vol-up\r\n");
            uint8_t keycode[6] = { HID_KEY_F1};
            vol_level = cur_slider_pos;
            usb_hid_send_key_stroke(keycode);
        }

        if(cur_slider_pos - last_slider_value == -2){
            printf("vol-down\r\n");
            vol_level = cur_slider_pos;
            uint8_t keycode[6] = {HID_KEY_F2};
            usb_hid_send_key_stroke(keycode);
        }
    }
    last_slider_value = slider.slider_value;
}
/*
    This Event handler controls main menu navigation, Upon Item click, 
    We get index at which click event occured, we use this to show slider prompt on screen.
    Before we show it one screen we configure slider position based on item it's clicked on,
    Keep in mind we using same slider object all menu items. so we need to restore it's state 
    based on item clicked. 
 */
void onItemClicked_main_menu(void){
    printf("clicked\r\n");
    if(menu_get_current_index(&main_menu)==0){
        main_menu.is_active = false;
        show_prompt(&prompt_1);
        set_slider_position(&slider,b1);
        set_slider_visiblity(&slider,VISIBLE);
        slider.is_active = true;
    }

    if(menu_get_current_index(&main_menu)==1){
        main_menu.is_active = false;
        show_prompt(&prompt_1);
        set_slider_position(&slider,b2);
        set_slider_visiblity(&slider,VISIBLE);
        slider.is_active = true;
    }

    if(menu_get_current_index(&main_menu)==2){
        main_menu.is_active = false;
        show_prompt(&prompt_1);
        slider.slider_step = 2;
        set_slider_position(&slider,vol_level); 
        set_slider_visiblity(&slider,VISIBLE);
        slider.is_active = true;
    }
}

void onScrollUp_main_menu(void){
    printf("[Event]:Scroll-Up : idx %d\r\n",menu_get_current_index(&main_menu));
}
void onScrollDown_main_menu(void){
    printf("[Event]:Scroll-Down : idx %d\r\n",menu_get_current_index(&main_menu));  
}

void create_main_menu_instace(void){
    main_menu.menu_position_x = MENU_POSITION_X;
    main_menu.menu_position_y = MENU_POSITION_Y;
    main_menu.span_x = SPAN_X;
    main_menu.span_y = SPAN_Y;
    main_menu.text_offset_x = 20;

    main_menu.click_handler        = onItemClicked_main_menu; //This function will be called when select button is pressed
    main_menu.scroll_up_handler    = onScrollUp_main_menu;
    main_menu.scroll_down_handler  = onScrollDown_main_menu;
    main_menu.select_btn_wait_to_release = true;

    menu_set_active_elements(&main_menu,32); //Set Active Members of menu object
    menu_set_enable_cursor_index_text(&main_menu,true);//shows cursor position in corner
    menu_add_gpio_control(&main_menu,47,48,45); //Add GPIO to control UP/DOWN/SELECT navigation
    
    /*
        The selector or cursor can be any object like sprite, rectangle, circle etc.
    */
    sprite2D_t selector_sprite;
    uCanvas_Compose_2DSprite_Obj(&selector_sprite,selector_icon,9,9);
    ui_selector = New_uCanvas_2DSprite(selector_sprite, MENU_POSITION_X,MENU_POSITION_Y);  

    /*Instantiate Menu Object*/
    create_menu(&main_menu,ui_selector);
    
    menu_set_title(&main_menu,"< PC Control Panel >"   ,20,0);
    
    New_uCanvas_2DLine(0,MENU_POSITION_Y-8,128,MENU_POSITION_Y-8);
    New_uCanvas_2DLine(0,MENU_POSITION_Y-7,128,MENU_POSITION_Y-7);

    /* Add Content in your menu object */
    menu_set_content(&main_menu,"M1 Brightness",0);
    menu_set_content(&main_menu,"M2 Brightness",1);
    menu_set_content(&main_menu,"Volume"       ,2);
    menu_set_content(&main_menu,"Source"       ,3);
    menu_set_content(&main_menu,"Next-Page"    ,4);
    
    main_menu.is_active = true;
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



