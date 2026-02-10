/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "readcmd.h"
#include "csapp.h"

//function for the command "quitte" to exit the program
//If the user typed the built-in command "quit" ou "q", terminate shell 
void quitteCommande(struct cmdline *l){
	if (l->seq && l->seq[0] && l->seq[0][0] && (strcmp(l->seq[0][0], "quit") == 0 || strcmp(l->seq[0][0], "q") == 0)) {
		printf("exit prog shell\n"); 
		exit(0);
	}
}

int main()
{
	while (1) {
		struct cmdline *l;
		int i, j;
		
		printf("shell> ");
		l = readcmd();

		/* If input stream closed, normal termination */
		if (!l) {
			printf("exit\n");
			exit(0);
		}

		//Call the function to check if the user typed "quit"
		quitteCommande(l);

		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}


		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);

		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
			for (j=0; cmd[j]!=0; j++) {
				printf("%s ", cmd[j]);
			}
			printf("\n");
		}

		if (l->seq[0] != NULL) {
			pid_t pid = fork();

			if (pid == -1) {
				perror("fork");
			} else if (pid == 0) { //child process
				execvp(l->seq[0][0], l->seq[0]);
				
				/* if execvp fails */
				fprintf(stderr, "%s: command not found\n", l->seq[0][0]);
				exit(1);
			} else {
				/* father process (le shell) */
				int status;
				waitpid(pid, &status, 0); // On attend la fin de l'enfant
			}
		}
	}
}
