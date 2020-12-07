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

void swap_init (void);
void swap_in (struct page *);
bool swap_out (struct page *);

#endif