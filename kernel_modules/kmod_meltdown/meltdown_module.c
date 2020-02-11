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
#include <linux/seq_file.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Google");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.1");

// Keeps a buffer in kernel memory and makes its address available at:
//   /proc/safeside_meltdown/address
// and its length available at:
//   /proc/safeside_meltdown/length
// which are both only accessible to root.

// Secret data stored in the kernel memory whose content is never directly
// leaked through sysfs.
const char *private_data = "It's a s3kr3t!!!";

// Directory record. Must be available on unloading the module.
struct proc_dir_entry *safeside_meltdown;

// Print the address of `private_data` into the provided buffer.
static int address_show(struct seq_file *file, void *v) {
  seq_printf(file, "%px\n", private_data);
  return 0;
}

// Print the length of `private_data` into the sequential file.
// At the same time loads `private_data` to cache by accessing it.
static int length_show(struct seq_file *file, void *v) {
  seq_printf(file, "%d\n", (int) strlen(private_data));
  return 0;
}

static int address_open(struct inode *i, struct file *file) {
  return single_open(file, address_show, NULL);
}

static int length_open(struct inode *i, struct file *file) {
  return single_open(file, length_show, NULL);
}

static struct file_operations address_file_ops = {
  .open = address_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};

static struct file_operations length_file_ops = {
  .open = length_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};

static int __init meltdown_init(void) {
  struct proc_dir_entry *address, *length;

  pr_info("safeside_meltdown init\n");

  safeside_meltdown = proc_mkdir("safeside_meltdown", NULL);
  if (safeside_meltdown == NULL) {
    return -ENOMEM;
  }

  // Read-only files, accessible only by root.
  address = proc_create("address", 0400, safeside_meltdown, &address_file_ops);
  if (address == NULL) {
    remove_proc_entry("safeside_meltdown", NULL);
    return -ENOMEM;
  }

  length = proc_create("length", 0400, safeside_meltdown, &length_file_ops);
  if (length == NULL) {
    remove_proc_entry("address", safeside_meltdown);
    remove_proc_entry("safeside_meltdown", NULL);
    return -ENOMEM;
  }

  return 0;
}

static void __exit meltdown_exit(void) {
  pr_info("safeside_meltdown exit\n");

  remove_proc_entry("length", safeside_meltdown);
  remove_proc_entry("address", safeside_meltdown);
  remove_proc_entry("safeside_meltdown", NULL);
}

module_init(meltdown_init);
module_exit(meltdown_exit);
