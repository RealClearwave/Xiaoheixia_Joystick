#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include "lv_drivers/display/drm.h"
#include "lv_drivers/indev/evdev.h"
#include "lv_drivers/display/fbdev.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/input.h>
#include <math.h>

#define DEVICE_PATH "/dev/input/event0"

// 定义输入事件类型
#define EV_TYPE_SOUND EV_SND

// 定义声音类型和事件代码
#define SND_TYPE_BEEP SND_BELL

#include "generated/gui_guider.h"
#include "generated/events_init.h"

#define DISP_BUF_SIZE (240 * 480)

lv_ui guider_ui;

char server_URL[1024]; //服务器地址+端口号
double eps = 0.2,cx = 10,cy = 5,cz = 5,ca = 5; //灵敏度及中央忽略区

// 打开设备
int open_device()
{
    int fd = open(DEVICE_PATH, O_WRONLY);
    if(fd == -1) {
        perror("Failed to open device");
    }
    return fd;
}

// 关闭设备
void close_device(int fd)
{
    close(fd);
}

// 发送蜂鸣器控制事件
void control_beeper(int fd, int frequency, int duration_ms)
{
    struct input_event event;
    memset(&event, 0, sizeof(event));

    // 填充输入事件
    event.type         = EV_TYPE_SOUND;
    event.code         = SND_TYPE_BEEP;
    event.value        = frequency; // 频率控制
    event.time.tv_sec  = 0;
    event.time.tv_usec = duration_ms * 1000; // 持续时间控制

    // 发送输入事件
    if(write(fd, &event, sizeof(event)) < sizeof(event)) {
        perror("Failed to write event");
    }
}

int main(void)
{
    //尝试读取配置文件
    FILE *js_cfg = fopen("/root/joystick.cfg","r");
    if (js_cfg != NULL){
        printf("CFG Set Up.");
        fscanf(js_cfg,"%s",server_URL);
        fscanf(js_cfg,"%lf%lf%lf%lf%lf",&eps,&cx,&cy,&cz,&ca);
    }else{
        printf("No CFG Assuming Default.");
        strcpy(server_URL,"192.168.31.82:5000");
    }
    lv_init();

    /*Linux frame buffer device init*/
    fbdev_init();

    /*A small buffer for LittlevGL to draw the screen's content*/
    static lv_color_t buf[DISP_BUF_SIZE];

    /*Initialize a descriptor for the buffer*/
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);

    /*Initialize and register a display driver*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res  = 240;
    disp_drv.ver_res  = 240;
    lv_disp_drv_register(&disp_drv);

    /* Linux input device init */
    evdev_init();

    /* Initialize and register a display input driver */
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type    = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    lv_indev_drv_register(&indev_drv);

    setup_ui(&guider_ui);
    events_init(&guider_ui);
    
    int fd = open_device();
    if(fd == -1) {
        return 1;
    }

    // 控制蜂鸣器发出1000Hz频率的声音，持续200毫秒
    control_beeper(fd, 1000, 200);
    sleep(1);
    control_beeper(fd, 0, 0);

    close_device(fd);

    lv_timer_handler();

    /*
    lv_bar_set_value(guider_ui.screen_bar_x,20,LV_ANIM_ON); //252~745
    lv_bar_set_value(guider_ui.screen_bar_y,40,LV_ANIM_ON); //263~758
    lv_bar_set_value(guider_ui.screen_bar_z,60,LV_ANIM_ON); //69~984
    lv_bar_set_value(guider_ui.screen_bar_a,80,LV_ANIM_ON); //156~863
    */
    

    /*Handle LVGL tasks*/
    FILE *fp;
    int b,c,z,a,y,x;
    double xx,yy,zz,aa;

    while(1) {
        //读取摇杆模拟量
        fp = popen("cat /sys/bus/iio/devices/iio:device0/in_voltage*_raw","r");
        if (fp == NULL){
            printf("Open File Failed.\n");
            continue;
        }
        
        fscanf(fp,"%d%d%d%d%d%d",&b,&c,&z,&a,&y,&x);
        pclose(fp);

        
        //量化至0~100
        yy = 100.0*(((double)(y-252)/(745.0-252))); xx = 100.0*(((double)(x-263)/(758.0-263)));
        zz = 100.0*(((double)(z-69)/(984.0-69))); aa = 100.0*(((double)(a-156)/(863.0-156)));
        
        //中央区舍去
        if (fabs(xx - 50) < cx) xx = 50;
        if (fabs(yy - 50) < cy) yy = 50;
        if (fabs(zz - 50) < cz) zz = 50;
        if (fabs(aa - 50) < ca) aa = 50;
        

        //更新UI
        lv_bar_set_value(guider_ui.screen_bar_y,yy,LV_ANIM_OFF); //252~745
        lv_bar_set_value(guider_ui.screen_bar_x,xx,LV_ANIM_OFF); //263~758
        lv_bar_set_value(guider_ui.screen_bar_z,zz,LV_ANIM_OFF); //69~984
        lv_bar_set_value(guider_ui.screen_bar_a,aa,LV_ANIM_OFF); //156~863

        //Push至服务器
        char tbf[1024];
        sprintf(tbf,"wget -q -O - \"http://192.168.31.82:5000/upload?x=%lf&y=%lf&a=%lf&z=%lf\"",xx,yy,aa,zz);
        system(tbf);

        lv_timer_handler();
        usleep(5000);
    }

    

    return 0;
}

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
