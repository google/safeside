/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 Google LLC
 *
 **/

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Google");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.1");

#ifndef __aarch64__
#  error Unsupported CPU. ARM64 required.
#endif

// Provides an endpoint on
//   /sys/kernel/eret_hvc_smc/address
// to which userspace programs can send their addresses that will be
// speculatively fetched after speculating over ERET, HVC and SMC instructions.
// Currently should be accessible only by root, because there is no checking of
// those addresses.

static ssize_t address_store(struct kobject *kobj, struct kobj_attribute *attr,
                             const char *buf, size_t length) {
  size_t address;
  int *memory, res = kstrtoul(buf, 0, &address);
  char *pointer;

  if (res != 0) {
    // Incorrectly formatted address was provided.
    pr_err("kstrtol failed with %d\n", res);
    return length;
  }

  pointer = (char *) address;

  __uaccess_enable(ARM64_ALT_PAN_NOT_UAO);
  memory = kmalloc(sizeof(int), GFP_KERNEL);
  memory[0] = (int) length;

  // Core functionality.
  asm volatile(
      // 1000 repetitions to confuse the PHT enough.
      ".rept 1000\n"
      // Flush "memory[0]" from cache and sync.
      "dc civac, %0\n"
      "dsb sy\n"
      // Slowly load "memory[0]" from main memory and speculate forward in the
      // meantime.
      "ldr w1, [%0]\n"
      "cmn w1, #0x1\n"
      "bne 1f\n"
      // Dead code begins - the four following instructions are speculated
      // over.
      "eret\n"
      "hvc #0\n"
      "smc #0\n"
      // Load the "pointer" into memory speculatively after ERET, HVC and SMC.
      "ldrb w1, [%1]\n"
      // Dead code ends.
      "1:\n"
      ".endr\n"::"r"(memory), "r"(pointer));

  kfree(memory);
  __uaccess_disable(ARM64_ALT_PAN_NOT_UAO);
  return length;
}

static struct kobj_attribute address_file_attribute =
    __ATTR_WO(address); // writeable only by root.

static struct kobject *sysfs_entry = NULL;

static int __init eret_hvc_smc_init(void) {
  int error;

  pr_info("eret_hvc_smc init\n");

  sysfs_entry = kobject_create_and_add("eret_hvc_smc", kernel_kobj);
  if (!sysfs_entry) {
    return -ENOMEM;
  }

  error = sysfs_create_file(sysfs_entry, &address_file_attribute.attr);

  if (error != 0) {
    kobject_put(sysfs_entry);
    sysfs_entry = NULL;

    return error;
  }

  return 0;
}

static void __exit eret_hvc_smc_exit(void) {
  pr_info("eret_hvc_smc exit\n");

  kobject_put(sysfs_entry);
  sysfs_entry = NULL;
}

module_init(eret_hvc_smc_init);
module_exit(eret_hvc_smc_exit);
