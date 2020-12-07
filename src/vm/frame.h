#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/synch.h"
#include <list.h>
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"


struct frame {
    void* base;            /* kernel virtual base address */
    struct thread* thread; /* thread which owns this frame */
    struct page* page;     /* page corresponding to this frame */
    struct list_elem elem;
};

void frame_init () ;

struct frame* frame_alloc();
void frame_free(struct frame * f);

struct frame* frame_find_victim();

#endif