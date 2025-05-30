#include <bootloader.h>
#include <caching.h>
#include <dents.h>
#include <malloc.h>
#include <proc.h>
#include <string.h>
#include <system.h>
#include <task.h>
#include <timer.h>
#include <util.h>

#include <fb.h>
#include <syscalls.h>

// todo: lay this out a better way

Fakefs rootProc = {0};

typedef struct UserspaceProc {
  uint64_t pid;

  Spinlock LOCK_PROP;
  int      timesOpened;
} UserspaceProc;

size_t meminfoRead(OpenFile *fd, uint8_t *out, size_t limit) {
  char   buff[1024] = {0};
  size_t allocated = physical.allocatedSizeInBlocks * BLOCK_SIZE / 1024;
  size_t total = bootloader.mmTotal / 1024;
  size_t free = total - allocated;

  size_t cached = cachingInfoBlocks() * BLOCK_SIZE / 1024;
  size_t available = free + cached;

  size_t length = snprintf(buff, 1024,
                           "%-15s %10lu kB\n"
                           "%-15s %10lu kB\n"
                           "%-15s %10lu kB\n"
                           "%-15s %10lu kB\n",
                           "MemTotal:", total, "MemFree:", free,
                           "MemAvailable:", available, "Cached:", cached);

  size_t toCopy = MIN(length - fd->pointer, limit);
  memcpy(out, buff, toCopy);
  fd->pointer += toCopy;
  return toCopy;
}
VfsHandlers handleMeminfo = {
    .read = meminfoRead, .seek = fsSimpleSeek, .stat = fakefsFstat};

size_t uptimeRead(OpenFile *fd, uint8_t *out, size_t limit) {
  char   buff[1024] = {0};
  size_t secs = timerTicks / 1000;
  int    msFirstTwo = (timerTicks % 1000) / 10;

  size_t length = snprintf(buff, 1024, "%ld.%02d 0.00\n", secs, msFirstTwo);

  size_t toCopy = MIN(length - fd->pointer, limit);
  memcpy(out, buff, toCopy);
  fd->pointer += toCopy;
  return toCopy;
}
VfsHandlers handleUptime = {
    .read = uptimeRead, .seek = fsSimpleSeek, .stat = fakefsFstat};

size_t statRead(OpenFile *fd, uint8_t *out, size_t limit) {
  // todo: more!
  char   buff[1024] = {0};
  size_t length = snprintf(buff, 1024, "btime %ld\n", timerBootUnix);

  size_t toCopy = MIN(length - fd->pointer, limit);
  memcpy(out, buff, toCopy);
  fd->pointer += toCopy;
  return toCopy;
}
VfsHandlers handleStat = {
    .read = statRead, .seek = fsSimpleSeek, .stat = fakefsFstat};

char procDetermineState(Task *task) {
  if (task->state == TASK_STATE_READY)
    return 'R';
  if (task->state == TASK_STATE_DEAD || task->state == TASK_STATE_CREATED)
    return 'X';
  if (signalsRevivableState(task->state))
    return 'S';
  return 'D';
}

size_t procCmdlineRead(OpenFile *fd, uint8_t *out, size_t limit) {
  UserspaceProc *uproc = fd->dir;
  Task          *target = taskGet(uproc->pid);
  assert(target);
  size_t toCopy = MIN(target->cmdlineLen - fd->pointer, limit);
  memcpy(out, &target->cmdline[fd->pointer], toCopy);
  fd->pointer += toCopy;
  return toCopy;
}

VfsHandlers handleProcCmdline = {.read = procCmdlineRead,
                                 .stat = fakefsFstat,
                                 .open = procEachOpen,
                                 .duplicate = procEachDuplicate,
                                 .close = procEachClose};

size_t procStatRead(OpenFile *fd, uint8_t *out, size_t limit) {
  UserspaceProc *uproc = fd->dir;
  Task          *target = taskGet(uproc->pid);
  assert(target);

  int32_t pid = target->id;
  char    comm[12] = {0};
  char    state = procDetermineState(target);

  int32_t  ppid = target->parent ? target->parent->id : 0;
  int32_t  pgrp = target->pgid;
  int32_t  session = 1234;
  int32_t  tty_nr = 0;
  int32_t  tpgid = -1;
  uint32_t flags = 0;

  uint64_t minflt = 10;
  uint64_t cminflt = 20;
  uint64_t majflt = 1;
  uint64_t cmajflt = 2;

  uint64_t utime = 100;
  uint64_t stime = 200;
  int64_t  cutime = 10;
  int64_t  cstime = 5;

  int64_t priority = 20;
  int64_t nice = 0;
  int64_t num_threads = 1;
  int64_t itrealvalue = 0;

  uint64_t starttime = 123456789;

  uint64_t vsize = 12345678;
  int64_t  rss = 500;
  uint64_t rsslim = 0xffffffffffffffffULL;

  uint64_t startcode = 0x400000;
  uint64_t endcode = 0x401000;
  uint64_t startstack = 0x7fff0000;
  uint64_t kstkesp = 0x7fff1000;
  uint64_t kstkeip = 0x400123;

  uint64_t signal = 0, blocked = 0, sigignore = 0, sigcatch = 0;
  uint64_t wchan = 0, nswap = 0, cnswap = 0;

  int32_t  exit_signal = 17;
  int32_t  processor = 0;
  uint32_t rt_priority = 0, policy = 0;

  uint64_t delayacct_blkio_ticks = 0;
  uint64_t guest_time = 0, cguest_time = 0;

  uint64_t start_data = 0x601000;
  uint64_t end_data = 0x602000;
  uint64_t start_brk = 0x603000;

  uint64_t arg_start = 0x7fffffff0000;
  uint64_t arg_end = 0x7fffffff1000;
  uint64_t env_start = 0x7fffffff2000;
  uint64_t env_end = 0x7fffffff3000;

  int32_t exit_code = 0;

  int commStart = 0;
  int cmdline0len = strlength(target->cmdline);
  for (int i = 0; i < cmdline0len; i++) {
    if (target->cmdline[i] == '/')
      commStart = i + 1;
  }
  memcpy(comm, &target->cmdline[commStart], MIN(cmdline0len - commStart, 11));

  char statOutput[4096] = {0};
  int  len = snprintf(
      statOutput, 4096,
      "%d (%s) %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld "
       "%ld %ld %ld %ld %lu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu "
       "%lu %lu %lu %lu %lu %d %d %u %u %lu %lu %ld %lu %lu %lu %lu %lu %lu "
       "%lu %d",
      pid, comm, state, ppid, pgrp, session, tty_nr, tpgid, flags, minflt,
      cminflt, majflt, cmajflt, utime, stime, cutime, cstime, priority, nice,
      num_threads, itrealvalue, starttime, vsize, rss, rsslim, startcode,
      endcode, startstack, kstkesp, kstkeip, signal, blocked, sigignore,
      sigcatch, wchan, nswap, cnswap, exit_signal, processor, rt_priority,
      policy, delayacct_blkio_ticks, guest_time, cguest_time, start_data,
      end_data, start_brk, arg_start, arg_end, env_start, env_end, exit_code);
  size_t toCopy = MIN(len - fd->pointer, limit);
  memcpy(out, &statOutput[fd->pointer], toCopy);
  fd->pointer += toCopy;
  return toCopy;
}

VfsHandlers handleProcStat = {.read = procStatRead,
                              .stat = fakefsFstat,
                              .open = procEachOpen,
                              .duplicate = procEachDuplicate,
                              .close = procEachClose};

size_t procStatmRead(OpenFile *fd, uint8_t *out, size_t limit) {
  UserspaceProc *uproc = fd->dir;
  Task          *target = taskGet(uproc->pid);
  assert(target);

  char   statOutput[4096] = {0};
  int    len = snprintf(statOutput, 4096, "0 0 0 0 0 0 0");
  size_t toCopy = MIN(len - fd->pointer, limit);
  memcpy(out, &statOutput[fd->pointer], toCopy);
  fd->pointer += toCopy;
  return toCopy;
}

VfsHandlers handleProcStatm = {.read = procStatmRead,
                               .stat = fakefsFstat,
                               .open = procEachOpen,
                               .duplicate = procEachDuplicate,
                               .close = procEachClose};

size_t procStatusRead(OpenFile *fd, uint8_t *out, size_t limit) {
  // todo: more!
  char   buff[1024] = {0};
  size_t length = snprintf(buff, 1024, "fasdfasdfasd %ld\n", timerBootUnix);

  size_t toCopy = MIN(length - fd->pointer, limit);
  memcpy(out, buff, toCopy);
  fd->pointer += toCopy;
  return toCopy;
}

VfsHandlers handleProcStatus = {.read = procStatusRead,
                                .stat = fakefsFstat,
                                .open = procEachOpen,
                                .duplicate = procEachDuplicate,
                                .close = procEachClose};

// todo: take <pid>/task into account and put proper limits
size_t procEachOpen(char *filename, int flags, int mode, OpenFile *fd,
                    char **symlinkResolve) {
  char *badFn = strdup(filename);
  int   len = strlength(badFn);

  int slashesEncountered = 0;
  int pid0 = 0;

  for (int i = 0; i < len; i++) {
    if (badFn[i] == '/') {
      slashesEncountered++;
      badFn[i] = '\0';
      continue;
    }

    if (slashesEncountered == 1) {
      if (i == 1 && memcmp(&badFn[i], "self", 4) == 0)
        pid0 = currentTask->id;
      if (pid0 != currentTask->id) {
        if (!isdigit(badFn[i])) {
          free(badFn);
          return ERR(ENOENT);
        }
        if (badFn[i + 1] == '\0' || badFn[i + 1] == '/') {
          char original = badFn[i + 1];
          badFn[i + 1] = '\0';
          pid0 = atoi(&badFn[1]);
          badFn[i + 1] = original;
        }
      }
    }
  }
  free(badFn);
  int pid = pid0;

  UserspaceProc *uproc = calloc(sizeof(UserspaceProc), 1);
  uproc->pid = pid;
  uproc->timesOpened = 1;
  fd->dir = uproc;
  return 0;
}

bool procEachDuplicate(OpenFile *original, OpenFile *orphan) {
  orphan->dir = original->dir;
  UserspaceProc *uproc = original->dir;
  spinlockAcquire(&uproc->LOCK_PROP);
  uproc->timesOpened++;
  spinlockRelease(&uproc->LOCK_PROP);
  return true;
}

bool procEachClose(OpenFile *fd) {
  UserspaceProc *uproc = fd->dir;
  spinlockAcquire(&uproc->LOCK_PROP);
  uproc->timesOpened--;
  if (!uproc->timesOpened)
    free(uproc);
  else
    spinlockRelease(&uproc->LOCK_PROP);
  return true;
}

void procSetup() {
  fakefsAddFile(&rootProc, rootProc.rootFile, "meminfo", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleMeminfo);
  fakefsAddFile(&rootProc, rootProc.rootFile, "uptime", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleUptime);
  fakefsAddFile(&rootProc, rootProc.rootFile, "stat", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleStat);
  FakefsFile *id =
      fakefsAddFile(&rootProc, rootProc.rootFile, "*", 0,
                    S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH, &fakefsRootHandlers);
  // after the catch-all to avoid addding the below handlers!
  fakefsAddFile(&rootProc, rootProc.rootFile, "self", 0,
                S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH, &fakefsNoHandlers);

  fakefsAddFile(&rootProc, id, "cmdline", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleProcCmdline);
  fakefsAddFile(&rootProc, id, "stat", 0, S_IFREG | S_IRUSR | S_IRGRP | S_IROTH,
                &handleProcStat);
  fakefsAddFile(&rootProc, id, "statm", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleProcStatm);
  fakefsAddFile(&rootProc, id, "status", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleProcStatus);
  FakefsFile *task =
      fakefsAddFile(&rootProc, id, "task", 0,
                    S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH, &fakefsRootHandlers);

  FakefsFile *taskId =
      fakefsAddFile(&rootProc, task, "*", 0,
                    S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &fakefsRootHandlers);
  fakefsAddFile(&rootProc, taskId, "cmdline", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleProcCmdline);
  fakefsAddFile(&rootProc, taskId, "stat", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleProcStat);
  fakefsAddFile(&rootProc, taskId, "statm", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleProcStatm);
  fakefsAddFile(&rootProc, taskId, "status", 0,
                S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, &handleProcStatus);
}

// todo: global: respect lseek of 0 and do a more standardized pointer system

size_t procGetdents64(OpenFile *fd, struct linux_dirent64 *start,
                      unsigned int hardlimit) {
  // todo: fakefs' getdents64() uses ->tmp1 instead of ->pointer
  size_t read = fakefsGetDents64(fd, start, hardlimit);
  if (read)
    return read;

  size_t initPtr = fd->pointer;

  struct linux_dirent64 *dirp = (struct linux_dirent64 *)start;
  size_t                 allocatedlimit = 0;

  spinlockCntReadAcquire(&TASK_LL_MODIFY);
  char  filename[32] = {0};
  Task *browse = firstTask;
  while (browse) {
    // id == tgid so we don't have duplicates. todo thread killing elsewhere
    if (browse->id == browse->tgid && browse->state != TASK_STATE_DEAD &&
        browse->tgid > initPtr) {
      size_t out = snprintf(filename, 32, "%d", browse->tgid);
      assert(out > 0 && out <= 32); // bounds
      DENTS_RES res = dentsAdd(start, &dirp, &allocatedlimit, hardlimit,
                               filename, out, 999 + browse->tgid, 0);
      // todo: type ^^^

      if (res == DENTS_NO_SPACE) {
        allocatedlimit = ERR(EINVAL);
        goto cleanup;
      } else if (res == DENTS_RETURN)
        goto cleanup;

      fd->pointer = browse->tgid;
    }
    browse = browse->next;
  }

cleanup:
  spinlockCntReadRelease(&TASK_LL_MODIFY);
  return allocatedlimit;
}

VfsHandlers procRootHandlers = {.getdents64 = procGetdents64,
                                .stat = fakefsFstat};

bool procMount(MountPoint *mount) {
  // install handlers
  mount->handlers = &fakefsHandlers;
  mount->stat = fakefsStat;
  mount->lstat = fakefsLstat;

  mount->fsInfo = malloc(sizeof(FakefsOverlay));
  memset(mount->fsInfo, 0, sizeof(FakefsOverlay));
  FakefsOverlay *proc = (FakefsOverlay *)mount->fsInfo;

  proc->fakefs = &rootProc;
  if (!rootProc.rootFile) {
    fakefsSetupRoot(&rootProc.rootFile);
    procSetup();
  }
  rootProc.rootFile->handlers = &procRootHandlers;

  return true;
}