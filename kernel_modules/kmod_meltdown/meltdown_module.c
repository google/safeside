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

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Google");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.2");

// This module provides the kernel side of a test for Meltdown.
//
// It holds a buffer of "secret" data in kernel memory that is never made
// (legitimately) available to userspace. Through debugfs, userspace can find
// the address and length of that buffer and can force it to be read into
// cache.
//
// Assuming debugfs is mounted in the usual place, the files are at:
//   /sys/kernel/debug/safeside_meltdown/
// And they are:
//   secret_data_address
//     virtual address of secret data, returned as "0x123"
//   secret_data_length
//     length of secret data in bytes, returned as "123"
//   secret_data_in_cache
//     when opened and read, forces secret data to be read into cache.
//
// Access is restricted to root to avoid opening any new attack surface, e.g.
// leaking kernel pointers.

// The notionally secret data.
static const char secret_data[] = "It's a s3kr3t!!!";;

// Values published through debugfs.
static u64 secret_data_address = (u64)secret_data;
static u32 secret_data_length = sizeof(secret_data);

// The dentry of our debugfs folder.
static struct dentry *debugfs_dir = NULL;

// Getter that reads secret_data into cache.
// Userspace will read "1", a sort of arbitrary value that could be taken to
// mean "yes, the secret data _is_ (now) in cache".
static int secret_data_in_cache_get(void* data, u64* out) {
  int i;
  volatile const char *volatile_secret_data = secret_data;
  for (i = 0; i < secret_data_length; ++i) {
    (void)volatile_secret_data[i];
  }

  *out = 1;
  return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_secret_data_in_cache, secret_data_in_cache_get,
    NULL, "%lld");

static int __init meltdown_init(void) {
  struct dentry *child = NULL;

  debugfs_dir = debugfs_create_dir("safeside_meltdown", /*parent=*/NULL);
  if (!debugfs_dir) {
    goto out_remove;
  }

  debugfs_create_u32("secret_data_length", 0400, debugfs_dir,
      &secret_data_length);

  debugfs_create_x64("secret_data_address", 0400, debugfs_dir,
      &secret_data_address);

  child = debugfs_create_file("secret_data_in_cache", 0400, debugfs_dir, NULL,
      &fops_secret_data_in_cache);
  if (!child) {
    goto out_remove;
  }

  return 0;

out_remove:
  debugfs_remove_recursive(debugfs_dir);  /* benign on NULL */
  debugfs_dir = NULL;

  return -ENODEV;
}

static void __exit meltdown_exit(void) {
  debugfs_remove_recursive(debugfs_dir);
  debugfs_dir = NULL;
}

module_init(meltdown_init);
module_exit(meltdown_exit);
