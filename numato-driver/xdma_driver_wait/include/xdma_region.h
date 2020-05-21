#ifndef _xdma_region_
#define _xdma_region_

#include <linux/types.h>
#include <linux/uaccess.h>

#define REGION_NUM (8)
#define DATA_MAX_LEN (1600 - 44)
#define ENGINE_NUM (1)
#define ENGINE_OFFSET(x) (0 + x*1)
#define LED_OFFSET (6464)


struct common_base {
    u32 h2c_buff_size;
    u32 h2c_buff_size_;

    u32 c2h_buff_size;
    u32 c2h_buff_size_;

    u32 padding[3];

    u32 head_inb;
    u32 tail_inb;

    u32 padding_;

    u32 head_outb;
    u32 tail_outb;

    u32 padding_last[6];
};

struct crypto_dsc {
    u32 data[7];
};

struct region {
    u32 region_dsc;
    u32 xfer_id;
    u8 xfer_dsc;
    u8 crypto_result;
    u16 data_len;
    u32 data_len__;
    struct crypto_dsc crypto_dsc;
    u8 data[DATA_MAX_LEN];
};

struct inbound {
    struct region region[REGION_NUM];
};

struct outbound {
    struct region region[REGION_NUM];
};


struct crypto_engine {
    struct common_base comm;
    struct inbound in;
    u32 padding[880];
    struct outbound out;
};

struct led_region {
    union {
        u32 red : 1;
        u32 blue: 1;
    } led;
};

struct base {
    struct crypto_engine *engine[ENGINE_NUM];
    struct led_region *led;
};


int set_engine_base(void *base, int engine_idx);
int set_led_base(void *base);
void toggle_red_led(void);
void toggle_blue_led(void);
int is_engine_full(int engine_idx);
int is_engine_empty(int engine_idx);
void *get_next_region_ep_addr(int engine_idx);
void *get_next_data_ep_addr(int engine_idx);
int increase_head_idx(int engine_idx);
int active_next_region(int engine_idx);


#endif