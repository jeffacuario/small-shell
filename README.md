# small-shell
> Unix shell with built in command line. Interaction that accepts bash commands. Supports background and foreground processes, signal control, variable expansion with parent PID, and input/output redirection.

![](smallsh.gif)

## About the Project
This was my portfolio project for my Operating Systems I course. Completing this project required using the Unix process API, implementation of custom signal handlers, and I/O redirection.

The project has custom code written to handle the exit, cd, and status commands. All other commands are forked into a child process and which is replaced using the exec family of functions. Input and output redirection is handled within the smallsh program itself. There are custom signal handlers for SIGINT and SIGTSTP which allow the user terminate children processes running in the foreground and toggle the ability to run processes in the background, respectively.

## Compilation
The program was tested using the GNU99 standard when compiling. Use the following to compile the program:
```sh
gcc --std=gnu99 -o smallsh main.c
```
