# A simple Unix Shell for COMS 352 Project 1

### Usage
```sh
$ make 
```
### Run
```sh
$ ./shell352
```
The shell program will begin and waiting for input(stdin).

**Note**

In the project1.c file. There are two addition helper method which was implemented.

```sh
static void execute_cmd(Cmd *cmd, int background)
```
This method will execute the given command.

The method takes in the Cmd pointer of the current command and a background indicator. 

If background = 1, execute it as background

If backgorund = 0, execute it as foreground

```sh
void bg_processes()
```
This method will check each background processes and update its status with a stdout if needed.

Process is done: [1] Done ls -la, [\<job num>] Done \<commandArgs>

Process is kill: Terminated sleep 100 ,Terminated \<commandArgs>

Process is exited: Exit \<exitCode> \<commandArgs>