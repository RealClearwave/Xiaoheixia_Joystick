#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/input.h>
#include <math.h>
#include <sys/epoll.h>
#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lv_drivers/display/drm.h"
#include "lv_drivers/indev/evdev.h"
#include "lv_drivers/display/fbdev.h"
#include "generated/gui_guider.h"
#include "generated/events_init.h"

#define DEVICE_PATH "/dev/input/event0"
#define EV_TYPE_SOUND EV_SND
#define SND_TYPE_BEEP SND_BELL
#define DISP_BUF_SIZE (240 * 480)
#define DEVICE1 "/dev/input/event1"
#define DEVICE3 "/dev/input/event3"
#define MAX_EVENTS 5

lv_ui guider_ui;
char server_URL[1024]; // 服务器地址+端口号
double eps = 0.2, cx = 10, cy = 5, cz = 5, ca = 5,rot_scale = 5; // 灵敏度及中央忽略区
int batt = -1, key = 0, r_cnt = 0, rot = 0;

//摇杆量化值
double xx, yy, zz, aa;

// 打开设备
int open_device() {
    int fd = open(DEVICE_PATH, O_WRONLY);
    if(fd == -1) {
        perror("Failed to open device");
    }
    return fd;
}

// 关闭设备
void close_device(int fd) {
    close(fd);
}

// 发送蜂鸣器控制事件
void control_beeper(int fd, int frequency, int duration_ms) {
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

void* joystick_thread(void* arg) {
    FILE *fp;
    int b, c, z, a, y, x;
    

    while(1) {
        // 读取摇杆模拟量
        fp = popen("cat /sys/bus/iio/devices/iio:device0/in_voltage*_raw", "r");
        if (fp == NULL) {
            lv_label_set_text(guider_ui.screen_label_log,"Open ADC Port Failed.");
            continue;
        }

        fscanf(fp, "%d%d%d%d%d%d", &b, &c, &z, &a, &y, &x);
        pclose(fp);

        // 量化至0~100
        yy = 100.0 * (((double)(y - 252) / (745.0 - 252))); // 252~745
        xx = 100.0 * (((double)(x - 263) / (758.0 - 263))); // 263~758
        zz = 100.0 * (((double)(z - 69) / (984.0 - 69))); // 69~984
        aa = 100.0 * (((double)(a - 156) / (863.0 - 156))); // 156~863

        // 中央区舍去
        if (fabs(xx - 50) < cx) xx = 50;
        if (fabs(yy - 50) < cy) yy = 50;
        if (fabs(zz - 50) < cz) zz = 50;
        if (fabs(aa - 50) < ca) aa = 50;

        // 更新UI
        lv_bar_set_value(guider_ui.screen_bar_y, yy, LV_ANIM_OFF); 
        lv_bar_set_value(guider_ui.screen_bar_x, xx, LV_ANIM_OFF); 
        lv_bar_set_value(guider_ui.screen_bar_z, zz, LV_ANIM_OFF); 
        lv_bar_set_value(guider_ui.screen_bar_a, aa, LV_ANIM_OFF); 
        lv_bar_set_value(guider_ui.screen_bar_r, rot, LV_ANIM_OFF);

        usleep(5000);
    }
}

void* event_thread(void* arg) {
    int fdd1, fdd3, epoll_fd, nfds, i;
    struct epoll_event ev, events[MAX_EVENTS];
    struct input_event ie;

    // 打开设备文件
    fdd1 = open(DEVICE1, O_RDONLY | O_NONBLOCK);
    if (fdd1 == -1) {
        perror("open");
        return NULL;
    }

    fdd3 = open(DEVICE3, O_RDONLY | O_NONBLOCK);
    if (fdd3 == -1) {
        perror("open");
        close(fdd1);
        return NULL;
    }

    // 创建epoll文件描述符
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(fdd1);
        close(fdd3);
        return NULL;
    }

    // 将设备文件描述符添加到epoll中
    ev.events = EPOLLIN;
    ev.data.fd = fdd1;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fdd1, &ev) == -1) {
        perror("epoll_ctl");
        close(fdd1);
        close(fdd3);
        close(epoll_fd);
        return NULL;
    }

    ev.data.fd = fdd3;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fdd3, &ev) == -1) {
        perror("epoll_ctl");
        close(fdd1);
        close(fdd3);
        close(epoll_fd);
        return NULL;
    }

    while (1) {
        // 等待事件
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            close(fdd1);
            close(fdd3);
            close(epoll_fd);
            return NULL;
        }

        // 处理事件
        for (i = 0; i < nfds; i++) {
            if (events[i].events & EPOLLIN) {
                ssize_t bytes = read(events[i].data.fd, &ie, sizeof(ie));
                if (bytes == -1) {
                    if (errno != EAGAIN) {
                        perror("read");
                        close(fdd1);
                        close(fdd3);
                        close(epoll_fd);
                        return NULL;
                    }
                } else if (bytes == sizeof(ie)) {
                    if (events[i].data.fd == fdd1) {
                        rot += ie.value * rot_scale;
                        if (rot > 100) rot = 100;
                        if (rot < 0) rot = 0;

                        printf("Rotary -> %d\n",rot);
                    } else if (events[i].data.fd == fdd3) {
                        if (ie.type == EV_KEY && ie.value == 1) {
                            char buf_tmp[1024];
                            sprintf(buf_tmp,"Key %d Pressed.\n",ie.code);
                            lv_label_set_text(guider_ui.screen_label_log,buf_tmp);
                            
                            key = ie.code;
                            r_cnt = 2;
                        }else if (ie.value == 0){
                            r_cnt --;
                            if (r_cnt == 0){
                                char buf_tmp[1024];
                                sprintf(buf_tmp,"Key %d Released.\n",ie.code);
                                lv_label_set_text(guider_ui.screen_label_log,buf_tmp);
                                key = 0;
                            }
                            
                        }
                    }
                }
            }
        }
    }

    close(fdd1);
    close(fdd3);
    close(epoll_fd);
    return NULL;
}

void* push_to_server(void *arg){
    // Push至服务器
    char tbf[1024];
    while (1){
        sprintf(tbf, "wget -q -O - \"http://%s/upload?x=%lf&y=%lf&a=%lf&z=%lf&rot=%d&key=%d\"", server_URL, xx, yy, aa, zz, rot, key);
        int r_code = system(tbf);
        if (r_code < 0)
            lv_label_set_text(guider_ui.screen_label_log,"Connection Failed.");
        usleep(5000);
    }
}

int main(void) {
    lv_init();
    fbdev_init();
    static lv_color_t buf[DISP_BUF_SIZE];
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res  = 240;
    disp_drv.ver_res  = 240;
    lv_disp_drv_register(&disp_drv);
    evdev_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    lv_indev_drv_register(&indev_drv);

    setup_ui(&guider_ui);
    events_init(&guider_ui);

    // 初始化配置
    FILE *js_cfg = fopen("/root/joystick.cfg", "r");
    if (js_cfg != NULL) {
        lv_label_set_text(guider_ui.screen_label_log,"Config Applied Successfully.");
        fscanf(js_cfg, "%s", server_URL);
        fscanf(js_cfg, "%lf%lf%lf%lf%lf", &eps, &cx, &cy, &cz, &ca, & rot_scale);
    } else {
        lv_label_set_text(guider_ui.screen_label_log,"Config Not Found, Assuming Default.");
        strcpy(server_URL, "192.168.31.82:5000");
    }

    // 读取电量
    FILE *fp_batt = popen("cat /sys/class/power_supply/battery/capacity", "r");
    if (fp_batt == NULL) {
        lv_label_set_text(guider_ui.screen_label_log,"Read Battery Capacity Failed.");
    }else{
        fscanf(fp_batt, "%d", &batt);
        pclose(fp_batt);

        char buf_tmp[1024];
        sprintf(buf_tmp,"%d%% ",batt);
        lv_label_set_text(guider_ui.screen_label_batt,buf_tmp);
    }

    int fd = open_device();
    if(fd == -1) {
        return 1;
    }

    // 控制蜂鸣器发出1000Hz频率的声音，持续200毫秒
    control_beeper(fd, 1000, 200);
    sleep(1);
    control_beeper(fd, 0, 0);
    close_device(fd);

    pthread_t thread1, thread2, thread3;
    pthread_create(&thread1, NULL, joystick_thread, NULL);
    pthread_create(&thread2, NULL, event_thread, NULL);
    pthread_create(&thread3, NULL, push_to_server, NULL);

    // 主循环
    while(1) {
        lv_timer_handler();
        usleep(5000);
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

    return 0;
}

/* Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR` */
uint32_t custom_tick_get(void) {
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
