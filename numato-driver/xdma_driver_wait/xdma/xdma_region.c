#include "xdma_region.h"

static struct base region_base;

void *get_base(void)
{
    return &region_base;
}
EXPORT_SYMBOL_GPL(get_base);

int set_engine_base(void *base, int engine_idx)
{
    if (engine_idx >= ENGINE_NUM)
        return -1;
    region_base.engine[engine_idx] = (struct crypto_engine *)base;
    return 0;
}

int set_led_base(void *base)
{
    region_base.led = (struct led_region *)base;
    return 0;
}

void toggle_red_led(void)
{
    u32 led_register = ioread32(region_base.led);
    u32 red = led_register ^ 0x00000001;
    iowrite32(red, region_base.led);
}
void toggle_blue_led(void)
{
    u32 led_register = ioread32(region_base.led);
    u32 blue = led_register ^ 0x00000002;
    iowrite32(blue, region_base.led);
}

/** BUFER ZONE */

void *get_next_region_ep_addr(int engine_idx)
{
    struct crypto_engine *base = region_base.engine[engine_idx];

    u32 head = ioread32(&base->comm.head_inb);

    return &base->in.region[head + 1];
}
u32 get_tail_inb_idx(int engine_idx)
{
    struct crypto_engine *base = region_base.engine[engine_idx];
    return ioread32(&base->comm.tail_inb);
}

void write_inb_xfer_id(int engine_idx, int region_idx, u32 xfer_id)
{
    struct crypto_engine *base = region_base.engine[engine_idx];
    iowrite32(xfer_id, &base->in.region[region_idx].xfer_id);
}
void *get_region_ep_addr_out(int engine_idx)
{
    struct crypto_engine *base = region_base.engine[engine_idx];

    u32 tail = ioread32(&base->comm.tail_outb);

    return &base->out.region[tail];
}
void *get_region_ep_addr(int engine_idx, int region_idx)
{
    struct crypto_engine *base = region_base.engine[engine_idx];

    return &base->in.region[region_idx];
}
u64 get_next_data_ep_addr(int engine_idx)
{
    struct crypto_engine *base = region_base.engine[engine_idx];

    u32 head = ioread32(&base->comm.head_inb);

    return offsetof(struct crypto_engine, in) + 
        offsetof(struct inbound, region) + 
        sizeof(struct region) * head +
        offsetof(struct region, data) ;    // return (void *)(&base->in.region[head + 1].data) - (void *)base;
}
u64 get_data_ep_addr_out(int engine_idx)
{
    struct crypto_engine *base = region_base.engine[engine_idx];

    u32 tail = ioread32(&base->comm.tail_outb);

    return offsetof(struct crypto_engine, out) + 
        offsetof(struct outbound, region) + 
        sizeof(struct region) * tail +
        offsetof(struct region, data) ;    // return (void *)(&base->in.region[head + 1].data) - (void *)base;
}

int is_engine_full(int engine_idx)
{

    u32 head = ioread32(&region_base.engine[engine_idx]->comm.head_inb);
    u32 tail = ioread32(&region_base.engine[engine_idx]->comm.tail_inb);
    
    pr_info("head = %x", head);
    pr_info("tail = %x", tail);

    if (head < 0 || tail < 0)
    {
        return -1;
    }
        
    if (((head + 1) % REGION_NUM) == (tail % REGION_NUM))
        return 1;
    else
        return 0;
}

int is_engine_full_out(int engine_idx)
{

    u32 head = ioread32(&region_base.engine[engine_idx]->comm.head_outb);
    u32 tail = ioread32(&region_base.engine[engine_idx]->comm.tail_outb);
    
    pr_info("head = %x", head);
    pr_info("tail = %x", tail);

    if (head < 0 || tail < 0)
    {
        return -1;
    }
        
    if (((head + 1) % REGION_NUM) == (tail % REGION_NUM))
        return 1;
    else
        return 0;
}

int is_engine_empty_out(int engine_idx)
{

    u32 head = ioread32(&region_base.engine[engine_idx]->comm.head_outb);
    u32 tail = ioread32(&region_base.engine[engine_idx]->comm.tail_outb);
    if (head < 0 || tail < 0)
        return -1;
    
    if (((head) % REGION_NUM) == (tail % REGION_NUM))
        return 1;
    else
        return 0;
}
void active_inb_region(int engine_idx, int region_idx)
{
    struct crypto_engine *base = region_base.engine[engine_idx];

    struct region *region = &base->in.region[region_idx];
    iowrite32(0xABCDABCD, &region->region_dsc);
}

int increase_head_inb_idx(int engine_idx, int booking)
{
    u32 head_idx, tail_idx;
    u32 region_dsc;
    int i;
    void *head_addr = &region_base.engine[engine_idx]->comm.head_inb;
    void *tail_addr = &region_base.engine[engine_idx]->comm.tail_inb;
    head_idx = ioread32(head_addr);
    tail_idx = ioread32(tail_addr);
    
    for (i = head_idx; i == (booking % REGION_NUM); i = (i + 1) % REGION_NUM)
    {
        region_dsc = ioread32(&region_base.engine[engine_idx]->in.region[i].region_dsc);
        if (region_dsc == 0xABCDABCD)
        {
            active_inb_region(engine_idx, i);
        }
        else
        {
            return 0;
        }
    }

    return 0;
}
int increase_tail_idx_out(int engine_idx)
{
    u32 tail_idx;
    void *tail_addr = &region_base.engine[engine_idx]->comm.tail_outb;
    tail_idx = ioread32(tail_addr);
    iowrite32(tail_idx + 1, tail_addr);

    return 0;
}

int active_next_region(int engine_idx)
{
    struct region *region = get_next_region_ep_addr(engine_idx);
    iowrite32(0xABCDABCD, &region->region_dsc);
    return 0;
}
int active_next_region_out(int engine_idx)
{
    struct region *region = get_region_ep_addr_out(engine_idx);
    iowrite32(0xABCDABCD, &region->region_dsc);
    return 0;
}



void test_mem(void)
{

    int i;
    printk("head_inb   = %d", ioread32(&region_base.engine[0]->comm.head_inb));
    printk("tail_inb   = %d", ioread32(&region_base.engine[0]->comm.tail_inb));
    
    for (i =0; i < REGION_NUM; i++){
        printk("region_dsc = %d", ioread32(&region_base.engine[0]->in.region[i].region_dsc));
        printk("region_dsc = %d", ioread32(&region_base.engine[0]->in.region[i].data_len));
    }
    
}
EXPORT_SYMBOL_GPL(test_mem);