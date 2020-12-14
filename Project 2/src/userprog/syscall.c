#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys/directory.h"

#define STDIN 0
#define STDOUT 1

#define SYS_CALL_NUM 20
static void syscall_handler (struct intr_frame *f);

void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
tid_t exec (const char *file);
int wait (tid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

static struct file *find_file_by_fd (int fd);
static struct fd_entry *find_fd_entry_by_fd (int fd);
static int alloc_fid (void);
static struct fd_entry *find_fd_entry_by_fd_in_process (int fd);

static int all_fd = 2;
/*
file_node descriptors
*/
struct fd_entry{
  int fd;
  struct file *file_node;
  struct list_elem file_elem;
  struct list_elem thread_elem;
};

static struct list file_list;

static int
generate_fd (void)
{
  return ++all_fd;
}

/*
find fd_entry in current's thread fd_list
*/
static struct fd_entry *
get_file (int fd)
{
  struct fd_entry *ret;
  struct list_elem *l;
  struct thread *t;

  t = thread_current ();

  for (l = list_begin (&t->fd_list); l != list_end (&t->fd_list); l = list_next (l))
    {
      ret = list_entry (l, struct fd_entry, thread_elem);
      if (ret->fd == fd)
        return ret;
    }

  return NULL;
}

/*
find file_node be fd id
*/
static struct file *
find_file_by_fd (int fd)
{
  struct fd_entry *ret;

  ret = find_fd_entry_by_fd (fd);
  if (!ret)
    return NULL;
  return ret->file_node;
}


bool create (const char *file, unsigned initial_size){
    return filesys_create(file,initial_size);
}

bool remove (const char *file){
  return filesys_remove(file);
}

int open (const char *file){
  struct file* tfile = filesys_open(file);

  if(tfile!=NULL){
    struct fd_entry* tmyfile = (struct myfile*) malloc(sizeof(struct fd_entry));
    if(tmyfile==NULL){
      file_close(tfile);
      return -1;
    }
    tmyfile->file_node = tfile;
    tmyfile->fd = generate_fd();
    list_push_back(&file_list, &tmyfile->file_elem);
    list_push_back(&thread_current()->fd_list, &tmyfile->thread_elem);
    return tmyfile->fd;
  }
  return -1;
}

int wait (tid_t tid){
  return process_wait(tid);
}

int write (int fd, const void *buffer, unsigned length){
//文件描述符不是标准输出 写到文件中
  if(fd!=STDOUT){
    struct file *f = find_file_by_fd(fd);
    if(f==NULL){
      exit(-1);
    }
    return (int) file_write(f,buffer,length);
  }
  //文件描述符为标准输出文件 写到标准输出
  else{
      putbuf((char *) buffer,(size_t)length);
      return (int)length;
  }
}

void exit(int status){

  /* Close all the files */
struct thread *t = thread_current ();
struct list_elem *e;
/*close file_node*/
while (!list_empty (&t->fd_list))
  {
    e = list_begin (&t->fd_list);
    close (list_entry (e, struct fd_entry, thread_elem)->fd);
  }
/*free children*/
while (!list_empty (&t->children))
     {
       e = list_pop_front (&t->children);
       struct child_thread *c = list_entry(e,struct child_thread,elem);
       free(c);
     }

t->exit_status = status;
thread_exit ();
}

void close (int fd){
  struct fd_entry *tmyfile = get_file(fd);
  if(tmyfile==NULL){
    exit(-1);
    return;
  }
  file_close(tmyfile->file_node);
  list_remove(&tmyfile->file_elem);
  list_remove(&tmyfile->thread_elem);
  free(tmyfile);
}

int read (int fd, void *buffer, unsigned length){
  // fd不是标准输入文件
  if(fd!=STDIN){
    struct file *f = find_file_by_fd(fd);
    if(f == NULL){
      return -1;
    }
    return file_read(f,buffer,length);
  }
  // fd 是标准输入文件
  else{
    for(unsigned int i=0;i<length;i++){
      *((char **)buffer)[i] = input_getc();
    }
    return length;
  }
}

tid_t exec (const char *file){
  return process_execute(file);
}

void seek (int fd, unsigned position){
  struct file *f = find_file_by_fd(fd);
  if(f == NULL){
    exit(-1);
  }
  file_seek(f,position);
}

int filesize (int fd){
  struct fd_entry *tmyfile = get_file(fd);
  if(tmyfile==NULL){
    exit(-1);
  } else{
    int t = file_length(tmyfile->file_node);
    return t;
  }
}

unsigned tell (int fd){
  struct file *f = find_file_by_fd(fd);
  if(f == NULL){
    exit(-1);
  }
  return file_tell(f);
}



void
syscall_init (void)

{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
  list_init (&file_list);
}


static int
get_user (const uint8_t *uaddr)
{
    //printf("%s\n", "call get user");
  if(!is_user_vaddr((void *)uaddr)){
    return -1;
  }
  if(pagedir_get_page(thread_current()->pagedir,uaddr)==NULL){
    return -1;
  }
  //printf("%s\n","is_user_vaddr" );
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

static bool
put_user (uint8_t *udst, uint8_t byte)
{
  if(!is_user_vaddr(udst))
    return false;
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

bool is_valid_pointer(void* esp,uint8_t argc){
  uint8_t i = 0;
for (; i < argc; ++i)
{
   if (get_user(((uint8_t *)esp)+i) == -1){
     return false;
   }
  if((!is_user_vaddr(esp))||(pagedir_get_page(thread_current()->pagedir,esp)==NULL)){
    return false;
  }
}
return true;
}

bool is_valid_string(void *str){
  //return true;
  int ch=-1;
while((ch=get_user((uint8_t*)str++))!='\0' && ch!=-1);
  if(ch=='\0')
    return true;
  else
    return false;
}


static void
syscall_handler (struct intr_frame *f)
{
  //printf ("system call!\n");
  if(!is_valid_pointer(f->esp,4)){
    exit(-1);
    return;
  }
  int syscall_num = * (int *)f->esp;
  //printf("system call number %d\n", syscall_num);
  if(syscall_num<=0||syscall_num>=SYS_CALL_NUM){
    exit(-1);
  }
  // printf("sys call number: %d\n",syscall_num );
  char* file_name;
  unsigned size;
  int fd;
  void *buffer;

  switch (syscall_num) {
    case SYS_HALT:
      shutdown();
      break;

    case SYS_EXIT:
      if(!is_valid_pointer(f->esp+4,4)){
        exit(-1);
      }
      int status = *(int *)(f->esp +4);
      exit(status);
      break;

    case SYS_WAIT:
      if(!is_valid_pointer(f->esp+4,4)){
        exit(-1);
      }
      tid_t tid = *((int*)f->esp+1);
      if(tid == -1){
        f->eax = -1;
        return;
      }
      f->eax = wait(tid);
      break;

    case SYS_CREATE:
      if(!is_valid_pointer(f->esp+4,4)){
        exit(-1);
      }
      file_name = *(char **)(f->esp+4);
      if(!is_valid_string(file_name)){
        exit(-1);
      }
      size = *(int *)(f->esp+8);
      f->eax = create(file_name,size);
      break;

    case SYS_REMOVE:
      if (!is_valid_pointer(f->esp +4, 4) || !is_valid_string(*(char **)(f->esp + 4))){
        exit(-1);
      }
      file_name = *(char **)(f->esp+4);
      f->eax = remove(file_name);
      break;

    case SYS_OPEN:
      if (!is_valid_pointer(f->esp +4, 4)){
        exit(-1);
      }
      if (!is_valid_string(*(char **)(f->esp + 4))){
        exit(-1);
      }
      file_name = *(char **)(f->esp+4);
      lock_acquire(&file_lock);
      f->eax = open(file_name);
      lock_release(&file_lock);
      break;

    case SYS_WRITE:
      if(!is_valid_pointer(f->esp+4,12)){
        exit(-1);
      }
      fd = *(int *)(f->esp +4);
      buffer = *(char**)(f->esp + 8);
      size = *(unsigned *)(f->esp + 12);
      if (!is_valid_pointer(buffer, 1) || !is_valid_pointer(buffer + size,1)){
        exit(-1);
      }
      lock_acquire(&file_lock);
      f->eax = write(fd,buffer,size);
      lock_release(&file_lock);
      break;

    case SYS_SEEK:
      if (!is_valid_pointer(f->esp +4, 8)){
        exit(-1);
      }
      fd = *(int *)(f->esp + 4);
      unsigned pos = *(unsigned *)(f->esp + 8);
      seek(fd,pos);
      break;

    case SYS_TELL:
      if (!is_valid_pointer(f->esp +4, 4)){
        exit(-1);
      }
      fd = *(int *)(f->esp + 4);
      f->eax = tell(fd);
      break;

    case SYS_CLOSE:
      if (!is_valid_pointer(f->esp +4, 4)){
        return exit(-1);
      }
      fd = *(int *)(f->esp + 4);
      close(fd);
      break;

    case SYS_EXEC:
      if(!is_valid_pointer(f->esp+4,4)||!is_valid_string(*(char **)(f->esp + 4))){
        exit(-1);
      }
      file_name = *(char **)(f->esp+4);
      /*copy file name to handle '/0'*/
      char *newfile_name = (char*)malloc(sizeof(char)*(strlen(file_name)+1));
      memcpy(newfile_name,file_name,strlen(file_name)+1);
      lock_acquire(&file_lock);
      f->eax  = exec(newfile_name);
      free(newfile_name);
      lock_release(&file_lock);
      break;

    case SYS_READ:
      if (!is_valid_pointer(f->esp + 4, 12)){
        exit(-1);
      }
      fd = *(int *)(f->esp + 4);
      buffer = *(char**)(f->esp + 8);
      size = *(unsigned *)(f->esp + 12);
      if (!is_valid_pointer(buffer, 1) || !is_valid_pointer(buffer + size,1)){
        exit(-1);
      }
      lock_acquire(&file_lock);
      f->eax = read(fd,buffer,size);
      lock_release(&file_lock);
      break;

    case SYS_FILESIZE:
      if (!is_valid_pointer(f->esp +4, 4)){
        exit(-1);
      }
      fd = *(int *)(f->esp + 4);
      f->eax = filesize(fd);
      break;

    default:
      exit(-1);
      break;
  }

}


static struct fd_entry *
find_fd_entry_by_fd (int fd)
{
  struct fd_entry *ret;
  struct list_elem *l;

  for (l = list_begin (&file_list); l != list_end (&file_list); l = list_next (l))
    {
      ret = list_entry (l, struct fd_entry, file_elem);
      if (ret->fd == fd)
        return ret;
    }

  return NULL;
}