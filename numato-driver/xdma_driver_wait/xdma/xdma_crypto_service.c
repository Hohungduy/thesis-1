#include "xdma_crypto_service.h"
/**
 * 
 */

#define MASK_IRQ_0 (1 << 0)
#define TEST_CHANNEL (0)
#define TEST_ADDR (0x000000000000000F)
#define TEST_TIMEOUT (3000)
#define TEST_SIZE (500)
#define WRITE_DIRECTION (1)
#define READ_DIRECTION (0)
#define TEST_ADDRESS_START (0x00000000000000AB)
#define TEST_ADDRESS_READ (0x00000000000000CC)
#define TEST_ADDRESS_READ_OFFSET (TEST_ADDRESS_READ - TEST_ADDRESS_START)


struct my_data {
    struct work_struct work;
    void *dev_handler;
} recv_data;

int get_sg_from_buf(void **buff, struct scatterlist *sg)
{
    struct page *pg;
    int i;


    for (i = 0; i < 3; i++){

        unsigned int offset = offset_in_page(buff[i]);
        unsigned int nbytes = 
            min_t(unsigned int, PAGE_SIZE - offset, TEST_SIZE);

        pg = virt_to_page(buff[i]);
        if (!pg){
            pr_info("Cannot convert buffer to page");
            return FALSE;
        }
        flush_dcache_page(pg);
        sg_set_page(sg + i, pg, nbytes, offset);
    }
    return true;
}

void my_work_handler(struct work_struct *work)
{
    void *hndl = recv_data.dev_handler;
    int channel = TEST_CHANNEL;
    bool write = READ_DIRECTION;
    bool dma_mapped = FALSE;
    u64 ep_addr = TEST_ADDRESS_START;
    struct sg_table *sgt;
    struct scatterlist *scatter;
    int timeout_ms = TEST_TIMEOUT;
    u8 *buff[3];
    int i,j, res;
    sgt = (struct sg_table *)kmalloc(sizeof(*sgt), GFP_DMA | GFP_ATOMIC);
    if (!sgt){
        pr_info("No mem\n");
        return;
    }
    memset(sgt, 0, sizeof(*sgt));

    scatter = (struct scatterlist *)kmalloc(3*sizeof(struct scatterlist), GFP_DMA | GFP_ATOMIC);
    if (!scatter){
        pr_info("No mem\n");
        return;
    }
    memset(scatter, 0, 3*sizeof(*sgt));

    for (i = 0; i < 3; i++){
        buff[i] = (u8 *)kmalloc(TEST_SIZE*sizeof(u8), GFP_DMA | GFP_ATOMIC);
        memset(buff[i], 0, TEST_SIZE*sizeof(u8));
    }

    get_sg_from_buf((void **)buff, scatter);
    sgt->sgl = scatter;
    sgt->nents = 3;
    sgt->orig_nents = 3;

    pr_info("Read from card\n");

    pr_info("Before reading\n");
    for (j = 0; j < 3; j++){
        for (i = 0; i < 500; i += sizeof(long long int)){
            pr_info(" Address %x = %llx \n", TEST_ADDRESS_START + i + j*500,  
                    *((long long int *)(&buff[0][i])));
        }
    }
    
    res = xdma_xfer_submit(hndl, channel, write, ep_addr, 
                sgt, dma_mapped, timeout_ms);

    pr_info("After reading\n");
    for (j = 0; j < 3; j++){
        for (i = 0; i < 500; i += sizeof(long long int)){
            pr_info(" Address %x = %llx \n", TEST_ADDRESS_START + i + j*500,  
                    *((long long int *)(&buff[0][i])));
        }
    }
    kfree(buff[0]);
    kfree(buff[1]);
    kfree(buff[2]);
    kfree(scatter);
    kfree(sgt);
}

static void send_request_test_blocking( struct xdma_pci_dev *xpdev)
{
    void *hndl = xpdev->xdev;
    int channel = TEST_CHANNEL;
    bool write = WRITE_DIRECTION;
    bool dma_mapped = FALSE;
    u64 ep_addr = TEST_ADDRESS_START;
    struct sg_table *sgt;
    struct scatterlist *scatter;
    u8 *buff[3];
    int timeout_ms = TEST_TIMEOUT;
    int i, j, res;
    sgt = (struct sg_table *)kmalloc(sizeof(*sgt), GFP_KERNEL);
    if (!sgt){
        pr_info("No mem\n");
        return;
    }
    memset(sgt, 0, sizeof(*sgt));

    scatter = (struct scatterlist *)kmalloc(3*sizeof(struct scatterlist), GFP_KERNEL);
    if (!scatter){
        pr_info("No mem\n");
        return;
    }
    memset(scatter, 0, 3*sizeof(*sgt));

    buff[0] = (u8 *)kmalloc(TEST_SIZE*sizeof(u8), GFP_KERNEL | GFP_DMA);
    for(i = 0; i < TEST_SIZE; i ++ ){
        buff[0][i] = (u8)(i % 200);
    }

    buff[1] = (u8 *)kmalloc(TEST_SIZE*sizeof(u8), GFP_KERNEL | GFP_DMA);
    for(i = 0; i < TEST_SIZE; i ++ ){
        buff[1][i] = (u8)(i % 200);
    }

    buff[2] = (u8 *)kmalloc(TEST_SIZE*sizeof(u8), GFP_KERNEL | GFP_DMA);
    for(i = 0; i < TEST_SIZE; i ++ ){
        buff[2][i] = (u8)(i % 200);
    }

    pr_info("Writing Buffer\n");
    for (j = 0; j < 3; j++){
        for (i = 0; i < 500; i += sizeof(long long int)){
            pr_info(" Address %x = %llx \n", TEST_ADDRESS_START + i + j*500,  
                    *((long long int *)(&buff[0][i])));
        }
    }
    
    get_sg_from_buf((void **)buff, scatter);
    sgt->sgl = scatter;
    sgt->nents = 3;
    sgt->orig_nents = 3;
    res = xdma_xfer_submit(xpdev->xdev, channel, write, ep_addr, 
                sgt, dma_mapped, timeout_ms);

    recv_data.dev_handler = hndl;
    INIT_WORK(&recv_data.work, my_work_handler);

    kfree(buff[0]);
    kfree(buff[1]);
    kfree(buff[2]);
    kfree(scatter);
    kfree(sgt);
}

irqreturn_t user_handler(int irq_no, void *dev_id)
{
    struct xdma_pci_dev *xpdev = (struct xdma_pci_dev *)dev_id;
    void *hndl = xpdev->xdev;

    pr_info("irq_no %d handler\n", irq_no);

    schedule_work(&recv_data.work);

    xdma_user_isr_disable(hndl, 1 << 0);

    return IRQ_HANDLED;
}

int xpdev_create_crypto_service(struct xdma_pci_dev *xpdev){
    u32 mask = MASK_IRQ_0;
    int rv;
    
    rv = xdma_user_isr_register(xpdev->xdev, mask, user_handler, (void *)xpdev);
    if (rv){
        pr_err("register user_irq_no %d failed\n", 0);
        return rv;
    }
        pr_info("register user_irq_no %d done\n", 0);

    pr_info("Send request to crypto dma\n");
    send_request_test_blocking(xpdev);
    pr_info("Sent\n");    

    return 0;
}