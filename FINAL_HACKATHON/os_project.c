
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/* =========================================================
   GLOBAL VARIABLES
   ========================================================= */

static pid_t child_pid = -1;

static volatile sig_atomic_t child_terminated = 0;
static volatile sig_atomic_t alarm_received = 0;
static volatile sig_atomic_t sigint_received = 0;
static volatile sig_atomic_t running = 1;


/* =========================================================
   CO-1: OS AS A SERVICE LAYER
   System services, process IDs and shell interaction
   ========================================================= */

static void co1_demo(void)
{
    /* CO-1: Process information system calls */
    pid_t pid = getpid();
    pid_t ppid = getppid();

    /* CO-1: Demonstrate shell interaction silently */
    (void)system(":");

    (void)pid;
    (void)ppid;
}


/* =========================================================
   CO-4: MEMORY MANAGEMENT
   Dynamic allocation and deallocation
   ========================================================= */

static void co4_demo(void)
{
    /* CO-4: Dynamically allocate memory */
    int *memory = malloc(5 * sizeof(*memory));

    if (memory == NULL)
        return;

    memory[0] = 10;
    memory[1] = 20;
    memory[2] = 30;
    memory[3] = 40;
    memory[4] = 50;

    /* CO-4: Release dynamically allocated memory */
    free(memory);
}


/* =========================================================
   CO-5: FILE SYSTEM AND FILE I/O
   open(), write(), lseek(), read(), close()
   ========================================================= */

static void co5_demo(void)
{
    const char data[] = "Linux OS Project File";
    char buffer[sizeof(data)] = {0};
    size_t remaining = sizeof(data) - 1;
    size_t written = 0;
    int fd;

    /* CO-5: Open/create a file and obtain its descriptor */
    fd = open("os_project.txt",
              O_CREAT | O_RDWR | O_TRUNC,
              0644);

    if (fd == -1)
        return;

    /* CO-5: Write data using file I/O system calls */
    while (written < remaining)
    {
        ssize_t n = write(fd, data + written,
                          remaining - written);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            close(fd);
            return;
        }

        written += (size_t)n;
    }

    /* CO-5: Move the file offset to the beginning */
    if (lseek(fd, 0, SEEK_SET) != (off_t)-1)
    {
        /* CO-5: Read data through the file descriptor */
        (void)read(fd, buffer, sizeof(buffer));
    }

    /* CO-5: Release the file descriptor */
    close(fd);
}


/* =========================================================
   CO-4 + CO-5: MEMORY-MAPPED FILE I/O
   mmap(), munmap()
   ========================================================= */

static void mmap_demo(void)
{
    int fd;
    void *mapped;

    /* CO-5: Open/create a file for mapped I/O */
    fd = open("os_project_mmap.txt",
              O_CREAT | O_RDWR | O_TRUNC,
              0644);

    if (fd == -1)
        return;

    /* CO-5: Set file size before mapping */
    if (ftruncate(fd, 100) == -1)
    {
        close(fd);
        return;
    }

    /* CO-4: Map the file into virtual memory
       CO-5: Demonstrate memory-mapped file I/O */
    mapped = mmap(NULL, 100,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, 0);

    if (mapped != MAP_FAILED)
    {
        char *data = mapped;

        data[0] = 'O';
        data[1] = 'S';

        /* CO-4 + CO-5: Remove the memory mapping */
        munmap(mapped, 100);
    }

    close(fd);
}


/* =========================================================
   CO-3: SIGNAL HANDLERS
   ========================================================= */

/* CO-3: Child receives SIGUSR1 notification */
static void child_sigusr1(int sig)
{
    const char msg[] =
        "\n[Child] SIGUSR1 received - Notification received\n";

    (void)sig;
    (void)write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}


/* CO-3: Child receives SIGUSR2 */
static void child_sigusr2(int sig)
{
    const char msg[] =
        "\n[Child] SIGUSR2 received - Performing task\n";

    (void)sig;
    (void)write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}


/* CO-3: Request graceful child termination */
static void child_sigterm(int sig)
{
    (void)sig;
    running = 0;
}


/* CO-3: SIGCONT notification */
static void child_sigcont(int sig)
{
    const char msg[] =
        "\n[Child] SIGCONT received - Child resumed\n";

    (void)sig;
    (void)write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}


/* CO-2 + CO-3: Parent is notified about child state changes */
static void parent_sigchld(int sig)
{
    (void)sig;
    child_terminated = 1;
}


/* CO-3: Timer notification */
static void parent_sigalrm(int sig)
{
    (void)sig;
    alarm_received = 1;
}


/* CO-3: Keyboard interrupt notification */
static void parent_sigint(int sig)
{
    (void)sig;
    sigint_received = 1;
}


/* =========================================================
   INSTALL A SIGNAL HANDLER
   ========================================================= */

static int install_handler(int signo, void (*handler)(int))
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    /* CO-3: POSIX signal-handler installation */
    return sigaction(signo, &action, NULL);
}


/* =========================================================
   CO-3: SEND A SIGNAL TO A PROCESS
   ========================================================= */

static int send_signal(pid_t pid, int signo)
{
    /* CO-3: kill() sends a signal to the target process */
    if (kill(pid, signo) == -1)
    {
        if (errno != ESRCH)
            perror("kill");

        return -1;
    }

    return 0;
}


/* =========================================================
   CO-2 + CO-3: CHILD PROCESS
   ========================================================= */

static void child_process(void)
{
    /* CO-3: Install handlers in the child */
    if (install_handler(SIGUSR1, child_sigusr1) == -1 ||
        install_handler(SIGUSR2, child_sigusr2) == -1 ||
        install_handler(SIGTERM, child_sigterm) == -1 ||
        install_handler(SIGCONT, child_sigcont) == -1)
    {
        _exit(EXIT_FAILURE);
    }

    /* No "Waiting for signals..." output */
    printf("\n[Child Process]\n");
    printf("Child PID  : %ld\n", (long)getpid());
    printf("Parent PID : %ld\n", (long)getppid());
    fflush(stdout);

    /* CO-2: Child remains alive until termination is requested */
    while (running)
        pause();

    printf("\n[Child] Gracefully terminated.\n");
    fflush(stdout);

    _exit(EXIT_SUCCESS);
}


/* =========================================================
   CO-2: REAP CHILD AND REPORT ITS STATUS
   ========================================================= */

static void reap_child(void)
{
    int status;
    pid_t result;

    do
    {
        result = waitpid(child_pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == child_pid)
    {
        if (WIFEXITED(status))
        {
            printf("[Parent] Child exited with status %d.\n",
                   WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("[Parent] Child terminated by signal %d.\n",
                   WTERMSIG(status));
        }
    }

    child_pid = -1;
}


/* =========================================================
   CO-2: PROCESS INFORMATION
   ========================================================= */

static void show_process_information(void)
{
    int status;
    pid_t result;

    printf("\n----------------------------------------\n");
    printf("          PROCESS INFORMATION\n");
    printf("----------------------------------------\n");
    printf("Parent PID : %ld\n", (long)getpid());
    printf("Child PID  : %ld\n", (long)child_pid);

    if (child_pid == -1)
    {
        printf("Child Status : TERMINATED\n");
    }
    else
    {
        /* CO-2: WNOHANG checks without blocking the parent */
        result = waitid(P_PID, (id_t)child_pid,
                        NULL, WEXITED | WNOHANG | WNOWAIT);

        /*
           waitid() with a NULL siginfo pointer is not portable
           to all POSIX implementations, so use kill(pid, 0)
           for a basic existence check instead.
        */
        (void)result;

        if (kill(child_pid, 0) == 0 || errno == EPERM)
        {
            printf("Child Status : RUNNING or STOPPED\n");
        }
        else if (errno == ESRCH)
        {
            printf("Child Status : TERMINATED (awaiting collection)\n");
        }
        else
        {
            printf("Child Status : UNKNOWN\n");
        }

        (void)status;
    }

    printf("----------------------------------------\n");
}


/* =========================================================
   MAIN PROGRAM
   ========================================================= */

int main(void)
{
    int choice;

    /* CO-1: OS/system-service demonstration, kept silent */
    co1_demo();

    /* CO-4: Dynamic memory demonstration, kept silent */
    co4_demo();

    /* CO-5: File I/O demonstration, kept silent */
    co5_demo();

    /* CO-4 + CO-5: Memory-mapped I/O, kept silent */
    mmap_demo();

    /*
       CO-2: Create the child process.
       CO-4: fork() also demonstrates copy-on-write behavior
       managed by the Linux kernel.
    */
    child_pid = fork();

    if (child_pid == -1)
    {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child_pid == 0)
        child_process();

    /* CO-3: Install parent signal handlers */
    if (install_handler(SIGCHLD, parent_sigchld) == -1 ||
        install_handler(SIGALRM, parent_sigalrm) == -1 ||
        install_handler(SIGINT, parent_sigint) == -1)
    {
        perror("sigaction");
        send_signal(child_pid, SIGTERM);
        reap_child();
        return EXIT_FAILURE;
    }

    /* CO-3: Prevent SIGPIPE from unexpectedly ending the parent */
    signal(SIGPIPE, SIG_IGN);

    while (1)
    {
        if (child_terminated)
        {
            child_terminated = 0;

            /* CO-2: Collect a terminated child */
            int status;
            pid_t result = waitpid(child_pid, &status, WNOHANG);

            if (result == child_pid)
            {
                if (WIFEXITED(status))
                    printf("\n[Parent] Child exited with status %d.\n",
                           WEXITSTATUS(status));
                else if (WIFSIGNALED(status))
                    printf("\n[Parent] Child terminated by signal %d.\n",
                           WTERMSIG(status));

                return EXIT_SUCCESS;
            }
        }

        if (sigint_received)
        {
            sigint_received = 0;
            printf("\n[Parent] SIGINT received.\n");

            if (child_pid > 0)
                send_signal(child_pid, SIGTERM);

            reap_child();
            return EXIT_SUCCESS;
        }

        if (alarm_received)
        {
            alarm_received = 0;
            printf("\n[Parent] SIGALRM received - Timer expired.\n");
        }

        printf("\n");
        printf("========================================\n");
        printf("   Linux Signal-Based Process Control\n");
        printf("========================================\n\n");

        printf("Parent PID : %ld\n", (long)getpid());
        printf("Child PID  : %ld\n", (long)child_pid);
        printf("Status     : Child created successfully\n\n");

        printf("----------------------------------------\n");
        printf("             SIGNAL MENU\n");
        printf("----------------------------------------\n\n");

        printf("1. SIGUSR1 - Send notification\n");
        printf("2. SIGUSR2 - Perform task\n");
        printf("3. SIGTERM - Graceful termination\n");
        printf("4. SIGSTOP - Suspend child\n");
        printf("5. SIGCONT - Resume child\n");
        printf("6. SIGKILL - Force termination\n");
        printf("7. SIGINT  - Keyboard interrupt\n");
        printf("8. SIGALRM - Timer\n");
        printf("9. Process information\n");
        printf("10. Exit\n\n");

        printf("Enter choice: ");
        fflush(stdout);

        if (scanf("%d", &choice) != 1)
        {
            int ch;

            while ((ch = getchar()) != '\n' && ch != EOF)
                ;

            printf("\nInvalid input. Please enter a number.\n");
            continue;
        }

        switch (choice)
        {
            case 1:
                /* CO-3: User-defined notification */
                send_signal(child_pid, SIGUSR1);
                break;

            case 2:
                /* CO-3: User-defined task signal */
                send_signal(child_pid, SIGUSR2);
                break;

            case 3:
                /* CO-3: Graceful termination */
                send_signal(child_pid, SIGTERM);
                reap_child();
                printf("[Parent] Child terminated gracefully.\n");
                return EXIT_SUCCESS;

            case 4:
                /* CO-3: SIGSTOP cannot be caught or ignored */
                send_signal(child_pid, SIGSTOP);
                printf("\n[Parent] SIGSTOP sent to child.\n");
                break;

            case 5:
                /* CO-3: Resume a stopped child */
                send_signal(child_pid, SIGCONT);
                printf("\n[Parent] SIGCONT sent to child.\n");
                break;

            case 6:
                /* CO-3: SIGKILL cannot be caught or ignored */
                send_signal(child_pid, SIGKILL);
                reap_child();
                printf("[Parent] Child forcefully terminated.\n");
                return EXIT_SUCCESS;

            case 7:
                /* CO-3: Simulate SIGINT in the parent */
                raise(SIGINT);
                break;

            case 8:
                /* CO-3: Set a five-second timer */
                printf("\n[Parent] Timer set for 5 seconds.\n");
                alarm(5);
                pause();
                break;

            case 9:
                /* CO-2: Display process information */
                show_process_information();
                break;

            case 10:
                /* CO-2 + CO-3: Terminate and collect child */
                send_signal(child_pid, SIGTERM);
                reap_child();
                printf("\n[Parent] Program exited successfully.\n");
                return EXIT_SUCCESS;

            default:
                printf("\nInvalid choice. Please select 1-10.\n");
        }
    }

    return EXIT_SUCCESS;
}
