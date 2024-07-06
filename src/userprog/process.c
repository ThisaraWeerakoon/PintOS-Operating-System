#include "userprog/process.h"
#include "userprog/syscall.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static thread_func start_process NO_RETURN;
static bool load(const char *cmdline, void (**eip)(void), void **esp);
static void pushing_arguments_to_the_stack(char **args, int argc, void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */

pid_t process_execute(const char *file_name)
{
  char *file_name_copy = NULL;
  char *program_name = NULL;
  struct process_control_block *pcb = NULL;
  tid_t new_thread_id;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  file_name_copy = palloc_get_page(0);
  if (file_name_copy == NULL)
    goto process_execute_error;
  strlcpy(file_name_copy, file_name, PGSIZE);

  /* Make an additional copy to store just the program name */
  char *save_ptr = NULL;
  program_name = palloc_get_page(0);
  if (program_name == NULL)
    goto process_execute_error;
  strlcpy(program_name, file_name, PGSIZE);
  program_name = strtok_r(program_name, " ", &save_ptr);

  /* Create a new thread to execute FILE_NAME. */
  pcb = palloc_get_page(0);
  if (pcb == NULL)
    goto process_execute_error;

  /* Initialize Process Control Block (PCB) */
  pcb->pid = -2;
  pcb->command = file_name_copy;
  pcb->waiting = false;
  pcb->exited = false;
  pcb->exit_code = -1;
  sema_init(&pcb->waiting_sema, 0);
  sema_init(&pcb->initialization_sema, 0);

  new_thread_id = thread_create(program_name, PRI_DEFAULT, start_process, pcb);

  if (new_thread_id == TID_ERROR)
    goto process_execute_error;

  sema_down(&pcb->initialization_sema);

  palloc_free_page(file_name_copy);
  palloc_free_page(program_name);

  if (pcb->pid >= 0) /* Executable successfully loaded */
    list_push_back(&thread_current()->child_list, &pcb->elem);
  return pcb->pid;

process_execute_error:
  if (file_name_copy)
    palloc_free_page(file_name_copy);
  if (program_name)
    palloc_free_page(program_name);
  if (pcb)
    palloc_free_page(pcb);

  return TID_ERROR;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process(void *pcb_)
{
  struct process_control_block *pcb = pcb_;
  char *command = pcb->command;
  struct intr_frame interrupt_frame;
  bool load_success;
  struct thread *current_thread;

  /* Tokenize command */
  char *token, *token_save_ptr;
  char **arguments = palloc_get_page(0);
  if (arguments == NULL)
    goto start_process_finished;
  int argument_count = 0;
  token = strtok_r(command, " ", &token_save_ptr);
  while (token != NULL)
  {
    arguments[argument_count++] = token;
    token = strtok_r(NULL, " ", &token_save_ptr);
  }

  /* Initialize interrupt frame and load executable. */
  memset(&interrupt_frame, 0, sizeof interrupt_frame);
  interrupt_frame.gs = interrupt_frame.fs = interrupt_frame.es = interrupt_frame.ds = interrupt_frame.ss = SEL_UDSEG;
  interrupt_frame.cs = SEL_UCSEG;
  interrupt_frame.eflags = FLAG_IF | FLAG_MBS;
  load_success = load(command, &interrupt_frame.eip, &interrupt_frame.esp);

  if (load_success)
    pushing_arguments_to_the_stack(arguments, argument_count, &interrupt_frame.esp);

  palloc_free_page(arguments);

start_process_finished:

  current_thread = thread_current();
  pcb->pid = load_success ? (pid_t)current_thread->tid : (pid_t)-1;
  current_thread->pcb = pcb;

  sema_up(&pcb->initialization_sema);

  /* If load failed, quit. */
  if (!load_success)
    sys_exit(-1);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile("movl %0, %%esp; jmp intr_exit"
               :
               : "g"(&interrupt_frame)
               : "memory");
  NOT_REACHED();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int process_wait(tid_t child_tid)
{
  /* Find the child process */
  struct process_control_block *target_child_pcb = NULL;
  struct list *child_list = &thread_current()->child_list;
  struct list_elem *elem;

  if (!list_empty(child_list))
  {
    for (elem = list_begin(child_list); elem != list_end(child_list);
         elem = list_next(elem))
    {
      struct process_control_block *child_pcb =
          list_entry(elem, struct process_control_block, elem);
      if (child_pcb->pid == child_tid)
      {
        target_child_pcb = child_pcb;
        break;
      }
    }
  }

  /* Return -1 if not a direct child process */
  if (target_child_pcb == NULL)
    return -1;

  /* Return -1 if called twice */
  if (target_child_pcb->waiting)
    return -1;
  else
    target_child_pcb->waiting = true;

  /* Wait for the child process and retrieve the exit status */
  if (!target_child_pcb->exited)
    sema_down(&target_child_pcb->waiting_sema);
  ASSERT(target_child_pcb->exited == true);

  list_remove(elem);

  int child_exit_code = target_child_pcb->exit_code;
  palloc_free_page(target_child_pcb);
  return child_exit_code;
}

/* Free the current process's resources. */
void process_exit(void)
{
  struct thread *current_thread = thread_current();
  uint32_t *page_directory;

  /* Close all opened files and free memory */
  struct list *file_descriptors = &current_thread->file_descriptors;
  struct file_desc *file_descriptor;

  while (!list_empty(file_descriptors))
  {
    file_descriptor = list_entry(list_pop_front(file_descriptors), struct file_desc, elem);
    file_close(file_descriptor->file);
    free(file_descriptor);
  }

  /* Free PCBs of all child processes */
  struct list *child_processes = &current_thread->child_list;
  struct process_control_block *child_pcb;

  while (!list_empty(child_processes))
  {
    child_pcb = list_entry(list_pop_front(child_processes), struct process_control_block, elem);
    palloc_free_page(child_pcb);
  }

  sema_up(&current_thread->pcb->waiting_sema);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  page_directory = current_thread->pagedir;

  if (page_directory != NULL)
  {
    /* Correct ordering here is crucial. We must set
       current_thread->pagedir to NULL before switching page directories,
       so that a timer interrupt can't switch back to the
       process page directory. We must activate the base page
       directory before destroying the process's page
       directory, or our active page directory will be one
       that's been freed (and cleared). */
    current_thread->pagedir = NULL;
    pagedir_activate(NULL);
    pagedir_destroy(page_directory);
  }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void process_activate(void)
{
  struct thread *t = thread_current();

  /* Activate thread's page tables. */
  pagedir_activate(t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32 /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32 /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32 /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16 /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
  unsigned char e_ident[16];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL 0           /* Ignore. */
#define PT_LOAD 1           /* Loadable segment. */
#define PT_DYNAMIC 2        /* Dynamic linking info. */
#define PT_INTERP 3         /* Name of dynamic loader. */
#define PT_NOTE 4           /* Auxiliary info. */
#define PT_SHLIB 5          /* Reserved. */
#define PT_PHDR 6           /* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

static bool setup_stack(void **esp);
static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */

bool load(const char *file_name, void (**entry_point)(void), void **stack_ptr)
{
  struct thread *current_thread = thread_current();
  struct Elf32_Ehdr elf_header;
  struct file *executable_file = NULL;
  off_t file_offset;
  bool load_success = false;
  int i;

  /* Allocate and activate page directory. */
  current_thread->pagedir = pagedir_create();
  if (current_thread->pagedir == NULL)
    goto done;
  process_activate();

  /* Open the executable file. */
  executable_file = filesys_open(file_name);
  if (executable_file == NULL)
  {
    printf("load: %s: open failed\n", file_name);
    goto done;
  }

  /* Read and verify the executable header. */
  if (file_read(executable_file, &elf_header, sizeof elf_header) != sizeof elf_header || memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) || elf_header.e_type != 2 || elf_header.e_machine != 3 || elf_header.e_version != 1 || elf_header.e_phentsize != sizeof(struct Elf32_Phdr) || elf_header.e_phnum > 1024)
  {
    printf("load: %s: error loading executable\n", file_name);
    goto done;
  }

  /* Read program headers. */
  file_offset = elf_header.e_phoff;
  for (i = 0; i < elf_header.e_phnum; i++)
  {
    struct Elf32_Phdr program_header;

    if (file_offset < 0 || file_offset > file_length(executable_file))
      goto done;
    file_seek(executable_file, file_offset);

    if (file_read(executable_file, &program_header, sizeof program_header) != sizeof program_header)
      goto done;
    file_offset += sizeof program_header;
    switch (program_header.p_type)
    {
    case PT_NULL:
    case PT_NOTE:
    case PT_PHDR:
    case PT_STACK:
    default:
      /* Ignore this segment. */
      break;
    case PT_DYNAMIC:
    case PT_INTERP:
    case PT_SHLIB:
      goto done;
    case PT_LOAD:
      if (validate_segment(&program_header, executable_file))
      {
        bool is_writable = (program_header.p_flags & PF_W) != 0;
        uint32_t file_page = program_header.p_offset & ~PGMASK;
        uint32_t mem_page = program_header.p_vaddr & ~PGMASK;
        uint32_t page_offset = program_header.p_vaddr & PGMASK;
        uint32_t bytes_to_read, zero_bytes;
        if (program_header.p_filesz > 0)
        {
          /* Normal segment.
             Read the initial part from the disk and zero the rest. */
          bytes_to_read = page_offset + program_header.p_filesz;
          zero_bytes = (ROUND_UP(page_offset + program_header.p_memsz, PGSIZE) - bytes_to_read);
        }
        else
        {
          /* Entirely zero.
             Don't read anything from the disk. */
          bytes_to_read = 0;
          zero_bytes = ROUND_UP(page_offset + program_header.p_memsz, PGSIZE);
        }
        if (!load_segment(executable_file, file_page, (void *)mem_page, bytes_to_read, zero_bytes, is_writable))
          goto done;
      }
      else
        goto done;
      break;
    }
  }

  /* Set up the stack. */
  if (!setup_stack(stack_ptr))
    goto done;

  /* Entry point. */
  *entry_point = (void (*)(void))elf_header.e_entry;

  load_success = true;

done:
  /* We arrive here whether the load is successful or not. */
  file_close(executable_file);
  return load_success;
}

/* load() helpers. */

static bool install_page(void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment(const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off)file_length(file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr((void *)phdr->p_vaddr))
    return false;
  if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */

static bool
load_segment(struct file *file, off_t file_offset, uint8_t *user_page,
             uint32_t bytes_to_read, uint32_t zero_bytes, bool is_writable)
{
  ASSERT((bytes_to_read + zero_bytes) % PGSIZE == 0);
  ASSERT(pg_ofs(user_page) == 0);
  ASSERT(file_offset % PGSIZE == 0);

  file_seek(file, file_offset);
  while (bytes_to_read > 0 || zero_bytes > 0)
  {
    /* Calculate how to fill this page.
       We will read PAGE_READ_BYTES bytes from FILE
       and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = bytes_to_read < PGSIZE ? bytes_to_read : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Get a page of memory. */
    uint8_t *kernel_page = palloc_get_page(PAL_USER);
    if (kernel_page == NULL)
      return false;

    /* Load this page. */
    if (file_read(file, kernel_page, page_read_bytes) != (int)page_read_bytes)
    {
      palloc_free_page(kernel_page);
      return false;
    }
    memset(kernel_page + page_read_bytes, 0, page_zero_bytes);

    /* Add the page to the process's address space. */
    if (!install_page(user_page, kernel_page, is_writable))
    {
      palloc_free_page(kernel_page);
      return false;
    }

    /* Advance. */
    bytes_to_read -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    user_page += PGSIZE;
  }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack(void **esp)
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  if (kpage != NULL)
  {
    success = install_page(((uint8_t *)PHYS_BASE) - PGSIZE, kpage, true);
    if (success)
      *esp = PHYS_BASE;
    else
      palloc_free_page(kpage);
  }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page(void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page(t->pagedir, upage) == NULL && pagedir_set_page(t->pagedir, upage, kpage, writable));
}

/* Setup stack at ESP with ARGC arguments from ARGS array. */
static void
pushing_arguments_to_the_stack(char **argument_strings, int argument_count, void **stack_pointer)
{
  int string_length;
  int argument_addresses[argument_count];
  int i = argument_count - 1;
  while (i >= 0)
  {
    string_length = strlen(argument_strings[i]) + 1;
    *stack_pointer -= string_length;
    memcpy(*stack_pointer, argument_strings[i], string_length);
    argument_addresses[i] = (int)*stack_pointer;
    i--;
  }

  /* Word align */
  *stack_pointer = (void *)((int)*stack_pointer & 0xfffffffc);

  /* Null */
  *stack_pointer -= 4;
  *(int *)*stack_pointer = 0;

  /* Addresses to arguments last to first, top to bottom */
  for (int i = argument_count - 1; i >= 0; i--)
  {
    *stack_pointer -= 4;
    *(int *)*stack_pointer = argument_addresses[i];
  }

  /* Address to the first argument */
  *stack_pointer -= 4;
  *(int *)*stack_pointer = (int)*stack_pointer + 4;

  /* Argument count (argc) */
  *stack_pointer -= 4;
  *(int *)*stack_pointer = argument_count;

  /* Return address (0) */
  *stack_pointer -= 4;
  *(int *)*stack_pointer = 0;
}
