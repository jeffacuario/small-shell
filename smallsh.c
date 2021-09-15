#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>

#define DELIMS " \n" // handle space and new line

/* Global variables */
char userInput[2048];
int backgroundArray[513]; // array to keep track of child processes
int backgroundCount = 0;
int statusValue = 0;
int childStatus = 0;
bool backgroundFlag = false;
bool flagSIGTSTP = false;

/* Struct for input information */
struct input
{
    char *command;
    char *argc[513];
    char *inputFile;
    char *outputFile;
};

/* Checks the background processes to see if they are complete. Prints out the status code for completed processes */
void backgroundProcesses()
{
    for (int i = 0; i < backgroundCount; i++)
    {
        if (backgroundArray[i] != -5)
        {
            int childStatus = -5;
            pid_t childPid = waitpid(backgroundArray[i], &childStatus, WNOHANG);
            if (childPid > 0)
            {
                if (WIFEXITED(childStatus) != 0) // regular exit
                {
                    printf("background pid %d is done: exit value %d\n", backgroundArray[i], WEXITSTATUS(childStatus));
                }
                else if (WIFSIGNALED(childStatus)) // signal termination
                {
                    printf("background pid %d is done: terminated by signal %d\n", backgroundArray[i], WTERMSIG(childStatus));
                }
                fflush(stdout);
            }
        }
    }
}

/* Provides command prompt and gets user input */
void getUserInput(int pid)
{
    memset(userInput, '\0', sizeof(userInput)); // reset userInput
    printf(": ");
    fflush(stdout);
    fgets(userInput, 2048, stdin);

    // convert pid to a string
    char stringPID[10];
    sprintf(stringPID, "%d", pid);

    char buffer[2048];
    char *ptr = userInput;

    // $$ expansion
    // found this as a really good reference for expanding $$:
    // https://stackoverflow.com/questions/32413667/replace-all-occurrences-of-a-substring-in-a-string-in-c/32413923
    ptr = strstr(ptr, "$$");
    while (ptr)
    {
        strncpy(buffer, userInput, ptr - userInput); // copy userInput to buffer all the way to the $$
        buffer[ptr - userInput] = '\0';              // makes the index before $$ the end of the string 'NULL terminator'
        strcat(buffer, stringPID);                   // add the pid to the end of the buffer
        strcat(buffer, ptr + 2);                     // adding everything after the $$ to the buffer
        strcpy(userInput, buffer);                   // userInput copies buffer value
        ptr = strstr(ptr, "$$");                     // ptr now checking again for $$
    }
}

/* Kills process */
void killProcesses()
{
    for (int i = 0; i < backgroundCount; ++i)
    {
        if (backgroundArray[i] != -5)
        {
            kill(backgroundArray[i], SIGTERM);
        }
    }
}

/* Updates the status value of the child process */
void updateStatus(int childStatus)
{
    if (WIFEXITED(childStatus))
    {
        statusValue = WEXITSTATUS(childStatus);
    }
    else
    {
        statusValue = WTERMSIG(childStatus);
    }
}

/* Prints out the status value of the child process */
void printChildStatus(int childStatus)
{
    if (WIFEXITED(childStatus))
    {
        printf("exit value %d\n", WEXITSTATUS(childStatus));
    }
    else
    {
        printf("terminated by signal %d\n", WTERMSIG(childStatus));
    }
    fflush(stdout);
}

/* Returns a struct processed from user input */
struct input processInput()
{

    // create an input struct and initialize the command, arguments, input, output, and backgroundFlag
    struct input currentLine;
    currentLine.command = NULL;
    memset(currentLine.argc, '\0', sizeof(currentLine.argc));
    currentLine.inputFile = NULL;
    currentLine.outputFile = NULL;
    backgroundFlag = false;

    // handle blank lines and comments
    if (strncmp(userInput, "#", 1) == 0 || strncmp(userInput, "\n", 1) == 0)
    {
        return currentLine;
    }
    else
    {
        char *token = strtok(userInput, DELIMS);
        currentLine.command = token; // get the command attribute
        int index = 0;

        while (token != NULL)
        {
            // handle & (make sure its the last one)
            if (strcmp(token, "&") == 0)
            {
                token = strtok(NULL, DELIMS);
                if (token == NULL)
                {
                    // handle not in foreground mode
                    if (flagSIGTSTP == false)
                    {
                        backgroundFlag = true;
                    }
                    else // handle foreground mode
                    {
                        backgroundFlag = NULL;
                    }
                }
                else if (token != NULL) // handle & in the middle of a comment
                // example: echo Testing foreground-only mode (20 points for entry & exit text AND ~5 seconds between times)
                {
                    currentLine.argc[index++] = "&";
                    currentLine.argc[index++] = token;
                }
            }
            // handle "<"
            else if (strcmp(token, "<") == 0)
            {
                token = strtok(NULL, DELIMS);
                currentLine.inputFile = token;
            }
            // handle ">"
            else if (strcmp(token, ">") == 0)
            {
                token = strtok(NULL, DELIMS);
                currentLine.outputFile = token;
            }
            else // add to argc array
            {
                currentLine.argc[index++] = token;
            }
            // get next token
            token = strtok(NULL, DELIMS);
        }
        // handle built-in command "exit"
        if (strcmp(currentLine.command, "exit") == 0)
        {
            // kills all other processes before exiting
            killProcesses();
            exit(0);
        }
        // handle built-in command "cd"
        else if (strcmp(currentLine.command, "cd") == 0)
        {
            if (currentLine.argc[1])
            {
                // handle invalid directory
                if (chdir(currentLine.argc[1]) != 0)
                {
                    perror("chdir");
                    fflush(stdout);
                }
            }
            else if (currentLine.argc[1] == NULL)
            {
                // No arguments - it changes to the directory specified in the HOME environment variable
                chdir(getenv("HOME"));
            }
        }
        // handle built-in command "status"
        else if (strcmp(currentLine.command, "status") == 0)
        {
            printChildStatus(childStatus);
        }
    }
    return currentLine;
}

/* Executes the command with the optional arguments */
void executeCommand(struct input currentLine, struct sigaction SIGINT_action)
{
    // handle blank lines and comments and does not execute command if "cd" or "status"
    if (currentLine.command == NULL || strcmp(currentLine.command, "cd") == 0 || strcmp(currentLine.command, "status") == 0)
    {
        return;
    }
    else
    {
        // Based a lot of this from Exploration: Process API – Creating and Terminating Processes
        // and Exploration: Process API - Monitoring Child Processes
        pid_t spawnpid = -5;
        int sourceFD;
        int result;
        int targetFD;

        // If fork is successful, the value of spawnpid will be 0 in the child, the child's pid in the parent
        spawnpid = fork();
        switch (spawnpid)
        {
        case -1:
            // Code in this branch will be exected by the parent when fork() fails and the creation of child process fails as well
            perror("fork() failed!");
            exit(1);
            break;

        case 0: // spawnpid is 0. This means the child will execute the code in this branch
            if (backgroundFlag)
            {
                // The process can't take CTRL C
                SIGINT_action.sa_handler = SIG_IGN;
                sigaction(SIGINT, &SIGINT_action, NULL);
            }
            else if (backgroundFlag == false)
            {
                // The process can take CTRL C
                SIGINT_action.sa_handler = SIG_DFL;
                sigaction(SIGINT, &SIGINT_action, NULL);
            }

            // referenced Exploration: Processes and I/O
            // handle input redirect
            if (currentLine.inputFile)
            {
                // Open source file
                sourceFD = open(currentLine.inputFile, O_RDONLY);
                if (sourceFD == -1)
                {
                    printf("cannot open %s for input\n", currentLine.inputFile); //Output error
                    fflush(stdout);
                    exit(1);
                }

                // Redirect stdin to source file
                result = dup2(sourceFD, 0);
                if (result == -1)
                {
                    perror("source dup2()");
                    exit(2);
                }
                // Now whenever this process or one of its child processes calls exec, the file descriptor fd will be closed in that process
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
            }
            // handle output redirect
            if (currentLine.outputFile)
            {
                // Open target file
                targetFD = open(currentLine.outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (targetFD == -1)
                {
                    perror("target open()");
                    exit(1);
                }
                // Redirect stdout to target file
                result = dup2(targetFD, 1);
                if (result == -1)
                {
                    perror("target dup2()");
                    exit(2);
                }
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
            }
            // Execute command
            if (execvp(currentLine.command, currentLine.argc) != 0)
            {
                printf("%s: no such file or directory\n", currentLine.command);
                fflush(stdout);
                exit(1);
            }
            break;

        default:
            // from Exploration: Process API - Monitoring Child Processes
            if (backgroundFlag)
            {
                // add child to background array
                backgroundArray[backgroundCount++] = spawnpid;
                printf("background pid is %d\n", spawnpid);
                fflush(stdout);
                break;
            }
            else
            {
                waitpid(spawnpid, &childStatus, 0);
                updateStatus(childStatus);

                if (WIFEXITED(childStatus) == false)
                {
                    printChildStatus(childStatus);
                }
                break;
            }
        }
    }
}

// Handler for SIGTSTP (referenced Exploration: Signal Handling API)
void handle_SIGTSTP(int signo)
{
    if (flagSIGTSTP == false)
    {
        char *message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50);
        flagSIGTSTP = true;
    }
    else
    {
        char *message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
        flagSIGTSTP = false;
    }
    char *prompt = ": ";
    write(STDOUT_FILENO, prompt, 2);
    fflush(stdout);
}

int main()
{
    // based from Exploration: Signal Handling API
    // handle signals
    struct sigaction SIGINT_action = {{0}}, SIGTSTP_action = {{0}};

    // ignore SIGINT (CTRL-C)
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // handle SIGTSTP (CTRL-Z)
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    struct input currentUserInput;
    int pid = getpid();
    while (1)
    {
        getUserInput(pid);
        currentUserInput = processInput();
        executeCommand(currentUserInput, SIGINT_action);
        backgroundProcesses();
    }
    return 0;
}