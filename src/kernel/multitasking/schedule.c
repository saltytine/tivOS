#include <bootloader.h>
#include <gdt.h>
#include <isr.h>
#include <malloc.h>
#include <paging.h>
#include <schedule.h>
#include <syscalls.h>
#include <system.h>
#include <task.h>
#include <timer.h>
#include <util.h>
#include <vmm.h>

// Clock-tick triggered scheduler

#define SCHEDULE_DEBUG 0

extern TSSPtr *tssPtr;
extern void    asm_finalize_sched(uint64_t rsp, uint64_t cr3, Task *old);

void schedule(uint64_t rsp) {
  if (!tasksInitiated)
    return;

  // try to find a next task
  AsmPassedInterrupt *cpu = (AsmPassedInterrupt *)rsp;
  Task               *next = currentTask->next;
  if (!next)
    next = firstTask;

  int fullRun = 0;
  while (next->state != TASK_STATE_READY ||
         (next->sleepUntil && next->sleepUntil > timerTicks)) {
    next = next->next;
    if (!next) {
      fullRun++;
      if (fullRun > 2)
        break;
      next = firstTask;
    }
  }

  // found no task
  if (!next)
    next = dummyTask;

  Task *old = currentTask;

  currentTask = next;

#if SCHEDULE_DEBUG
  // if (old->id != 0 || next->id != 0)
  debugf("[scheduler] Switching context: id{%d} -> id{%d}\n", old->id,
         next->id);
  debugf("cpu->usermode_rsp{%lx} rip{%lx} fsbase{%lx} gsbase{%lx}\n",
         next->registers.usermode_rsp, next->registers.rip, old->fsbase,
         old->gsbase);
#endif

  // Before doing anything, handle any signals
  signalsPendingHandle(next);

  // Change TSS rsp0 (software multitasking)
  tssPtr->rsp0 = next->whileTssRsp;
  threadInfo.syscall_stack = next->whileSyscallRsp;

  // Save MSRIDs (HIGHLY unsure)
  // old->fsbase = rdmsr(MSRID_FSBASE);
  // old->gsbase = rdmsr(MSRID_GSBASE);

  // Apply new MSRIDs
  wrmsr(MSRID_FSBASE, next->fsbase);
  wrmsr(MSRID_GSBASE, next->gsbase);
  wrmsr(MSRID_KERNEL_GSBASE, (size_t)&threadInfo);

  // Save generic (and non) registers
  memcpy(&old->registers, cpu, sizeof(AsmPassedInterrupt));

  // Apply new generic (and non) registers (not needed!)
  // memcpy(cpu, &next->registers, sizeof(AsmPassedInterrupt));

  // Apply pagetable (not needed!)
  // ChangePageDirectoryUnsafe(next->pagedir);

  // Save & load appropriate FPU state
  if (!old->kernel_task) {
    asm volatile(" fxsave %0 " ::"m"(old->fpuenv));
    asm("stmxcsr (%%rax)" : : "a"(&old->mxcsr));
  }

  if (!next->kernel_task) {
    asm volatile(" fxrstor %0 " ::"m"(next->fpuenv));
    asm("ldmxcsr (%%rax)" : : "a"(&next->mxcsr));
  }

  // Cleanup any old tasks left dead (not needed!)
  // if (old->state == TASK_STATE_DEAD)
  //   taskKillCleanup(old);

  // Put next task's registers in tssRsp
  AsmPassedInterrupt *iretqRsp =
      (AsmPassedInterrupt *)(next->whileTssRsp - sizeof(AsmPassedInterrupt));
  memcpy(iretqRsp, &next->registers, sizeof(AsmPassedInterrupt));

  // Pass off control to our assembly finalization code that:
  //   - uses the tssRsp to iretq (give control back)
  //   - applies the new pagetable
  //   - cleanups old killed task (if necessary)
  // .. basically replaces all (not needed!) stuff
  uint64_t *pagedir =
      next->pagedirOverride ? next->pagedirOverride : next->infoPd->pagedir;
  ChangePageDirectoryFake(pagedir);
  // ^ just for globalPagedir to update (note potential race cond)
  asm_finalize_sched((size_t)iretqRsp, VirtualToPhysical((size_t)pagedir), old);
}
