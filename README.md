# Microshell

This repository contains a solution for the 42 School Exam Rank 04 question, **microshell**.

You can view the source code here: [Microshell Source Code](./microshell.c)

## Exam Requirements

### Allowed Functions:

- `malloc`, `free`, `write`, `close`, `fork`, `waitpid`, `signal`, `kill`, `exit`, `chdir`, `execve`, `dup`, `dup2`, `pipe`, `strcmp`, `strncmp`.

### Program Specifications:

Write a program that behaves like executing a shell command.

- The command line arguments passed to the program represent the command to be executed.
- The executable's path can be absolute or relative, but **do not build a path** (from `PATH` environment variable).
- Implement support for `|` (pipe) and `;` (command separator) as in Bash.
    - There will never be a `|` immediately followed or preceded by nothing or another `|` or `;`.

### Built-in `cd` Command:

- Your program must implement the built-in command `cd` with only one path argument (no `-` or no argument).
    - If `cd` has the wrong number of arguments, print `error: cd: bad arguments` to `stderr` followed by a newline (`\n`).
    - If `cd` fails, print `error: cd: cannot change directory to path_to_change` to `stderr` followed by a newline, where `path_to_change` is replaced by the argument provided to `cd`.
    - `cd` will never be immediately followed or preceded by a `|`.

### Additional Considerations:

- You don't need to handle wildcards (`*`, `~`, etc.).
- You don't need to manage environment variables like `$BLA`.
- If any system call, except `execve` and `chdir`, returns an error, immediately print `error: fatal` to `stderr` and exit the program.
- If `execve` fails, print `error: cannot execute executable_that_failed` to `stderr` followed by a newline, where `executable_that_failed` is the first argument of `execve`.
- Your program must be able to handle more than hundreds of `|` pipes, even with a limit of less than 30 open files.

### Example:

This example should work as expected:

```bash
$> ./microshell /bin/ls "|" /usr/bin/grep microshell ";" /bin/echo i love my microshell
microshell
i love my microshell
$>
```

## Microshell Code Overview

Below is the C code for the `microshell` project, with detailed comments explaining each function and its role in the program.

```c
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   microshell.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dzuiev <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/09/04 11:29:01 by dzuiev            #+#    #+#             */
/*   Updated: 2024/09/04 15:12:56 by dzuiev           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* ************************************************************************** */
/*                                                                            */
/*   err                                                                       */
/*                                                                            */
/*   Description: This function prints an error message to stderr (file        */
/*   descriptor 2). It is used whenever a system call or command fails,        */
/*   printing an error message to standard error and returning an error code.  */
/*                                                                            */
/*   Arguments:                                                                */
/*   - char *msg: A string containing the error message to be printed.         */
/*                                                                            */
/*   Returns:                                                                  */
/*   - Always returns 1, indicating failure.                                   */
/* ************************************************************************** */
int err(char *msg) {
	while (*msg) {
		write(STDERR_FILENO, msg++, 1);
	}
	return (1);
}

/* ************************************************************************** */
/*                                                                            */
/*   set_pipe                                                                  */
/*                                                                            */
/*   Description: This function sets up pipes for inter-process communication  */
/*   by redirecting the pipe's file descriptors to either the input (STDIN)    */
/*   or output (STDOUT) of the current process.                                */
/*                                                                            */
/*   Arguments:                                                                */
/*   - int fd[2]: The file descriptor array representing the pipe.             */
/*   - int type_of_end: Indicates whether the pipe should be set to stdin or   */
/*     stdout. If it's STDIN_FILENO, the process reads from the pipe; if it's  */
/*     STDOUT_FILENO, the process writes to the pipe.                          */
/*                                                                            */
/*   Returns:                                                                  */
/*   - No return value, but it exits with an error message if dup2 or close    */
/*     fails.                                                                  */
/* ************************************************************************** */
void set_pipe(int fd[2], int type_of_end) {
	if (
		dup2(fd[type_of_end == STDIN_FILENO ? 0 : 1], type_of_end) == -1
		|| close(fd[0]) == -1
		|| close(fd[1]) == -1
	)
		exit(err("error: fatal\n"));
}

/* ************************************************************************** */
/*                                                                            */
/*   cd                                                                       */
/*                                                                            */
/*   Description: This function implements the built-in `cd` command, allowing */
/*   the current working directory to be changed. The function only accepts    */
/*   one argument (the path to change to) and handles errors if the path is    */
/*   invalid or not provided.                                                  */
/*                                                                            */
/*   Arguments:                                                                */
/*   - char **cmd: The command and arguments passed to `cd`.                   */
/*                                                                            */
/*   Returns:                                                                  */
/*   - Returns 0 on success or prints an error and returns 1 on failure.       */
/* ************************************************************************** */
int cd(char **cmd) {
	if (!cmd[1] || cmd[2]) {
		return (err("error: cd: bad arguments\n"));
	}
	if (chdir(cmd[1]) == -1) {
		return (
			err("error: cd: cannot change directory to "),
			err(cmd[1]),
			err("\n")
		);
	}
	return 0;
}

/* ************************************************************************** */
/*                                                                            */
/*   exec_cmd                                                                 */
/*                                                                            */
/*   Description: This function forks a new child process to execute a command */
/*   using `execve`. It sets up pipes for communication between processes if   */
/*   needed and handles errors.                                                */
/*                                                                            */
/*   Arguments:                                                                */
/*   - char **cmd: The command and arguments to execute.                       */
/*   - char **envp: The environment variables passed to execve.                */
/*   - int has_pipe: A flag indicating whether the command has a pipe.         */
/*                                                                            */
/*   Returns:                                                                  */
/*   - Returns the exit status of the child process after execution.           */
/* ************************************************************************** */
int exec_cmd(char **cmd, char **envp, int has_pipe) {
	int fd[2];   // Pipe for inter-process communication
	int status = 0; // Status of the child process
	int pid = 0;    // Process ID

	// If the command has a pipe, create the pipe
	if (has_pipe && pipe(fd) == -1) {
		exit(err("error: fatal\n"));
	}

	// Fork a new process to execute the command
	if ((pid = fork()) == -1) {
		exit(err("error: fatal\n"));
	}

	if (pid == 0) {  // In the child process
		if (has_pipe) {
			set_pipe(fd, STDOUT_FILENO); // Set up the pipe for writing
		}
		execve(*cmd, cmd, envp);  // Execute the command
		err("error: cannot execute command "); // If execve fails
		err(*cmd);
		err("\n");
		exit(1); // Exit if execve fails
	}

	// In the parent process, set the pipe for reading if needed
	if (has_pipe) {
		set_pipe(fd, STDIN_FILENO);
	}

	waitpid(pid, &status, 0);  // Wait for the child process to finish
	return WIFEXITED(status) && WEXITSTATUS(status);
}

/* ************************************************************************** */
/*                                                                            */
/*   main                                                                     */
/*                                                                            */
/*   Description: The main function parses the command line arguments, checks  */
/*   for pipes or semicolons, and calls the `exec_cmd` or `cd` function        */
/*   accordingly. It handles command chaining and pipes between commands.      */
/*                                                                            */
/*   Arguments:                                                                */
/*   - int argc: The number of arguments passed to the program.                */
/*   - char **argv: The array of arguments (commands and arguments).           */
/*   - char **envp: The array of environment variables.                        */
/*                                                                            */
/*   Returns:                                                                  */
/*   - The status code of the last executed command or 1 if no arguments are   */
/*     passed.                                                                 */
/* ************************************************************************** */
int main(int argc, char **argv, char **envp) {
	int status = 0; // Status of the last command executed
	int has_pipe = 0; // Flag indicating if the current command has a pipe
	int i = 1; // Index to iterate over command line arguments

	// Check if there are enough arguments to process
	if (argc < 2) {
		return (1); // Return error if there are not enough arguments
	}

	// Iterate over the arguments and execute commands
	while (argv[i]) {
		char **cmd = &argv[i]; // Set cmd to the current command

		// Find the next delimiter (pipe or semicolon)
		while (argv[i] && strcmp(argv[i], ";") && strcmp(argv[i], "|"))
			i++;

		// Check if the current command is followed by a pipe
		has_pipe = (argv[i] && !strcmp(argv[i], "|"));
		if (argv[i]) {
			argv[i] = 0; // Terminate the current command string
			i++;
		}

		// Execute the command
		if (*cmd) {
			if (!strcmp(*cmd, "cd")) {
				status = cd(cmd); // Execute built-in cd
			} else {
				status = exec_cmd(cmd, envp, has_pipe); // Execute other commands
			}
		}
	}
	return status; // Return the status of the last executed command
}

```

## Hints:

- Always pass the environment variables to `execve`.
- Make sure to avoid leaking file descriptors! Properly close them after they are no longer needed to ensure efficient usage of system resources.

## How to Compile:

You can compile the microshell using the following command:

```bash
gcc -Wall -Wextra -Werror -o microshell microshell.c
```

This will generate an executable file named `microshell`.

## How to Run:

Once compiled, you can run the microshell by passing commands and arguments as parameters. For example:

```bash
./microshell /bin/ls "|" /usr/bin/grep microshell ";" /bin/echo "Done!"
```

This command sequence will:

1. List the files in the current directory using `ls`.
2. Pipe the output to `grep` to search for "microshell".
3. Once those commands are complete, it will print "Done!" using `echo`.

## Contributing:

Contributions to improve the readability, efficiency, or performance of this code are welcome, as long as they adhere to the exam's constraints and keep the functionality intact. Please open an issue or submit a pull request with your proposed changes.

## License:

This project is licensed under the MIT License.

