/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Google");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.1");

#ifndef __aarch64__
#  error Unsupported CPU. ARM64 required.
#endif

// Provides an endpoint on
//   /proc/safeside_eret_hvc_smc/address
// to which userspace programs can send their addresses that will be
// fetched due to speculation over ERET, HVC and SMC instructions.
// Currently should be accessible only by root, because there is no checking of
// those addresses.

// Directory record. Must be available on unloading the module.
struct proc_dir_entry *safeside_eret_hvc_smc;

static ssize_t address_store(struct file *f, const char __user *buf,
                             size_t length, loff_t *off) {
  ptrdiff_t userspace_address;
  int res;
  int *kernel_memory;
  // Enable kernel access to userspace memory.
  __uaccess_enable(ARM64_ALT_PAN_NOT_UAO);
  res = kstrtoul(buf, 0, &userspace_address);

  if (res != 0) {
    // Incorrectly formatted address was provided.
    pr_err(
        "kstrtol failed with input %s and return value %d,"
        "correct format is 0xHHHHHHHHHHHHHHHH\n", buf, res);
    return length;
  }

  kernel_memory = kmalloc(sizeof(int), GFP_KERNEL);
  kernel_memory[0] = 0;

  // Core functionality.
  asm volatile(
      // 1000 repetitions to confuse the Pattern History Table sufficiently and
      // achieve a Spectre v1 misspeculation.
      ".rept 1000\n"
      // Flush kernel_memory from cache and synchronize.
      "dc civac, %0\n"
      "dsb sy\n"
      // Slowly load kernel_memory from main memory and speculate forward in the
      // meantime.
      "ldr w1, [%0]\n"
      // Impossible condition - always false because the kernel_memory[0] is 0
      // and not 1.
      "cmp w1, #0x1\n"
      // De-facto unconditional forward jump to the 1: label.
      "bne 1f\n"
      // Dead code begins - the four following instructions are executed only
      // speculatively.
      "eret\n"
      "hvc #0\n"
      "smc #0\n"
      // Load the userspace_address speculatively after speculating over ERET,
      // HVC and SMC.
      "ldrb w1, [%1]\n"
      // Dead code ends.
      "1:\n"
      ".endr\n"::"r"(kernel_memory), "r"(userspace_address));

  kfree(kernel_memory);
  // Disable kernel access to userspace memory.
  __uaccess_disable(ARM64_ALT_PAN_NOT_UAO);
  return length;
}

static struct file_operations address_file_ops = {
  write: address_store
};

static int __init eret_hvc_smc_init(void) {
  struct proc_dir_entry *address;

  pr_info("safeside_eret_hvc_smc init\n");

  safeside_eret_hvc_smc = proc_mkdir("safeside_eret_hvc_smc", NULL);
  if (safeside_eret_hvc_smc == NULL) {
    return -ENOMEM;
  }

  // Write-only file, accessible only by root.
  address = proc_create("address", 0200, safeside_eret_hvc_smc,
                        &address_file_ops);
  if (address == NULL) {
    remove_proc_entry("safeside_eret_hvc_smc", NULL);
    return -ENOMEM;
  }

  return 0;
}

static void __exit eret_hvc_smc_exit(void) {
  pr_info("safeside_eret_hvc_smc exit\n");

  remove_proc_entry("address", safeside_eret_hvc_smc);
  remove_proc_entry("safeside_eret_hvc_smc", NULL);
}

module_init(eret_hvc_smc_init);
module_exit(eret_hvc_smc_exit);
