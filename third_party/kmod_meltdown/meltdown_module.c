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
#include <linux/sysfs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Google");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.1");

// Keeps a buffer in kernel memory and makes its address available at
//   /sys/kernel/safeside_meltdown/address
// which is only accessible to root.

// Secret data stored in the kernel memory whose content is never directly
// leaked through sysfs.
const char *private_data = "It's a s3kr3t!!!";

// Puts address of `private_data` as a string into `buf`, which is PAGE_SIZE
// bytes.
static ssize_t address_show(struct kobject *kobj, struct kobj_attribute *attr,
                            char *buf) {
  return snprintf(buf, PAGE_SIZE, "%px\n", private_data);
}

// Puts length of `private_data` as a string into `buf`, which is PAGE_SIZE
// bytes. At the same time loads `private_data` to cache by accessing it.
static ssize_t length_show(struct kobject *kobj, struct kobj_attribute *attr,
                           char *buf) {
  return snprintf(buf, PAGE_SIZE, "%d\n", (int) strlen(private_data));
}

static struct kobj_attribute address_file_attribute =
    __ATTR_RO_MODE(address, 0400);  // owner-read only; owned by root

static struct kobj_attribute length_file_attribute =
    __ATTR_RO_MODE(length, 0400);  // owner-read only; owned by root

static struct kobject *sysfs_entry = NULL;

static int __init meltdown_init(void) {
  int error;

  pr_info("safeside_meltdown init\n");

  sysfs_entry = kobject_create_and_add("safeside_meltdown", kernel_kobj);
  if (!sysfs_entry) {
    return -ENOMEM;
  }

  error = sysfs_create_file(sysfs_entry, &address_file_attribute.attr);
  if (error == 0) {
    error = sysfs_create_file(sysfs_entry, &length_file_attribute.attr);
  }

  if (error != 0) {
    kobject_put(sysfs_entry);
    sysfs_entry = NULL;

    return error;
  }

  return 0;
}

static void __exit meltdown_exit(void) {
  pr_info("safeside_meltdown exit\n");

  kobject_put(sysfs_entry);
  sysfs_entry = NULL;
}

module_init(meltdown_init);
module_exit(meltdown_exit);
