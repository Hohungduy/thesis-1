/*******************************************************************************
 *
 *  NetFPGA-10G http://www.netfpga.org
 *
 *  File:
 *        nf10driver.c
 *
 *  Project:
 *        nic
 *
 *  Author:
 *        Mario Flajslik
 *
 *  Description:
 *        Top level file for the nic driver. Contains functions that are called
 *        when module is loaded/unloaded to initialize/remove PCIe device and
 *        nic datastructures.
 *
 *  Copyright notice:
 *        Copyright (C) 2010, 2011 The Board of Trustees of The Leland Stanford
 *                                 Junior University
 *
 *  Licence:
 *        This file is part of the NetFPGA 10G development base package.
 *
 *        This file is free code: you can redistribute it and/or modify it under
 *        the terms of the GNU Lesser General Public License version 2.1 as
 *        published by the Free Software Foundation.
 *
 *        This package is distributed in the hope that it will be useful, but
 *        WITHOUT ANY WARRANTY; without even the implied warranty of
 *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *        Lesser General Public License for more details.
 *
 *        You should have received a copy of the GNU Lesser General Public
 *        License along with the NetFPGA source package.  If not, see
 *        http://www.gnu.org/licenses/.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/pci.h>
#include "nf10driver.h"
#include "nf10fops.h"
#include "nf10iface.h"

// These attributes have been removed since Kernel 3.8.x. Keep them here for backward
// compatibility.
// See https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=54b956b903607
#ifndef __devinit
  #define __devinit
#endif
#ifndef __devexit
  #define __devexit
#endif
#ifndef __devexit_p
  #define __devexit_p
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Le Thanh Long");
MODULE_DESCRIPTION("numato driver");


#define PCI_VENDOR_ID_NUMATO 0x10ee
#define PCI_DEVICE_ID_NUMATO 0x7024
#define BAR0_SIZE 0x10000ULL
#define PCIE_BITS 32

static struct pci_device_id pci_id[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_NUMATO, PCI_DEVICE_ID_NUMATO)},
    {0}
};
MODULE_DEVICE_TABLE(pci, pci_id);

void aller_numato_test(struct nf10_card *card){
    volatile void *tx_dsc = card->tx_dsc;
    /*

        Test TX region

    */
    *(((uint64_t*)tx_dsc)) = 0x12345672;

    if ( (*(((uint64_t*)tx_dsc))) == 0x12345672 ){
        printk("NUMATO TEST OKAY %llx\n", (*(((uint64_t*)tx_dsc) )));
    }
    else{
        printk("NUMATO TEST FAILED\n");
    }
        


    // *(((uint64_t*)tx_dsc) + 15) = 0x12345675;

    // if ( (*(((uint64_t*)tx_dsc) + 15)) == 0x12345675 ){
    //     printk("NUMATO TEST OKAY %llx\n", (*(((uint64_t*)tx_dsc) + 15)));
    // }
    // else{
    //     printk("NUMATO TEST FAILED\n");
    // }
    
}

static int __devinit nf10_probe(struct pci_dev *pdev, const struct pci_device_id *id){
	int err;
    // int i;
    int ret = -ENODEV;
    struct nf10_card *card;

    printk(KERN_ERR "NUMATO: Probe function!\n");

	// enable device
	if((err = pci_enable_device(pdev))) {
		printk(KERN_ERR "NUMATO: Unable to enable the PCI device!\n");
        ret = -ENODEV;
		goto err_out_none;
	}

    printk(KERN_ERR "NUMATO: Enable PCI device done!\n");

    // set DMA addressing masks (full 64bit)
    if(dma_set_mask(&pdev->dev, DMA_BIT_MASK(PCIE_BITS)) < 0){
        printk(KERN_ERR "NUMATO: dma_set_mask fail!\n");
        ret = -EFAULT;
        goto err_out_disable_device;

        if(dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(PCIE_BITS)) < 0){
            printk(KERN_ERR "NUMATO: dma_set_mask fail!\n");
            ret = -EFAULT;
            goto err_out_disable_device;
        }
    }

    // enable BusMaster (enables generation of pcie requests)
	pci_set_master(pdev);

    // enable MSI
    if(pci_enable_msi(pdev) != 0){
        printk(KERN_ERR "NUMATO: failed to enable MSI interrupts\n");
        ret = -EFAULT;
		goto err_out_clear_master;
    }
	
    // be nice and tell kernel that we'll use this resource
	printk(KERN_ERR "NUMATO: Reserving memory region for NUMATO\n");
printk(KERN_ERR "NUMATO: pci_resource_start(pdev, 0) = %p\n", pci_resource_start(pdev, 0));
    printk(KERN_ERR "NUMATO: Reserving memory region for NUMATO %llu \n", pci_resource_len(pdev, 0));
	if (!request_mem_region(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0), DEVICE_NAME)) {
		printk(KERN_ERR "NUMATO: Reserving memory region failed\n");
        ret = -ENOMEM;
        goto err_out_msi;
	}
 //    printk(KERN_INFO "NUMATO: Reserving memory region for NF10 %llu \n", pci_resource_len(pdev, 2));
	// if (!request_mem_region(pci_resource_start(pdev, 2), pci_resource_len(pdev, 2), DEVICE_NAME)) {
	// 	printk(KERN_ERR "NUMATO: Reserving memory region failed\n");
 //        ret = -ENOMEM;
	// 	goto err_out_release_mem_region1;
	// }

    // create private structure
	card = (struct nf10_card*)kmalloc(sizeof(struct nf10_card), GFP_KERNEL);
	if (card == NULL) {
		printk(KERN_ERR "NUMATO: Private card memory alloc failed\n");
		ret = -ENOMEM;
		goto err_out_release_mem_region2;
	}
	memset(card, 0, sizeof(struct nf10_card));
    card->pdev = pdev;
	
    // map the cfg memory
	// printk(KERN_INFO "nf10: mapping cfg memory\n");
 //    card->cfg_addr = ioremap_nocache(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	// if (!card->cfg_addr)
	// {
	// 	printk(KERN_ERR "NUMATO: cannot mem region len:%lx start:%lx\n",
	// 		(long unsigned)pci_resource_len(pdev, 0),
	// 		(long unsigned)pci_resource_start(pdev, 0));
	// 	goto err_out_iounmap;
	// }

	printk(KERN_ERR "NUMATO: mapping mem memory\n");

    card->tx_dsc = ioremap_nocache(pci_resource_start(pdev, 0) , pci_resource_len(pdev, 0));
   //card->rx_dsc = ioremap_nocache(pci_resource_start(pdev, 0) + 1 * 0x00100000ULL, 0x00100000ULL);
	printk(KERN_ERR "NUMATO: mapping mem memory %p\n",card->tx_dsc );
	if (!card->tx_dsc )
	{
		printk(KERN_ERR "NUMATO: cannot mem region len:%lx start:%lx\n",
			(long unsigned)pci_resource_len(pdev, 0),
			(long unsigned)pci_resource_start(pdev, 0));
		goto err_out_iounmap;
	}
    /*
    // reset
    *(((uint64_t*)card->cfg_addr)+30) = 1;
    msleep(1);

    // set buffer masks
    card->tx_dsc_mask = 0x000007ffULL;
    card->rx_dsc_mask = 0x000007ffULL;
    card->tx_pkt_mask = 0x00007fffULL;
    card->rx_pkt_mask = 0x00007fffULL;
    card->tx_dne_mask = 0x000007ffULL;
    card->rx_dne_mask = 0x000007ffULL;
    
    if(card->tx_dsc_mask > card->tx_dne_mask){
        *(((uint64_t*)card->cfg_addr)+1) = card->tx_dne_mask;
        card->tx_dsc_mask = card->tx_dne_mask;
    }
    else if(card->tx_dne_mask > card->tx_dsc_mask){
        *(((uint64_t*)card->cfg_addr)+7) = card->tx_dsc_mask;
        card->tx_dne_mask = card->tx_dsc_mask;
    }

    if(card->rx_dsc_mask > card->rx_dne_mask){
        *(((uint64_t*)card->cfg_addr)+9) = card->rx_dne_mask;
        card->rx_dsc_mask = card->rx_dne_mask;
    }
    else if(card->rx_dne_mask > card->rx_dsc_mask){
        *(((uint64_t*)card->cfg_addr)+15) = card->rx_dsc_mask;
        card->rx_dne_mask = card->rx_dsc_mask;
    }
    */

    // allocate buffers to play with
    //card->host_tx_dne_ptr = pci_alloc_consistent(pdev, card->tx_dne_mask+1, &(card->host_tx_dne_dma));
    //card->host_rx_dne_ptr = pci_alloc_consistent(pdev, card->rx_dne_mask+1, &(card->host_rx_dne_dma));

    // if( (card->host_tx_dne_ptr == NULL) ){
        
    //     printk(KERN_ERR "NUMATO: cannot allocate dma buffer\n");
    //     goto err_out_free_private2;
    // }
    /*

    *(((uint64_t*)card->cfg_addr)+16) = card->host_tx_dne_dma;
    *(((uint64_t*)card->cfg_addr)+17) = card->tx_dne_mask;
    *(((uint64_t*)card->cfg_addr)+18) = card->host_rx_dne_dma;
    *(((uint64_t*)card->cfg_addr)+19) = card->rx_dne_mask;
    */

    // store private data to pdev
	pci_set_drvdata(pdev, card);

    aller_numato_test(card);

    // ret = nf10iface_probe(pdev, card);
    // if(ret < 0){
    //     printk(KERN_ERR "nf10: failed to initialize interfaces\n");
    //     goto err_out_free_private2;
    // }

    ret = nf10fops_probe(pdev, card);
    if(ret < 0){
        printk(KERN_ERR "nf10: failed to initialize dev file\n");
        goto err_out_free_private2;
    }
    else{
        printk(KERN_INFO "nf10: device ready\n");
        return ret;
    }

 // error out
 err_out_free_private2:
    pci_free_consistent(pdev, card->tx_dne_mask+1, card->host_tx_dne_ptr, card->host_tx_dne_dma);
    //pci_free_consistent(pdev, card->rx_dne_mask+1, card->host_rx_dne_ptr, card->host_rx_dne_dma);
 err_out_iounmap:
    if(card->tx_dsc) iounmap(card->tx_dsc);
	pci_set_drvdata(pdev, NULL);
	kfree(card);
 err_out_release_mem_region2:
	release_mem_region(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
 err_out_msi:
    pci_disable_msi(pdev);
 err_out_clear_master:
    pci_clear_master(pdev);
 err_out_disable_device:
	pci_disable_device(pdev);
 err_out_none:
	return ret;
}

static void __devexit nf10_remove(struct pci_dev *pdev){
    struct nf10_card *card;

    // free private data
    printk(KERN_INFO "NUMATO: releasing private memory\n");
    card = (struct nf10_card*)pci_get_drvdata(pdev);
    if(card){

        nf10fops_remove(pdev, card);
        nf10iface_remove(pdev, card);

        //if(card->cfg_addr) iounmap(card->cfg_addr);

        if(card->tx_dsc) iounmap(card->tx_dsc);
        //if(card->rx_dsc) iounmap(card->rx_dsc);
        /*
        pci_free_consistent(pdev, card->tx_dne_mask+1, card->host_tx_dne_ptr, card->host_tx_dne_dma);
        pci_free_consistent(pdev, card->rx_dne_mask+1, card->host_rx_dne_ptr, card->host_rx_dne_dma);

        if(card->tx_bk_dma_addr) kfree(card->tx_bk_dma_addr);
        if(card->tx_bk_skb) kfree(card->tx_bk_skb);
        if(card->tx_bk_size) kfree(card->tx_bk_size);
        if(card->tx_bk_port) kfree(card->tx_bk_port);
        if(card->rx_bk_dma_addr) kfree(card->rx_bk_dma_addr);
        if(card->rx_bk_skb) kfree(card->rx_bk_skb);
        if(card->rx_bk_size) kfree(card->rx_bk_size);
        
        kfree(card);
        */
    }

    pci_set_drvdata(pdev, NULL);

    // release memory
    printk(KERN_INFO "NUMATO: releasing mem region\n");
	release_mem_region(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	//release_mem_region(pci_resource_start(pdev, 2), pci_resource_len(pdev, 2));

    // disabling device
    printk(KERN_INFO "NUMATO: disabling device\n");
    pci_disable_msi(pdev);
    pci_clear_master(pdev);
	pci_disable_device(pdev);
}

pci_ers_result_t nf10_pcie_error(struct pci_dev *dev, enum pci_channel_state state){
    printk(KERN_ALERT "NUMATO: PCIe error: %d\n", state);
    return PCI_ERS_RESULT_RECOVERED;
}

static struct pci_error_handlers pcie_err_handlers = {
    .error_detected = nf10_pcie_error
};

static struct pci_driver pci_driver = {
	.name = DEVICE_NAME,
	.id_table = pci_id,
	.probe = nf10_probe,
	.remove = __devexit_p(nf10_remove),
    .err_handler = &pcie_err_handlers
};

static int __init nf10_init(void)
{
	printk(KERN_ERR "\n\n\nNUMATO: module loaded\n");
	return pci_register_driver(&pci_driver);
}

static void __exit nf10_exit(void)
{
    pci_unregister_driver(&pci_driver);
	printk(KERN_ERR "NUMATO: module unloaded\n\n\n\n");
}

module_init(nf10_init);
module_exit(nf10_exit);
