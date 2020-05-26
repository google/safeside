/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Google");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.2");

// Keeps a buffer in kernel memory and makes its address available at:
//   /proc/safeside_meltdown/address
// and its length available at:
//   /proc/safeside_meltdown/length
// which are both only accessible to root.

// Secret data stored in the kernel memory whose content is never directly
// leaked through sysfs.
static const char secret_data[] = "It's a s3kr3t!!!";
static const int secret_data_length = sizeof(secret_data);

static struct dentry *root_dir = NULL;

static int __init meltdown_init(void) {
  struct dentry *child = NULL;

  root_dir = debugfs_create_dir("safeside_meltdown", /*parent=*/NULL);
  if (!root_dir) {
    goto err;
  }

  child = debugfs_create_u32("secret_data_length",
      0400, root_dir, (u32*)&secret_data_length);
  if (!child) {
    goto err;
  }

  child = debugfs_create_x64("secret_data_address",
      0400, root_dir, (u64*)&secret_data);
  if (!child) {
    goto err;
  }

  return 0;

err:
  debugfs_remove_recursive(root_dir);  /* benign on NULL */
  root_dir = NULL;

  return -ENODEV;
}

static void __exit meltdown_exit(void) {
  debugfs_remove_recursive(root_dir);
  root_dir = NULL;
}

module_init(meltdown_init);
module_exit(meltdown_exit);
