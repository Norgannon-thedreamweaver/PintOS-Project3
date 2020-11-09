#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  if(!is_valid_buffer(f->esp,4)){
    exit(-1);
    return;
  }
  int syscall_number = * (int *)f->esp;

  switch(syscall_number){
    case SYS_HALT:
      syscall_halt(f);  
      break;
    case SYS_EXIT:
      syscall_exit(f);  
      break;
    case SYS_EXEC:
      syscall_exec(f);     
      break;
    case SYS_WAIT:
      syscall_wait(f);   
      break;
    case SYS_CREATE:
      syscall_create(f);   
      break;
    case SYS_REMOVE:
      syscall_remove(f); 
      break;
    case SYS_OPEN:
      syscall_open(f); 
      break;
    case SYS_FILESIZE:
      syscall_filesize(f); 
      break;
    case SYS_READ:
      syscall_read(f); 
      break;
    case SYS_WRITE:
      syscall_write(f);  
      break;
    case SYS_SEEK:
      syscall_seek(f);  
      break;
    case SYS_TELL:
      syscall_tell(f);   
      break;
    case SYS_CLOSE:
      syscall_close(f); 
      break;

    default:
      exit(-1);
  }
}

void exit (int status){
  struct thread *current_thread=thread_current ();
  struct list_elem *tmp;
  struct thread_exit_status *tes;
  
  if(lock_held_by_current_thread(&file_lock))
    lock_release(&file_lock);
  
  for (tmp = list_begin (&current_thread->file_list); tmp != list_end (&current_thread->file_list); tmp = list_begin (&current_thread->file_list))
    close (list_entry (tmp, struct process_file,elem)->fd);
  if(current_thread->executable_file!=NULL){
    file_close(current_thread->executable_file);
    current_thread->executable_file=NULL;
  }
    
  
  
  current_thread->exit_status = status;
  
  if(current_thread->parent!=NULL){
    struct thread_exit_status* cur_exit_status=(struct thread_exit_status*)malloc(sizeof(struct thread_exit_status));
    cur_exit_status->tid=current_thread->tid;
    cur_exit_status->exit_status=status;
    list_push_back(&current_thread->parent->dead_children_list,&cur_exit_status->child_elem);
  }
  list_remove(&current_thread->alive_child_elem);

  for (tmp = list_begin (&current_thread->alive_children_list); tmp != list_end (&current_thread->alive_children_list); tmp = list_next (tmp)){
    list_entry (tmp, struct thread,alive_child_elem)->parent=NULL;
  }
  for (tmp = list_begin (&current_thread->dead_children_list); tmp != list_end (&current_thread->dead_children_list); tmp = list_begin (&current_thread->dead_children_list)){
    tes=list_entry (tmp, struct thread_exit_status,child_elem);
    list_remove(&tes->child_elem);
    free(tes);
  }
  
  thread_exit ();
  
}

pid_t exec (const char *file){
  return process_execute(file);
}

int wait (pid_t pid){
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){
  return filesys_create(file,initial_size);
}

bool remove (const char *file){
  return filesys_remove(file);
}

int open (const char *file){
  struct file* f = filesys_open(file);
  if(f == NULL)
    return -1;
  struct process_file *pf = (struct process_file *)malloc(sizeof(struct process_file));
  if(pf == NULL){
    file_close(f);
    return -1;
  }

  struct thread *current_thread = thread_current();
  pf->fd = current_thread->max_fd;
  current_thread->max_fd++;
  current_thread->file_open++;
  pf->file = f;
  list_push_back(&current_thread->file_list,&pf->elem);
  return pf->fd;
}

int filesize (int fd){
  struct process_file *f = get_process_file_by_fd(fd);
  if(f == NULL){
    exit(-1);
  }
  return file_length(f->file);

}

int read (int fd, void *buffer, unsigned length){
  if(fd==STDIN){
    unsigned int i;
    for(i=0;i<length;i++){
      *((char **)buffer)[i] = input_getc();
    }
    return length;
  }
  else if(fd==STDOUT){
    exit(-1);
  }
  else{
    struct process_file *f = get_process_file_by_fd(fd);
    if(f == NULL){
      exit(-1);
    }
    return (int) file_read(f->file,buffer,length);
  }
}

int write (int fd, const void *buffer, unsigned length){
  if(fd==STDOUT){
    putbuf(buffer,length);
    return length;
  }
  else if(fd==STDIN){
    exit(-1);
  }
  else{
    struct process_file *f = get_process_file_by_fd(fd);
    if(f==NULL){
      exit(-1);
    }
    return (int) file_write(f->file,buffer,length);
  }
}

void seek (int fd, unsigned position){
  struct process_file *f = get_process_file_by_fd(fd);
  if(f == NULL){
    exit(-1);
  }
  file_seek(f->file,position);
}

unsigned tell (int fd){
  struct process_file *f = get_process_file_by_fd(fd);
  if(f == NULL){
    exit(-1);
  }
  return (unsigned) file_tell(f->file);
}

void close (int fd){
  
  struct process_file *pf = get_process_file_by_fd(fd);

  if(pf == NULL||pf->file==NULL){
    exit(-1);
  }
  
  file_close (pf->file);
  list_remove (&pf->elem);
  thread_current()->file_open--;
  free (pf);
}

//↓ real system call function

void syscall_halt (struct intr_frame* f){
  shutdown_power_off();
}

void syscall_exit (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,4)){
    exit(-1);
  }
  int status = *(int *)(f->esp +4);
  exit(status);
}

void syscall_exec (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,4)){
    exit(-1);
  }
  char *cmd_line = *(char **)(f->esp+4);
  if(cmd_line==NULL || !is_valid_addr(cmd_line) || !is_valid_string(cmd_line)){
    exit(-1);
  }
  char *new_cmd = (char*)malloc(strlen(cmd_line) + 1);
  strlcpy(new_cmd, cmd_line, strlen(cmd_line) + 1);

  f->eax = exec(new_cmd);
  free(new_cmd);
}

void syscall_wait (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,4)){
    exit(-1);
  }
  pid_t pid = *(int *)(f->esp+4);
  f->eax = wait(pid);
}

void syscall_create (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,8)){
    exit(-1);
  }
  char* file_name = *(char **)(f->esp+4);
  if(file_name==NULL || !is_valid_addr(file_name) || !is_valid_string(file_name)){
    exit(-1);
  }
  unsigned size = *(int *)(f->esp+8);
  lock_acquire(&file_lock);
  f->eax = create(file_name,size);
  lock_release(&file_lock);
}

void syscall_remove (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,4)){
    exit(-1);
  }
  char* file_name = *(char **)(f->esp+4);
  if(file_name==NULL || !is_valid_addr(file_name) || !is_valid_string(file_name)){
    exit(-1);
  }
  lock_acquire(&file_lock);
  f->eax = remove(file_name);
  lock_release(&file_lock);
}

void syscall_open (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,4)){
    exit(-1);
  }
  char* file_name = *(char **)(f->esp+4);
  if(file_name==NULL || !is_valid_addr(file_name) || !is_valid_string(file_name)){
    exit(-1);
  }
  lock_acquire(&file_lock);
  f->eax = open(file_name);
  lock_release(&file_lock);
}

void syscall_filesize (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,4)){
    exit(-1);
  }
  int fd = *(int *)(f->esp + 4);
  lock_acquire(&file_lock);
  f->eax = filesize(fd);
  lock_release(&file_lock);
}

void syscall_read (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,12)){
    exit(-1);
  }
  int fd = *(int *)(f->esp +4);
  void *buffer = *(char**)(f->esp + 8);
  unsigned size = *(unsigned *)(f->esp + 12);

  if(!is_valid_buffer(buffer,size)){
    exit(-1);
  }
  lock_acquire(&file_lock);
  f->eax = read(fd,buffer,size);
  lock_release(&file_lock);
}

void syscall_write (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,12)){
    exit(-1);
  }
  int fd = *(int *)(f->esp +4);
  void *buffer = *(char**)(f->esp + 8);
  unsigned size = *(unsigned *)(f->esp + 12);

  if(!is_valid_buffer(buffer,size)){
    exit(-1);
  }
  lock_acquire(&file_lock);
  f->eax=write(fd,buffer,size);
  lock_release(&file_lock);
}

void syscall_seek (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,8)){
    exit(-1);
  }
  int fd = *(int *)(f->esp + 4);
  unsigned pos = *(unsigned *)(f->esp + 8);
  lock_acquire(&file_lock);
  seek(fd,pos);
  lock_release(&file_lock);
}

void syscall_tell (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,4)){
    exit(-1);
  }
  int fd = *(int *)(f->esp +4);
  lock_acquire(&file_lock);
  f->eax =tell(fd);
  lock_release(&file_lock);
}

void syscall_close (struct intr_frame* f){
  if(!is_valid_buffer(f->esp+4,4)){
    exit(-1);
  }
  int fd = *(int *)(f->esp +4);
  lock_acquire(&file_lock);
  close(fd);
  lock_release(&file_lock);
  
}



struct process_file*
get_process_file_by_fd(int fd){
  struct thread *current_thread=thread_current ();
  struct list_elem *tmp;
  for (tmp = list_begin (&current_thread->file_list); tmp != list_end (&current_thread->file_list); tmp = list_next (tmp)){
    if(list_entry (tmp, struct process_file, elem)->fd== fd)
      return list_entry (tmp, struct process_file, elem);
  }
  return NULL;
}

bool
is_valid_addr(const void *vaddr){
	if (!is_user_vaddr(vaddr) || !(pagedir_get_page(thread_current()->pagedir, vaddr))){
		return false;
	}
	return true;
}

bool
is_valid_buffer (void *vaddr, unsigned size)
{
  unsigned i;
  char* tmp=vaddr;
  for (i = 0; i < size; i++){
    if(!is_valid_addr(tmp)){
      return false;
    }
    tmp++;
  }
  return true;
}

bool 
is_valid_string(void *str){
  int ch=-1;
  char* tmp=str;
  while(1){
    if(!is_valid_addr(tmp))
      return false;
    ch=get_user((uint8_t*)tmp);
    if(ch==-1||ch=='\0')
      break;
    tmp++;
  }
  if(ch=='\0')
    return true;
  else
    return false;
}
	
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}