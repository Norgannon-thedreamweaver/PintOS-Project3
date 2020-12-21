#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <stdbool.h>

#define NO_SECTOR 4294967295U

struct lock evict_lock;

void swap_init (void);
void swap_in (struct page *);
bool swap_out (struct page *);

void reset_swap_bitmap(block_sector_t sector);
#endif