/*
 * Cryptographic Hash operations.
 * 
 * Copyright (c) 2006 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/seq_file.h>

#include "internal.h"

static unsigned int crypto_hash_ctxsize(struct crypto_alg *alg, u32 type,
					u32 mask)
{
	return alg->cra_ctxsize;
}

static int hash_setkey_unaligned(struct crypto_hash *crt, const u8 *key,
		                 unsigned int keylen)
{
	struct crypto_tfm *tfm = crypto_hash_tfm(crt);
	struct hash_alg *alg = &tfm->__crt_alg->cra_hash;
	unsigned long alignmask = crypto_hash_alignmask(crt);
	int ret;
	u8 *buffer, *alignbuffer;
	unsigned long absize;

	absize = keylen + alignmask;
	buffer = kmalloc(absize, GFP_ATOMIC);
	if (!buffer)
		return -ENOMEM;

	alignbuffer = (u8 *)ALIGN((unsigned long)buffer, alignmask + 1);
	memcpy(alignbuffer, key, keylen);
	ret = alg->setkey(crt, alignbuffer, keylen);
	memset(alignbuffer, 0, keylen);
	kfree(buffer);
	return ret;
}

static int hash_setkey(struct crypto_hash *crt, const u8 *key,
		       unsigned int keylen)
{
	struct crypto_tfm *tfm = crypto_hash_tfm(crt);
	struct hash_alg *alg = &tfm->__crt_alg->cra_hash;
	unsigned long alignmask = crypto_hash_alignmask(crt);

	if ((unsigned long)key & alignmask)
		return hash_setkey_unaligned(crt, key, keylen);

	return alg->setkey(crt, key, keylen);
}

static int crypto_init_hash_ops(struct crypto_tfm *tfm, u32 type, u32 mask)
{
	struct hash_tfm *crt = &tfm->crt_hash;
	struct hash_alg *alg = &tfm->__crt_alg->cra_hash;

	if (alg->digestsize > crypto_tfm_alg_blocksize(tfm))
		return -EINVAL;

	crt->init = alg->init;
	crt->update = alg->update;
	crt->final = alg->final;
	crt->digest = alg->digest;
	crt->setkey = hash_setkey;
	crt->digestsize = alg->digestsize;

	return 0;
}

static void crypto_hash_show(struct seq_file *m, struct crypto_alg *alg)
	__attribute__ ((unused));
static void crypto_hash_show(struct seq_file *m, struct crypto_alg *alg)
{
	seq_printf(m, "type         : hash\n");
	seq_printf(m, "blocksize    : %u\n", alg->cra_blocksize);
	seq_printf(m, "digestsize   : %u\n", alg->cra_hash.digestsize);
}

const struct crypto_type crypto_hash_type = {
	.ctxsize = crypto_hash_ctxsize,
	.init = crypto_init_hash_ops,
#ifdef CONFIG_PROC_FS
	.show = crypto_hash_show,
#endif
};
EXPORT_SYMBOL_GPL(crypto_hash_type);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Generic cryptographic hash type");
