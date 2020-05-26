/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include <asm/tlbflush.h>

#include <linux/debugfs.h>
#include <linux/hugetlb.h>
#include <linux/mm.h>
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

static int seal_page_set(void *data, u64 addr)
{
  struct vm_area_struct *vma = NULL;
  pgd_t *pgd = NULL;
  p4d_t *p4d = NULL;
  pud_t *pud = NULL;
  pmd_t *pmd = NULL;
  pte_t *ptep = NULL;
  spinlock_t *pte_lock = NULL;

  int ret = -EINVAL;

	pr_info("process %d to seal virtual address: 0x%llx\n", current->pid, addr);

  if (down_write_killable(&current->mm->mmap_sem)) {
    return -EINTR;
  }

  vma = find_vma(current->mm, addr);
  if (!vma) {
    pr_info("no vma for address\n");
    goto out_unlock_mmap_sem;
  } else if (is_vm_hugetlb_page(vma)) {
    pr_info("addr is on hugepage\n");
    goto out_unlock_mmap_sem;
  } else if ((vma->vm_flags & VM_READ) == 0 ||
             (vma->vm_flags & VM_WRITE) == 0) {
    pr_info("address is not read/write\n");
    goto out_unlock_mmap_sem;
  }

  pgd = pgd_offset(current->mm, addr);
  if (pgd_none(*pgd) || pgd_bad(*pgd)) {
    pr_info("pgd none/bad\n");
    goto out_unlock_mmap_sem;
  }

  p4d = p4d_offset(pgd, addr);
  if (p4d_none(*p4d) || p4d_bad(*p4d)) {
    pr_info("p4d none/bad\n");
    goto out_unlock_mmap_sem;
  }

  pud = pud_offset(p4d, addr);
  if (pud_none(*pud) || pud_bad(*pud) || !pud_present(*pud)) {
    pr_info("pud none/bad/!present\n");
    goto out_unlock_mmap_sem;
  }

  pmd = pmd_offset(pud, addr);
  if (pmd_none(*pmd) || pmd_bad(*pmd) || !pmd_present(*pmd)) {
    pr_info("pmd none/bad/!present\n");
    goto out_unlock_mmap_sem;
  }

  ptep = pte_offset_map_lock(current->mm, pmd, addr, &pte_lock);

  pr_info("old pte = 0x%lx\n", pte_flags(*ptep));

  // remove PAGE_RW as well, otherwise writes trigger an infinite loop
  set_pte(ptep, __pte(pte_val(*ptep) & ~_PAGE_USER));

  pr_info("new pte = 0x%lx\n", pte_flags(*ptep));

  // flush_tlb_mm_range isn't exported for modules
  __flush_tlb_one_user(addr);

  ret = 0;
  pte_unmap_unlock(ptep, pte_lock);

out_unlock_mmap_sem:
  up_write(&current->mm->mmap_sem);
  return ret;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_seal_page, NULL, seal_page_set, "0x%016llx\n");

static struct dentry *root_dir = NULL;

static int __init meltdown_init(void) {
  struct dentry *child = NULL;

  root_dir = debugfs_create_dir("safeside_meltdown", /*parent=*/NULL);
  if (!root_dir) {
    goto err;
  }

  child = debugfs_create_u32("secret_data_length", 0400, root_dir,
      (u32*)&secret_data_length);
  if (!child) {
    goto err;
  }

  child = debugfs_create_x64("secret_data_address", 0400, root_dir,
      (u64*)&secret_data);
  if (!child) {
    goto err;
  }

  child = debugfs_create_file("address_to_seal", 0200, root_dir, NULL,
      &fops_seal_page);
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
