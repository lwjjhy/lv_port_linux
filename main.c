#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lvgl/src/core/lv_global.h"

#if LV_USE_WAYLAND
#include "backends/interface.h"
#endif

uint16_t window_width = 1280;
uint16_t window_height = 800;


static const char *getenv_default(const char *name, const char *dflt)
{
    return getenv(name) ? : dflt;
}

#if LV_USE_EVDEV
static void indev_deleted_cb(lv_event_t * e)
{
    if(LV_GLOBAL_DEFAULT()->deinit_in_progress) return;
    lv_obj_t * cursor_obj = lv_event_get_user_data(e);
    lv_obj_delete(cursor_obj);
}

static void discovery_cb(lv_indev_t * indev, lv_evdev_type_t type, void * user_data)
{
    LV_LOG_USER("new '%s' device discovered", type == LV_EVDEV_TYPE_REL ? "REL" :
                                              type == LV_EVDEV_TYPE_ABS ? "ABS" :
                                              type == LV_EVDEV_TYPE_KEY ? "KEY" :
                                              "unknown");

    lv_display_t * disp = user_data;
    lv_indev_set_display(indev, disp);

    if(type == LV_EVDEV_TYPE_REL) {
        /* Set the cursor icon */
        LV_IMAGE_DECLARE(mouse_cursor_icon);
        lv_obj_t * cursor_obj = lv_image_create(lv_display_get_screen_active(disp));
        lv_image_set_src(cursor_obj, &mouse_cursor_icon);
        lv_indev_set_cursor(indev, cursor_obj);

        /* delete the mouse cursor icon if the device is removed */
        lv_indev_add_event_cb(indev, indev_deleted_cb, LV_EVENT_DELETE, cursor_obj);
    }
}

static void lv_linux_init_input_pointer(lv_display_t *disp)
{
    /* Enables a pointer (touchscreen/mouse) input device
     * Use 'evtest' to find the correct input device. /dev/input/by-id/ is recommended if possible
     * Use /dev/input/by-id/my-mouse-or-touchscreen or /dev/input/eventX
     * 
     * If LV_LINUX_EVDEV_POINTER_DEVICE is not set, automatic evdev disovery will start
     */
    const char *input_device = getenv("LV_LINUX_EVDEV_POINTER_DEVICE");

    if (input_device == NULL) {
        LV_LOG_USER("the LV_LINUX_EVDEV_POINTER_DEVICE environment variable is not set. using evdev automatic discovery.");
        lv_evdev_discovery_start(discovery_cb, disp);
        return;
    }

    lv_indev_t *touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, input_device);
    lv_indev_set_display(touch, disp);

    /* Set the cursor icon */
    LV_IMAGE_DECLARE(mouse_cursor_icon);
    lv_obj_t * cursor_obj = lv_image_create(lv_display_get_screen_active(disp));
    lv_image_set_src(cursor_obj, &mouse_cursor_icon);
    lv_indev_set_cursor(touch, cursor_obj);
}
#endif

#if LV_USE_LINUX_FBDEV
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
    lv_display_t * disp = lv_linux_fbdev_create();

#if LV_USE_EVDEV
    lv_linux_init_input_pointer(disp);
#endif

    lv_linux_fbdev_set_file(disp, device);
}
#elif LV_USE_LINUX_DRM
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_DRM_CARD", "/dev/dri/card0");
    lv_display_t * disp = lv_linux_drm_create();

#if LV_USE_EVDEV
    lv_linux_init_input_pointer(disp);
#endif

    lv_linux_drm_set_file(disp, device, -1);
}
#elif LV_USE_SDL
static void lv_linux_disp_init(void)
{
    lv_display_t * disp = lv_sdl_window_create(window_width, window_height);

    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);

    lv_sdl_mouse_create();
    lv_indev_t * mousewheel = lv_sdl_mousewheel_create();
    lv_indev_set_group(mousewheel, lv_group_get_default());

    lv_indev_t * keyboard = lv_sdl_keyboard_create();
    lv_indev_set_group(keyboard, lv_group_get_default());

}
#elif LV_USE_WAYLAND
    /* see backend/wayland.c */
#else
#error Unsupported configuration
#endif

#if LV_USE_WAYLAND == 0
void lv_linux_run_loop(void)
{
    uint32_t idle_time;

    /*Handle LVGL tasks*/
    while(1) {

        idle_time = lv_timer_handler(); /*Returns the time to the next timer execution*/
        usleep(idle_time * 1000);
    }
}
#endif


#if LV_USE_ARC && LV_BUILD_EXAMPLES

static void value_changed_event_cb(lv_event_t * e);

void lv_example_arc_1(void)
{
    lv_obj_t * label = lv_label_create(lv_screen_active());

    /*Create an Arc*/
    lv_obj_t * arc = lv_arc_create(lv_screen_active());
    lv_obj_set_size(arc, 150, 150);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_value(arc, 10);
    lv_obj_center(arc);
    lv_obj_add_event_cb(arc, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, label);

    /*Manually update the label for the first time*/
    lv_obj_send_event(arc, LV_EVENT_VALUE_CHANGED, NULL);
}

static void value_changed_event_cb(lv_event_t * e)
{
    lv_obj_t * arc = lv_event_get_target_obj(e);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);

    lv_label_set_text_fmt(label, "%" LV_PRId32 "%%", lv_arc_get_value(arc));

    /*Rotate the label to the current position of the arc*/
    lv_arc_rotate_obj_to_angle(arc, label, 25);
}

static void set_angle(void * obj, int32_t v)
{
    lv_arc_set_value((lv_obj_t *)obj, v);
}

/**
 * Create an arc which acts as a loader.
 */
void lv_example_arc_2(void)
{
    /*Create an Arc*/
    lv_obj_t * arc = lv_arc_create(lv_screen_active());
    lv_obj_set_x(arc, 100);
    lv_obj_set_y(arc, 100);

    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);   /*Be sure the knob is not displayed*/
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);  /*To not allow adjusting by click*/
    // lv_obj_center(arc);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, set_angle);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);    /*Just for the demo*/
    lv_anim_set_repeat_delay(&a, 500);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_start(&a);

}
#endif

void btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_current_target(e);
    switch (code) {
    case LV_EVENT_CLICKED: {
        printf("btn pressed!!!\r\n");
        static int led_status = 0;
        led_status++;
        lv_obj_t *lab = lv_obj_get_child(btn, 0);
        if(led_status%2) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xff0000), 0);
            lv_label_set_text(lab, "LED OFF");
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2196f3), 0);
            lv_label_set_text(lab, "LED ON");
        }
        /* code */
        break;
    }
    default:
        break;
    }
    
}

void lv_example_btn(void)
{
    static lv_style_t style = {0};
    lv_style_init(&style);
    
    /*Set a background color and a radius*/
    lv_style_set_radius(&style, 15);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_color_hex(0x2196f3));

    /*Add border to the bottom+right*/
    lv_style_set_border_color(&style, lv_color_hex(0x00ff00));
    lv_style_set_border_width(&style, 5);
    lv_style_set_border_opa(&style, LV_OPA_50);
    lv_style_set_border_side(&style, LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT);

    /*Add a shadow*/
    lv_style_set_shadow_width(&style, 55);
    lv_style_set_shadow_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_shadow_opa(&style, LV_OPA_100);
    // rotation
    lv_style_set_transform_rotation(&style, 120);
    /*Create an Arc*/
    lv_obj_t * btn = lv_btn_create(lv_screen_active());
    lv_obj_add_event(btn, btn_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_x(btn, 100);
    lv_obj_set_y(btn, 100);
    lv_obj_set_size(btn, 180, 160);
    lv_obj_add_style(btn, &style, 0);
    

    //label
    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, "LED ON");
    lv_obj_center(lab);

    // /*Create an object with the new style*/
    // lv_obj_t * image = lv_image_create(btn);
    // LV_IMAGE_DECLARE(img_cogwheel_argb);
    // lv_image_set_src(image, &img_cogwheel_argb);
    // // lv_obj_move_to_index(image, -1);
    // lv_obj_move_to_index(btn, -1);


}

void lv_example_style_6(void)
{
    static lv_style_t style;
    lv_style_init(&style);

    /*Set a background color and a radius*/
    lv_style_set_radius(&style, 5);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_palette_lighten(LV_PALETTE_GREY, 3));
    lv_style_set_border_width(&style, 2);
    lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    lv_style_set_image_recolor(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_image_recolor_opa(&style, LV_OPA_50);
    lv_style_set_transform_rotation(&style, 300);
    
    /*Create an object with the new style*/
    lv_obj_t * obj = lv_image_create(lv_screen_active());
    lv_obj_add_style(obj, &style, 0);

    LV_IMAGE_DECLARE(img_cogwheel_argb);
    lv_image_set_src(obj, &img_cogwheel_argb);

    lv_obj_center(obj);
}

void xdev_demo(void)
{
    // #include "../../lv_examples.h"

    // lv_example_arc_1();
    // lv_example_arc_2();
    lv_example_btn();
    lv_example_style_6();
}

int main(int argc, char **argv)
{

    printf("main!!!\r\n");

    /* Initialize LVGL. */
    lv_init();

    /* Initialize the configured backend SDL2, FBDEV, libDRM or wayland */
    lv_linux_disp_init();

    /*Create a Demo*/
    // lv_demo_widgets();
    // lv_demo_widgets_start_slideshow();
    xdev_demo();

    lv_linux_run_loop();

    return 0;
}
