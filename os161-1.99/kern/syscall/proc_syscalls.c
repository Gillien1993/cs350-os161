#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

#include "opt-A2.h"
#include <synch.h>
#include <mips/trapframe.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

  //A helper function for sys__exit to recursively free all it's children's procinfo in the procinfotable.
void free_children(int ppid);
//void thread_forkhelper(void *tf, unsigned long data2);


void free_children(int ppid){
  struct procinfo *cpi;

  //struct procinfo *ppi = array_get(procinfotable, ppid);
  for(unsigned int i = 0; i<array_num(procinfotable); i++){
    cpi = array_get(procinfotable, i);
    if(cpi == NULL){
      continue;
    }
    if(cpi->parent_pid == ppid){
      cv_destroy(cpi->waitpid_cv);
      array_set(procinfotable, i, NULL);
      kfree(cpi);
    } 
  }
}

/*
void free_root(struct procinfo* pi){
  KASSERT(pi != NULL);
  if (pi->parent_pid == -1){
    array_set(proc)
  }
  
}*/
void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  #if OPT_A2
  
  struct procinfo *pi = array_get(procinfotable, p->pid-1);
  
  if(pi == NULL){
    goto parentexited;
  }

  lock_acquire(p->p_waitpid_lock);
  
  pi->exit_code = _MKWAIT_EXIT(exitcode);
  pi->active = 0;
  cv_broadcast(pi->waitpid_cv,p->p_waitpid_lock);

  lock_release(p->p_waitpid_lock);

  free_children(p->pid);
  
parentexited:  
  #else

  (void)exitcode;

  #endif
  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
#if OPT_A2
  
  struct procinfo* pi = array_get(procinfotable,pid-1);
  if(pi == NULL){return(ECHILD);}
  if(pi->parent_pid != curproc->pid){return(ECHILD);}

  lock_acquire(curproc->p_waitpid_lock);

  while(pi->active == 1){
    cv_wait(pi->waitpid_cv, curproc->p_waitpid_lock);
  }

  lock_release(curproc->p_waitpid_lock);

  exitstatus = pi->exit_code;


#else
  exitstatus = 0;
#endif  

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}


#if OPT_A2

/*
void thread_forkhelper(void *tf, unsigned long data2){
  struct trapframe childtf = *tf; //move it on stack
  kfree(tf);

  enter_forked_process(&childtf);


}

*/


int sys_fork(struct trapframe *tf, pid_t *retval){

  const char* name = "Anonymous student's process";


// create process struct for child
  struct proc *childp = proc_create_runprogram(name);
  if(childp == NULL){
    return ENOMEM; //OUT OF MEMORY
  }

// create and copy as to child
  struct addrspace* childas = kmalloc(sizeof(struct addrspace));
  if(childas == NULL){
    proc_destroy(childp);
    return ENOMEM;
  }

  if(as_copy(curproc->p_addrspace, &childas)){
    kfree(childas);
    as_destroy(childas);
    proc_destroy(childp);
    return ENOMEM;
  }
// attach new as to child proc
  childp->p_addrspace = childas;
  
// create parent child relationship
  struct procinfo *pi = array_get(procinfotable, childp->pid - 1);
  if(pi == NULL){
    kfree(childas);
    as_destroy(childas);
    proc_destroy(childp);
    return ECHILD; // NO SUCH CHILD IN THE INFO TABLE
  }
  pi->parent_pid = curproc->pid;

// create thread for child and pass it tf
  struct trapframe *childtf = kmalloc(sizeof(struct trapframe));
  memcpy(childtf,tf,sizeof(struct trapframe));

  void ** data = kmalloc(2*sizeof(void*));
  data[0] = (void*)childtf;
  data[1] = (void*)childas; 

  int err = thread_fork(name, childp,(void *)enter_forked_process, data, 0);
  if(err){
    kfree(childas);
    kfree(childtf);
    as_destroy(childas);
    proc_destroy(childp);
    kfree(childtf);
    return ENOMEM;
  }


  *retval = childp->pid;
  return 0;
}
#endif
