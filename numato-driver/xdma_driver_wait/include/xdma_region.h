#ifndef XDMA_REGION
#define XDMA_REGION

#include <linux/version.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/workqueue.h>
#include "libxdma.h"

#define H2C_CH0_BUFFER_SIZE_OFFSET (0x00)
#define H2C_CH1_BUFFER_SIZE_OFFSET (0x04)
#define C2H_CH0_BUFFER_SIZE_OFFSET (0x08)
#define C2H_CH1_BUFFER_SIZE_OFFSET (0x0C)
#define REMAINING_REQUEST_OFFSET   (0x20)
#define NEXT_DSC_REQUEST_ADDRESS_OFFSET   (0x24)
#define FINAL_DSC_REQUEST_ADDRESS_OFFSET  (0x28)
#define DSC_REQ_SIZ                       (0x10)
#define DSC_REQ_OFFSET                    (0x44)
#define DATA_REQ_SIZ                      (0x5F0)
#define DATA_REQ_OFF_SET                  (0xD0)
#define MAGIC_BTYE                        (0x3D)
#define MAX_REGION_ADDR                   (0x3100)
#define MAX_REGION_ADDR_TEST              (0x00001000)
#define DEBUG_REGION 1

struct dsc;

enum region_state {
    REGION_STATE_EMPTY,
    REGION_STATE_NEED_PROCESS,
    REGION_STATE_PROCESSING,
    REGION_STATE_PROCESSED,
    REGION_STATE_UNKNOWN
};

extern void set_base(void __iomem* base);

extern u32 get_remaining_request(void);

extern u32 get_h2c_buffer_size(int channel);

extern u32 get_c2h_buffer_size(int channel);

extern void get_next_dsc_req(struct dsc *dsc);

extern void get_final_dsc_req(struct dsc *dsc);

extern bool is_only_last_req(void);

extern bool is_free_req(void);

extern inline bool is_dsc_valid(struct dsc *dsc);

inline enum region_state get_region_state(struct dsc *dsc);

extern inline bool is_c2h_xfer(struct dsc *dsc);

#ifdef DEBUG_REGION
#define debug_mem debug_mem

void debug_mem(void);

#else
#define debug_mem(...)
#endif

#endif