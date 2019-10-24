/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("Proprietary");
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
