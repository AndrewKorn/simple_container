#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <linux/unistd.h>
#include <syscall.h>

#define STACK_SIZE 5 * 1024 * 1024
#define MAX_NAME 20

static char child_stack[STACK_SIZE];
char cont_name[MAX_NAME];

char* const args[] = {
    "/bin/sh",
    NULL
};

int child_func(void* arg) {
    int* pid_p = (int*)arg;
    int pid = *pid_p;
    printf("%d\n", pid);
    
    sprintf(cont_name, "cont-%d", pid);
    sethostname(cont_name, strlen(cont_name));


    printf("child pid %d\n", getpid());

    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("mount before");
	return -1;
    }

    if (mkdir(cont_name, 0777) == -1 && errno != EEXIST) {
        perror("mkdir cont_name");
	return -1;
    }

    if (chdir(cont_name) == -1) {
    	perror("chdir");
	return -1;
    }

    const char dirs[3][4] = {"./u", "./w", "m"};
    for (int i = 0; i < 3; ++i) {
    	if (mkdir(dirs[i], 0777) == -1 && errno != EEXIST) {
            perror("mkdir");
            return -1;
        }
    }

    if (mount("overlay", "./m", "overlay", MS_MGC_VAL, "lowerdir=../alpine,upperdir=./u,workdir=./w") == -1) {
    	perror("mount overlay");
	return -1;
    }

    if (chdir("./m") == -1) {
        perror("chdir ./m");
        return -1;
    }

    const char* old = "./old";
    if (mkdir(old, 0777) == -1 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }

    if (syscall(SYS_pivot_root, ".", old) == -1) {
    	perror("pivot root");
	return -1;
    }

    if (chdir("/") == -1) {
        perror("chdir ./m");
        return -1;
    }
	
 
    if (mkdir("/proc", 0555) == -1 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }

    if (mount("proc", "/proc", "proc", 0, "") == -1) {
    	perror("mount proc");
	return -1;
    }

    execv(args[0], args);

    printf("exec failed\n");

    return 0;
}

int main() {
    int child_pid;
    int parent_pid = getpid();
    printf("parent pid %d\n", parent_pid);
    child_pid = clone(child_func, child_stack + STACK_SIZE, CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, &parent_pid);
    if (child_pid == -1) {
         perror("clone failed");
	 exit(-1);
    }
    waitpid(child_pid, NULL, 0);
    return 0;
}
