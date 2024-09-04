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

int err(char *msg) {
	while (*msg) {
		write(STDERR_FILENO, msg++, 1);
	}
	return (1);
}

void set_pipe(int fd[2], int type_of_end) {
	if (
		dup2(fd[type_of_end == STDIN_FILENO ? 0 : 1], type_of_end) == -1
		|| close(fd[0]) == -1
		|| close(fd[1]) == -1
	)
		exit(err("error: fatal\n"));
}

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

int exec_cmd(char **cmd, char **envp, int has_pipe) {
	int fd[2];
	int status = 0;
	int pid = 0;

	if (has_pipe && pipe(fd) == -1) {
		exit(err("error: fatal\n"));
	}

	if ((pid = fork()) == -1) {
		exit(err("error: fatal\n"));
	}

	if (pid == 0) {
		if (has_pipe) {
			set_pipe(fd, STDOUT_FILENO);
		}
		execve(*cmd, cmd, envp);
		err("error: cannot execute command ");
		err(*cmd);
		err("\n");
		exit(1);
	}

	if (has_pipe) {
		set_pipe(fd, STDIN_FILENO);
	}
	waitpid(pid, &status, 0);
	return WIFEXITED(status) &&  WEXITSTATUS(status);
}

int main(int argc, char **argv, char **envp) {
	int status = 0;
	int has_pipe = 0;
	int i = 1;

	if (argc < 2) {
		return (1);
	}

	while(argv[i])
	{
		char **cmd = &argv[i];

		while (argv[i] && strcmp(argv[i], ";") && strcmp(argv[i], "|"))
			i++;

		has_pipe = (argv[i] && !strcmp(argv[i], "|"));
		if (argv[i]) {
			argv[i] = 0;
			i++;
		}
		if (*cmd) {
			if (!strcmp(*cmd, "cd")) {
				status = cd(cmd);
			} else {
				status = exec_cmd(cmd, envp, has_pipe);
			}
		}
	}
	return status;
}
