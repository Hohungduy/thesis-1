/*
 * Support for Cryptographic Engine in FPGA card using PCIe interface
 * that can be found on the following platform: Armada. 
 *
 * Author: Duy H.Ho <duyhungho.work@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

//#include "mycrypto.h"
//#include "common.h"
#include "cipher.h"

/*	Test function for a typical handle request :
	- This function is registered in cra_init function.
	- Using for testing data flow.
*/

#define AAAA
//#define not_free

#ifdef AAAA
#define pr_aaa(...)
#else 
#define pr_aaa pr_err
#endif

int mycrypto_skcipher_handle_request(struct crypto_async_request *base)
{
	//int ret;
	
	struct skcipher_request *req = skcipher_request_cast(base);
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(req->base.tfm);
	// context for transformation object
	struct mycrypto_cipher_req *req_ctx = skcipher_request_ctx(req);
	//context for skcipher request
	struct mycrypto_dev *mydevice = ctx->mydevice;
	size_t len = (size_t)req->cryptlen;
	printk(KERN_INFO "Module mycrypto: handle request (copy to buffer)\n");
	//ret = mycrypto_handle_request(base,req_ctx,req->src,req->dst,req->cryptlen,0,0,req->iv);
	len = sg_pcopy_to_buffer(req->src, req_ctx->src_nents,
				 mydevice->buffer,
				 len, 0);
	// Turn on timer.
	return 0;
}

/*	Test function for a typical handle result :
	- This function is registered in cra_init function.
	- Using for testing data flow.
	- Using for testing with the lower layer of driver (pcie layer).
*/

int mycrypto_skcipher_handle_result(struct crypto_async_request *base, bool *should_complete)
{
	//int ret;
	
	// struct skcipher_request *req = skcipher_request_cast(base);
	// struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(req->base.tfm);
	//struct mycrypto_cipher_req *req_ctx = skcipher_request_ctx(req);
	//struct mycrypto_dev *mydevice = ctx->mydevice;
	//size_t len = (size_t)req->cryptlen;
	printk(KERN_INFO "Module mycrypto: handle result (copy from buffer)\n");
	// len = sg_pcopy_from_buffer(req->dst, req_ctx->dst_nents,
	// 			 mydevice->buffer,
	// 			 len, 0);
	*should_complete = true;
	return 0;
}

/* Using for checking:
   -  whether or not the length of request and block size accommodate the critetia.
   -  The number of entries in src and dst list.
*/
static int mycrypto_skcipher_req_init(struct skcipher_request *req)
{
	struct mycrypto_cipher_req *req_ctx = skcipher_request_ctx(req);
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	unsigned int blksize = crypto_skcipher_blocksize(tfm);
	int ret = 0;
	if (!IS_ALIGNED(req->cryptlen, blksize))
		return -EINVAL;
	req_ctx->src_nents = sg_nents_for_len(req->src, req->cryptlen);
	if (req_ctx->src_nents < 0) {
		printk(KERN_INFO "Invalid number of src SG\n");
		return req_ctx->src_nents;
	}
	req_ctx->dst_nents = sg_nents_for_len(req->dst, req->cryptlen);
	if (req_ctx->dst_nents < 0) {
		printk(KERN_INFO "Invalid number of dst SG\n");
		return req_ctx->dst_nents;
	}
	return ret;

}

/*
   This function facilitates us to process request with queue 
   - Copy all parameters for encr/decr process into the context of operation (transformation objects) 
   and the context of request.
   - Enqueue the request into crypto queue.
   - Set schedule for workqueue task to dequeue this request in the future.
   
*/
static int mycrypto_queue_skcipher_req(struct crypto_async_request *base,
			struct mycrypto_cipher_req *req_ctx,
			u32 dir, u32 mode)
{
	
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(base->tfm);
	// Get the context for transformation object
	struct skcipher_request *req = skcipher_request_cast(base);
	// Get skcipher request from its asysc request.
	struct mycrypto_dev *mydevice = ctx->mydevice;
	int ret;
	printk(KERN_INFO "Module mycrypto: enqueue request\n");

	req_ctx->dir = dir;
	ctx->mode = mode;
	ctx->dir =dir;
	ctx->len = AES_BLOCK_SIZE;
	ctx->iv = req->iv;
	ctx->assoclen = 0;
	ctx->cryptlen = req->cryptlen;
	ctx->authsize = 0;
	spin_lock(&mydevice->queue_lock);
	// enqueue request.
	ret = crypto_enqueue_request(&mydevice->queue, base);
	spin_unlock(&mydevice->queue_lock);
	
	// dequeue using workqueue
	queue_work(mydevice->workqueue,
		   &mydevice->work_data.work);

	// dequeue without using workqueue (testing ...)
	//mycrypto_dequeue_req(mydevice);
	return ret;
}

static int my_crypto_skcipher_aes_setkey(struct crypto_skcipher *cipher, const u8 *key,unsigned int len)
{
	struct crypto_tfm *tfm = crypto_skcipher_tfm(cipher);
	// Get/retrieve transformation object
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(tfm);
	// Get transformation context 
	//struct mycrypto_dev *mydevice = ctx->mydevice;
	struct crypto_aes_ctx aes; 
	/* @aes: The location where the processing key (computed key) will be store*/
	int ret,i;
	printk(KERN_INFO "Module mycrypto: mycrypto_skcipher_aes_setkey \n");
	ret =crypto_aes_expand_key(&aes, key,len);
	// get key and let it adapt standard criteria of aes, then store it into struct aes.
	if(ret){
		crypto_skcipher_set_flags(cipher,CRYPTO_TFM_RES_BAD_KEY_LEN);
		return ret;
	}
	// copy key stored in aes to ctx
	for(i = 0; i < len /sizeof(u32); i++)
		ctx->key[i] = cpu_to_le32(aes.key_enc[i]);
	ctx->keylen = len;
	//Beside, it is not necessary to fill aes.key_dec.
	//If you wanna continue, just refer to setkey function for skcipher 
	//in file cipher.c (mv_cesa)
		pr_aaa("                     %s, %d, %p", __func__, __LINE__, &aes);
	memzero_explicit(&aes, sizeof(aes));
	//free memory
	return 0;
}
static int my_crypto_cbc_aes_encrypt(struct skcipher_request *req)
{	
	int ret;
	//struct skcipher_request *req = skcipher_request_cast(base);
	//struct mycrypto_cipher_req *req_ctx = skcipher_request_ctx(req);
	printk(KERN_INFO "Module mycrypto: mycrypto_skcipher_aes_encrypt \n");
	/* checking the number of entries in scattergather list */
	ret = mycrypto_skcipher_req_init(req);
	if (ret)
		printk(KERN_INFO "ERROR SRC/DEST NUMBER OF ENTRY\n");
	//----------------------------------------------------
	// processing the queue of this request
	return mycrypto_queue_skcipher_req(&req->base, skcipher_request_ctx(req),
			MYCRYPTO_DIR_ENCRYPT, AES_MODE_CBC);
}
static int my_crypto_cbc_aes_decrypt(struct skcipher_request *req)
{
	
	int ret;
	//struct skcipher_request *req = skcipher_request_cast(base);
	//struct mycrypto_cipher_req *req_ctx = skcipher_request_ctx(req);
	ret = mycrypto_skcipher_req_init(req);
	printk(KERN_INFO "Module mycrypto: mycrypto_skcipher_aes_decrypt \n");
	if (ret)
		printk(KERN_INFO "ERROR SRC/DEST NUMBER OF ENTRY\n");
	return mycrypto_queue_skcipher_req(&req->base, skcipher_request_ctx(req),
			MYCRYPTO_DIR_DECRYPT, AES_MODE_CBC);
}

static int my_crypto_skcipher_cra_init(struct crypto_tfm *tfm)
{
	
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(tfm);
	struct mycrypto_alg_template *tmpl =
		container_of(tfm->__crt_alg, struct mycrypto_alg_template,
			     alg.skcipher.base);
	printk(KERN_INFO "Module mycrypto: my_crypto_skcipher_cra_init\n");
	// it means that tfm->__crt_alg
	crypto_skcipher_set_reqsize(__crypto_skcipher_cast(tfm),
				    sizeof(struct mycrypto_cipher_req));
	ctx->mydevice = tmpl->mydevice;
	ctx->base.handle_request = mycrypto_skcipher_handle_request;
	ctx->base.handle_result = mycrypto_skcipher_handle_result;

	return 0;
}
static void my_crypto_skcipher_cra_exit(struct crypto_tfm *tfm)
{
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(tfm);
	memzero_explicit(ctx, tfm->__crt_alg->cra_ctxsize);
}
//--------------------------------------------------------------------
static int my_crypto_aead_aes_setkey(struct crypto_aead *cipher, const u8 *key,unsigned int len)
{
	struct crypto_tfm *tfm = crypto_aead_tfm(cipher);
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(tfm);
	//struct my_crypto_hash_state istate,ostate;
	//struct mycrypto_dev *mydevice = ctx->mydevice;
	struct crypto_authenc_keys keys;
	/* @keys: The location where the processing key  
	(computed key for encrypt/decrypt and authentication) 
	will be store
	*/
	//--- Check condition of key (authenc style)
	printk(KERN_INFO "Module mycrypto:my_crypto_aead_aes_setkey \n");
	// if (crypto_authenc_extractkeys(&keys, key, len) != 0)
	// 	goto badkey;
	if (keys.enckeylen > sizeof(ctx->key))
		goto badkey;
	//-----------------------------------------------------------------
	//----------- Lack of authenc hash - set key -----------------------
	crypto_aead_set_flags(cipher, crypto_aead_get_flags(cipher) &
				    CRYPTO_TFM_RES_MASK);
	/* Now copy the keys into the context */
	memcpy(ctx->key, keys.enckey, keys.enckeylen);
	ctx->keylen = keys.enckeylen;
	//---------- Lack of authenc---------------
	//-----------------------------------------
	memzero_explicit(&keys, sizeof(keys));
	return 0;
badkey:
	crypto_aead_set_flags(cipher, CRYPTO_TFM_RES_BAD_KEY_LEN);
	memzero_explicit(&keys, sizeof(keys));
	return -EINVAL;
}
static int my_crypto_aead_aes_encrypt(struct aead_request *req)
{
	printk(KERN_INFO "Module mycrypto: mycrypto_aead_aes_encrypt \n");
	return 0;
}
static int my_crypto_aead_aes_decrypt(struct aead_request *req)
{
	printk(KERN_INFO "Module mycrypto: mycrypto_aead_aes_decrypt \n");
	return 0;
}
static int my_crypto_aead_cra_init(struct crypto_tfm *tfm)
{
	return 0;
}
static void my_crypto_aead_cra_exit(struct crypto_tfm *tfm)
{
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(tfm);
	memzero_explicit(ctx, tfm->__crt_alg->cra_ctxsize);
}
//----------------------------------------------------------------



/*	Test function for a typical handle request :
	- This function is registered in cra_init function.
	- Using for testing data flow.
*/

int mycrypto_aead_handle_request(struct crypto_async_request *base)
{
	//int ret;
	
// 	struct aead_request *req = aead_request_cast(base);
// 	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(req->base.tfm);
// 	// context for transformation object
// 	struct mycrypto_cipher_req *req_ctx = aead_request_ctx(req);
// 	//context for skcipher request
// 	struct mycrypto_dev *mydevice = ctx->mydevice;
// 	size_t len = (size_t)req->cryptlen;
// 	printk(KERN_INFO "Module mycrypto: handle request (copy to buffer)\n");
// 	// ret = mycrypto_handle_request(base,req_ctx,req->src,req->dst,req->cryptlen,0,0,req->iv);
// 	len = sg_pcopy_to_buffer(req->src, req_ctx->src_nents,
// 				 mydevice->buffer,
// 				 len, 0);
// 	// Turn on timer.
	return 0;
}

/*	Test function for a typical handle result :
	- This function is registered in cra_init function.
	- Using for testing data flow.
	- Using for testing with the lower layer of driver (pcie layer).
*/

int mycrypto_aead_handle_result(struct crypto_async_request *base, bool *should_complete)
{
	//int ret;
	
	// struct aead_request *req = aead_request_cast(base);
	// struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(req->base.tfm);
	//struct mycrypto_cipher_req *req_ctx = aead_request_ctx(req);
	//struct mycrypto_dev *mydevice = ctx->mydevice;
	//size_t len = (size_t)req->cryptlen;
	printk(KERN_INFO "Module mycrypto: handle result \n");
	// len = sg_pcopy_from_buffer(req->dst, req_ctx->dst_nents,
	// 			 mydevice->buffer,
	// 			 len, 0);
	*should_complete = true;
	return 0;
}

/*
   This function facilitates us to process request with queue 
   - Copy all parameters for encr/decr process into the context of operation (transformation objects) 
   and the context of request.
   - Enqueue the request into crypto queue.
   - Set schedule for workqueue task to dequeue this request in the future.
   
*/
static int mycrypto_queue_aead_req(struct crypto_async_request *base,
			u32 dir, u32 mode)
{
	
	struct mycrypto_cipher_op *ctx;
	// Get the context for transformation object
	struct aead_request *aead_req ;
	// Get skcipher request from its asysc request.
	struct crypto_aead *tfm ;
	struct mycrypto_dev *mydevice;
	int ret;

	// printk(KERN_INFO "Module mycrypto: enqueue request\n");
	ctx = crypto_tfm_ctx(base->tfm);
	mydevice = ctx->mydevice;

	aead_req = aead_request_cast(base);
	pr_err("Cipher.c:enqueue: sg_nents:%x -sg_length:%x - \
		page_link:%x- offset:%lx-dma_address:%llx\n",
		sg_nents(aead_req->src),aead_req->src->length,
		aead_req->src->page_link,aead_req->src->offset,
		aead_req->src->dma_address);
	// pr_aaa("aead_req pointer: %p - Adsress of req_ctx: %p - Value of req_ctx pointer: %p - Offset:%x\n", aead_req ,&(req_ctx), req_ctx, offsetof(struct aead_request, __ctx));
    
    tfm = crypto_aead_reqtfm(aead_req);
    
    // req_ctx->dir = 0xabcdef05;
    
    ctx->mode = mode;
    
    ctx->dir =dir;
    
    ctx->len = AES_BLOCK_SIZE;
    
    ctx->iv = aead_req->iv;

	ctx->assoclen = aead_req->assoclen;
			
	ctx->authsize = crypto_aead_authsize(tfm);
	//test for the disparity between encrypt and decrypt
	switch(ctx->dir){
		case MYCRYPTO_DIR_DECRYPT:
			ctx->cryptlen = aead_req->cryptlen -ctx->authsize;
			break;
		case MYCRYPTO_DIR_ENCRYPT:
			ctx->cryptlen = aead_req->cryptlen;
			break;
	}
	pr_err("Cipher.c: Address of req:%p - assoclen+Cryptlen =  %d %d \n",aead_req,  
            aead_req->assoclen, aead_req->cryptlen);
			        	
	// spin_lock(&mydevice->queue_lock);
	// /* enqueue request. */
	// ret = crypto_enqueue_request(&mydevice->queue, base);
	// pr_err("asys req:%p\n",base);
	// spin_unlock(&mydevice->queue_lock);
	
	// /* dequeue using workqueue */
	// queue_work(mydevice->workqueue,
	// 	   &mydevice->work_data.work);

	mydevice->req = base;// for no crypto-queue

	// dequeue without using workqueue (testing ...)
	// mycrypto_dequeue_req(mydevice);
	// return ret;
	return mycrypto_dequeue_req(mydevice);
}
/* Using for checking:
   -  whether or not the length of request and block size accommodate the critetia.
   -  The number of entries in src and dst list.
*/
static int mycrypto_aead_req_init(struct aead_request *aead_req)
{
	// struct mycrypto_cipher_req *req_ctx = (struct mycrypto_cipher_req *) aead_req->__ctx;
	// struct crypto_aead *tfm = crypto_aead_reqtfm(aead_req);
	// unsigned int blksize = crypto_aead_blocksize(tfm);
	int ret = 0;
	int src_nents;
	int dst_nents;

	// pr_aaa("aead_req pointer: %p - Adsress of aead_req->_ctx: %p - Value of req_ctx pointer: %p - Offset:%x\n", aead_req ,&(aead_req->__ctx), req_ctx, offsetof(struct aead_request, __ctx));
	// if (!IS_ALIGNED(aead_req->cryptlen, blksize))
	// 	return -EINVAL;
	src_nents = sg_nents(aead_req->src);
	if (src_nents != 1) {
		printk(KERN_INFO "Invalid number of src SG\n");
		return -ENOSPC;
	}
	dst_nents = sg_nents(aead_req->dst);
	if (dst_nents != 1) {
		printk(KERN_INFO "Invalid number of dst SG\n");
		return -ENOSPC;
	}
	return ret;

}


static int my_crypto_aead_gcm_encrypt(struct aead_request *aead_req)
{	
	int ret;

	// printk(KERN_INFO "Module mycrypto: mycrypto_aead_gcm_encrypt \n");
		/* checking the number of entries in scattergather list */
            	
	ret = mycrypto_aead_req_init(aead_req);
    if (ret)
    {
    	printk(KERN_INFO "ERROR SRC/DEST NUMBER OF ENTRY\n");
    	// return -ENOSPC;
    }
		
		//----------------------------------------------------
	// processing the queue of this request
	return mycrypto_queue_aead_req(&aead_req->base,
			MYCRYPTO_DIR_ENCRYPT, AES_MODE_GCM);
}
static int my_crypto_aead_gcm_decrypt(struct aead_request *aead_req)
{
	
	int ret;
	//struct skcipher_request *req = skcipher_request_cast(base);
	//struct mycrypto_cipher_req *req_ctx = skcipher_request_ctx(req);
	ret = mycrypto_aead_req_init(aead_req);
	// printk(KERN_INFO "Module mycrypto: mycrypto_aead_gcm_decrypt \n");
	if (ret)
		printk(KERN_INFO "ERROR SRC/DEST NUMBER OF ENTRY\n");
	return mycrypto_queue_aead_req(&aead_req->base,
			MYCRYPTO_DIR_DECRYPT, AES_MODE_GCM);
}

static int my_crypto_aead_gcm_cra_init(struct crypto_tfm *tfm)
{
	struct mycrypto_cipher_op *ctx =crypto_tfm_ctx(tfm);
	struct mycrypto_alg_template *tmpl =
		container_of(tfm->__crt_alg, struct mycrypto_alg_template,
			     alg.aead.base);
	// printk(KERN_INFO "Module mycrypto: my_crypto_aead_cra_init \n");
	// it means that tfm->__crt_alg
	crypto_skcipher_set_reqsize(__crypto_skcipher_cast(tfm),
				    sizeof(struct mycrypto_cipher_req));
	//ctx->hash_alg = AUTHENC_MODE_GHASH;
	//ctx->state_sz = GHASH_BLOCK_SIZE;
	ctx->mode = AES_MODE_GCM;
	ctx->mydevice = tmpl->mydevice;
	ctx->base.handle_request = mycrypto_aead_handle_request;
	ctx->base.handle_result = mycrypto_aead_handle_result;
	// ctx->hkaes = crypto_alloc_cipher("aes",0,0);
	
	return 0;
}
static void my_crypto_aead_gcm_cra_exit(struct crypto_tfm *tfm)
{
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(tfm);
	// pr_aaa("                     %s, %d, %p", __func__, __LINE__, ctx->hkaes);
	pr_aaa("                     %s, %d, %p", __func__, __LINE__, ctx);


	// crypto_free_cipher(ctx->hkaes); // free cipher for hash key (optional)
	memzero_explicit(ctx, tfm->__crt_alg->cra_ctxsize);
}
static int my_crypto_aead_gcm_setkey(struct crypto_aead *cipher, const u8 *key,unsigned int len)
{
	struct crypto_tfm *tfm = crypto_aead_tfm(cipher);
	// Get/retrieve transformation object
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(tfm);
	// Get transformation context 
	//struct mycrypto_dev *mydevice = ctx->mydevice;
	struct crypto_aes_ctx aes; 
	/* @aes: The location where the processing key (computed key) will be store*/
	// u32 hashkey[AES_BLOCK_SIZE >> 2];
	int ret,i;
	// printk(KERN_INFO "GCM_SETKEY");
	ret = crypto_aes_expand_key(&aes, key,len);
	// get key and let it adapt standard criteria of aes, then store it into struct aes.
	if (ret) {
		crypto_aead_set_flags(cipher,CRYPTO_TFM_RES_BAD_KEY_LEN);
		memzero_explicit(&aes, sizeof(aes));
		return ret;
	}
	// copy key stored in aes to ctx
	//ctx->keylen = (aes.key_length-16)/4;
	// for (i = 0; i < 20 ; i+=4)
    // {
    //     pr_err("file cipher.c:key = %3.3x , data =  %x %x %x %x \n", i ,
	// 		(key[i + 3]),(key[i + 2]), 
    //         (key[i + 1]),(key[i]));
    // }
	ctx->keylen=len;
	for(i = 0; i < len/sizeof(u32); i++)
		// ctx->key[i] = cpu_to_le32(aes.key_enc[i]);
		ctx->key[i] = cpu_to_be32(aes.key_enc[i]);//Test
	// for (i = 0; i < 8 ; i+=4)
    // {
    //     pr_err("file cipher.c:ctx->key = %3.3x , data =  %8.0x %8.0x %8.0x %8.0x \n", i ,
	// 		(ctx->key[i + 3]),(ctx->key[i + 2]), 
    //         (ctx->key[i + 1]),(ctx->key[i]));
    // }
	
	//Beside, it is not necessary to fill aes.key_dec.
	//If you wanna continue, just refer to setkey function for skcipher 
	//in file cipher.c (mv_cesa)
	/* Compute hash key by encrypting zeroes with cipher key */
	// crypto_cipher_clear_flags(ctx->hkaes, CRYPTO_TFM_REQ_MASK);
	// crypto_cipher_set_flags(ctx->hkaes, crypto_aead_get_flags(cipher) &
	// 			CRYPTO_TFM_REQ_MASK);
	// ret = crypto_cipher_setkey(ctx->hkaes, key, len);
	// if (ret)
	// 	return ret;

	// memset(hashkey, 0, AES_BLOCK_SIZE);
	// crypto_cipher_encrypt_one(ctx->hkaes, (u8 *)hashkey, (u8 *)hashkey);

	// for (i = 0; i < AES_BLOCK_SIZE / sizeof(u32); i++)
	// 	ctx->ipad[i] = cpu_to_be32(hashkey[i]);
	// pr_aaa("                     %s, %d, %p", __func__, __LINE__, &aes);
	// memzero_explicit(hashkey, AES_BLOCK_SIZE);
	memzero_explicit(&aes, sizeof(aes));
	//free memory
	return 0;
}

static int my_crypto_aead_rfc4106_gcm_setkey(struct crypto_aead *cipher, const u8 *key,unsigned int len)
{
	struct crypto_tfm *tfm = crypto_aead_tfm(cipher);
	// Get/retrieve transformation object
	struct mycrypto_cipher_op *ctx = crypto_tfm_ctx(tfm);
	// Get transformation context

	/*last 4 bytes of key are the nonce
	get this salt value and store in ctx->nonce
	*/ 
	ctx->nonce = *(u32 *)(key+len-CTR_RFC3686_NONCE_SIZE);// copy salt to ctx->nonce
	ctx->nonce = cpu_to_be32(ctx->nonce);//test
	len -= CTR_RFC3686_NONCE_SIZE;// 16 bytes
	return my_crypto_aead_gcm_setkey(cipher,key,len);
}
static int my_crypto_aead_gcm_setauthsize(struct crypto_aead *tfm, unsigned int authsize)
{
	//unsigned int authsize = tfm->authsize;
	switch(authsize)
	{
		case 4:
		case 8:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
			break;
		default:
			return -EINVAL;
		
	}
	return 0;
}
/*
 * validate assoclen for RFC4106/RFC4543
 */
static int crypto_ipsec_check_assoclen(unsigned int assoclen)
{
	switch (assoclen) {
	case 16: // ivlen +aadlen
	case 20: // ivlen +aadlen (esn sequence number)
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int my_crypto_aead_rfc4106_gcm_encrypt(struct aead_request *req)
{
		return crypto_ipsec_check_assoclen(req->assoclen) ?:
	       my_crypto_aead_gcm_encrypt(req);
}
static int my_crypto_aead_rfc4106_gcm_decrypt(struct aead_request *req)
{
		return crypto_ipsec_check_assoclen(req->assoclen) ?:
	       my_crypto_aead_gcm_decrypt(req);
}
//-----------------------------------------------------------------------
//struct AEAD algorithm which is registered after driver probing
struct mycrypto_alg_template mycrypto_alg_cbc_aes = {
    .type = MYCRYPTO_ALG_TYPE_SKCIPHER,
	.alg.skcipher = {
			.setkey = my_crypto_skcipher_aes_setkey,
    		.encrypt = my_crypto_cbc_aes_encrypt,
    		.decrypt = my_crypto_cbc_aes_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
    		.ivsize = AES_BLOCK_SIZE,
    		.base = {
        			.cra_name = "cbc(aes)",
					.cra_driver_name = "mycrypto_cbc_aes",
					.cra_priority = 600,
					.cra_flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_KERN_DRIVER_ONLY,
					.cra_blocksize = AES_BLOCK_SIZE,
					.cra_ctxsize = sizeof(struct mycrypto_cipher_op),
					.cra_alignmask = 0,
					.cra_init = my_crypto_skcipher_cra_init,
					.cra_exit = my_crypto_skcipher_cra_exit,
					.cra_module = THIS_MODULE,
    		},
	},
};
struct mycrypto_alg_template mycrypto_alg_gcm_aes = {
    .type = MYCRYPTO_ALG_TYPE_AEAD,
	.alg.aead = {
			.setkey = my_crypto_aead_gcm_setkey,
			.setauthsize = my_crypto_aead_gcm_setauthsize,
    		.encrypt = my_crypto_aead_gcm_encrypt,
    		.decrypt = my_crypto_aead_gcm_decrypt,
    		.ivsize = GCM_AES_IV_SIZE,
			.maxauthsize = GHASH_DIGEST_SIZE,
    		.base = {
        			.cra_name = "gcm(aes)",
					.cra_driver_name = "mycrypto_gcm_aes",
					.cra_priority = 500,
					.cra_flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_KERN_DRIVER_ONLY,
					.cra_blocksize = 1,
					.cra_ctxsize = sizeof(struct mycrypto_cipher_op),
					.cra_alignmask = 0,
					.cra_init = my_crypto_aead_gcm_cra_init,
					.cra_exit = my_crypto_aead_gcm_cra_exit,
					.cra_module = THIS_MODULE,
    		},
	},
};

struct mycrypto_alg_template mycrypto_alg_rfc4106_gcm_aes = {
    .type = MYCRYPTO_ALG_TYPE_AEAD,
	.alg.aead = {
			.setkey = my_crypto_aead_rfc4106_gcm_setkey,
			.setauthsize = my_crypto_aead_gcm_setauthsize,
    		.encrypt = my_crypto_aead_rfc4106_gcm_encrypt,
    		.decrypt = my_crypto_aead_rfc4106_gcm_decrypt,
    		.ivsize = GCM_RFC4106_IV_SIZE,
			.maxauthsize = GHASH_DIGEST_SIZE,
    		.base = {
        			.cra_name = "rfc4106(gcm(aes))",
					.cra_driver_name = "mycrypto_gcm_aes",
					.cra_priority = 500,
					.cra_flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_KERN_DRIVER_ONLY,
					.cra_blocksize = 1,
					.cra_ctxsize = sizeof(struct mycrypto_cipher_op),
					.cra_alignmask = 0,
					.cra_init = my_crypto_aead_gcm_cra_init,
					.cra_exit = my_crypto_aead_gcm_cra_exit,
					.cra_module = THIS_MODULE,
    		},
	},
};
struct mycrypto_alg_template mycrypto_alg_authenc_hmac_sha256_ctr_aes = {
    .type = MYCRYPTO_ALG_TYPE_AEAD,
	.alg.aead = {
			.setkey = my_crypto_aead_aes_setkey,
    		.encrypt = my_crypto_aead_aes_encrypt,
    		.decrypt = my_crypto_aead_aes_decrypt,
    		.ivsize = 12,
			.maxauthsize = SHA256_DIGEST_SIZE,
    		.base = {
        			.cra_name = "authenc(hmac(sha256),ctr(aes))",
					.cra_driver_name = "mycrypto_alg_authenc_hmac_sha256_ctr_aes",
					.cra_priority = 250,
					.cra_flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_KERN_DRIVER_ONLY,
					.cra_blocksize = 1,
					.cra_ctxsize = sizeof(struct mycrypto_cipher_op),
					.cra_alignmask = 0,
					.cra_init = my_crypto_aead_cra_init,
					.cra_exit = my_crypto_aead_cra_exit,
					.cra_module = THIS_MODULE,
    		},
	},
};

struct mycrypto_alg_template mycrypto_alg_authenc_hmac_sha256_cbc_aes = {
    .type = MYCRYPTO_ALG_TYPE_AEAD,
	.alg.aead = {
			.setkey = my_crypto_aead_aes_setkey,
    		.encrypt = my_crypto_aead_aes_encrypt,
    		.decrypt = my_crypto_aead_aes_decrypt,
    		.ivsize = AES_BLOCK_SIZE,
			.maxauthsize = SHA256_DIGEST_SIZE,
    		.base = {
        			.cra_name = "authenc(hmac(sha256),cbc(aes))",
					.cra_driver_name = "mycrypto_alg_authenc_hmac_sha256_cbc_aes",
					.cra_priority = 300,
					.cra_flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_KERN_DRIVER_ONLY,
					.cra_blocksize = AES_BLOCK_SIZE,
					.cra_ctxsize = sizeof(struct mycrypto_cipher_op),
					.cra_alignmask = 0,
					.cra_init = my_crypto_aead_cra_init,
					.cra_exit = my_crypto_aead_cra_exit,
					.cra_module = THIS_MODULE,
    		},
	},
};
