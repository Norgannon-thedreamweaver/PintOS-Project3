#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/interrupt.h"
#include <stdbool.h>
#include <debug.h>
#include <list.h>
#include "filesys/file.h"
/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

/* Maximum characters in a filename written by readdir(). */
#define READDIR_MAX_LEN 14

/* Typical return values from main() and arguments to exit(). */
#define EXIT_SUCCESS 0          /* Successful execution. */
#define EXIT_FAILURE 1          /* Unsuccessful execution. */

#define STDIN 0
#define STDOUT 1

struct process_file{
    int fd;
    struct file *file;
    struct list_elem elem;
};

struct lock file_lock;


void syscall_init (void);

void exit (int status);
pid_t exec (const char *file);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
/*
void syscall_halt (struct intr_frame* f);
void syscall_exit (struct intr_frame* f);
void syscall_exec (struct intr_frame* f);
void syscall_wait (struct intr_frame* f);
void syscall_create (struct intr_frame* f);
void syscall_remove (struct intr_frame* f);
void syscall_open (struct intr_frame* f);
void syscall_filesize (struct intr_frame* f);
void syscall_read (struct intr_frame* f);
void syscall_write (struct intr_frame* f);
void syscall_seek (struct intr_frame* f);
void syscall_tell (struct intr_frame* f);
void syscall_close (struct intr_frame* f);
*/
struct process_file* get_process_file_by_fd(int fd);
bool is_valid_addr(const void *vaddr);
bool is_valid_buffer (void *vaddr, unsigned size);
bool is_valid_string(void *str);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

#endif /* userprog/syscall.h */
