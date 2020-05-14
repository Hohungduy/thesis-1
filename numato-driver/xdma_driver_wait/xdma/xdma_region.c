#include "xdma_region.h"

struct dsc {
    u32 region_dsc;
    u32 xfer_id;
    u8 xfer_dsc;
    u8 crypto_res;
    u8 data_len;
    u8 magic;
    u32 next;
};

spinlock_t lock;

static struct global_mem *config_base;

void set_base(void *base)
{
    config_base = base;
    spin_lock_init(&lock);
}

static inline u32 get_next_inb_region_addr(void)
{
    return ioread32(&config_base->
        cb.NEXT_INB_REGION);
}
static inline u32 get_next_outb_region_addr(void)
{
    return ioread32(&config_base->
        cb.NEXT_OUTB_REGION);
}

static inline u32 get_final_inb_region_addr(void)
{
    return ioread32(&config_base->
        cb.FINAL_INB_REGION);
}

static inline u32 get_final_outb_region_addr(void)
{
    return ioread32(&config_base->
        cb.FINAL_OUTB_REGION);
}

inline u32 get_h2c_buffer_size(void)
{
    return ioread32(&config_base->
        cb.H2C_BUFFER_SIZE);
}

inline u32 get_c2h_buffer_size(void)
{
    return ioread32(&config_base->
        cb.C2H_BUFFER_SIZE);
}

#ifdef DEBUG_REGION
#define debug_mem debug_mem

static void _debug_mem(u32 start, u32 end)
{
    u32 idx = 0;
    u8 *buff;

    start = (start >> 2) << 2;
    end   = (end >> 2) << 2;

    if (end < start){
        pr_err(" end < start ?\n");
        return;
    }
    buff = (u8 *)kzalloc(end - start, GFP_KERNEL);
    memcpy_fromio(buff, config_base + start, end - start);

    for (idx = start; idx < end; idx += 4*4){
        pr_info("%08x = %08x %08x %08x %08x", idx,  
            ioread32(config_base + idx), ioread32(config_base + idx + 4),
            ioread32(config_base + idx + 8), ioread32(config_base + idx +12));
    }
    kfree(buff);
}

void debug_mem(void)
{
    _debug_mem(0x00000000, MAX_REGION_ADDR_TEST);
}

#else
#define debug_mem(...)
#endif

void reset_mem(void)
{
    memset(config_base, 0, MAX_REGION_ADDR_TEST);
}

void create_dsc(void)
{
    int idx;
    struct dsc dsc[8];
    for (idx = 0; idx < 8; idx ++)
    {
        dsc[idx].region_dsc = REGION_STATE_NEED_PROCESS;
        dsc[idx].xfer_id = idx + 0xFF00;
        dsc[idx].xfer_dsc= XFER_C2H;
        dsc[idx].crypto_res = CRYPTO_RES_DONE;
        dsc[idx].data_len = 1500 + idx;
        dsc[idx].magic = 0x3d;
        dsc[idx].next = DSC_REQ_OFFSET + DSC_REQ_SIZ*(idx+1);
    }
    dsc[7].next = 0;

    memcpy_toio(config_base + DSC_REQ_OFFSET,
            &dsc,  8*sizeof(struct dsc));
}

// void create_data(void)
// {
//     u32 idx;
//     for (idx = DATA_REQ_OFF_SET; 
//         idx < DATA_REQ_OFF_SET + 8*DATA_REQ_SIZ; idx += 16)
//     {
//         memset32(config_base + idx, idx, 1);
//         memset32(config_base + idx + 4, idx + 4, 1);
//         memset32(config_base + idx + 8, idx + 8, 1);
//         memset32(config_base + idx +12, idx + 12, 1);
//     }
// }

// void create_global_region_for_testing(void)
// {
//     reset_mem();

//     iowrite32(8, config_base + H2C_CH0_BUFFER_SIZE_OFFSET);
//     iowrite32(0, config_base + H2C_CH1_BUFFER_SIZE_OFFSET);
//     iowrite32(8, config_base + C2H_CH0_BUFFER_SIZE_OFFSET);
//     iowrite32(0, config_base + C2H_CH1_BUFFER_SIZE_OFFSET);
//     iowrite32(8, config_base + REMAINING_REQUEST_OFFSET);
//     iowrite32(0x44, config_base + NEXT_DSC_REQUEST_ADDRESS_OFFSET);
//     iowrite32(0xb4, config_base + FINAL_DSC_REQUEST_ADDRESS_OFFSET);
//     create_dsc();
//     create_data();
// }

// int process_next_req(void)
// {
//     u32 next_req_addr;
//     u32 xfer_id = 0;
//     u8 crypto_res = 0;
    
//     int idx;
//     unsigned long flags;
//     struct dsc next_dsc;

//     spin_lock_irqsave(&lock, flags);
//     if (unlikely(is_free_req()))
//         goto process_done;
//     get_next_dsc_req(&next_dsc);
//     if (unlikely(next_dsc.magic != MAGIC_BTYE))
//         goto invalid_dsc;

//     if (likely(next_dsc.xfer_dsc == XFER_C2H))
//         goto move_data;
    
//     xfer_id = next_dsc.xfer_id;
//     crypto_res = next_dsc.crypto_res;
//     if (unlikely(crypto_res != CRYPTO_RES_DONE))
//         goto submit_crypto_req;
    

// move_data:

// submit_crypto_req:
    

// process_done:
//     spin_unlock_irqrestore(&lock, flags);
//     return 0;

// invalid_dsc:
//     pr_err("Invalid dsc\n");
//     return -1;
// }

int is_real_mem_available(void)
{

    return 0;
}

int is_buff_available(void)
{
    
}

int is_backlog_available(void)
{

}
