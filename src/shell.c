/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>     // Pour open
#include <unistd.h>    // Pour dup2, fork, exec
#include <sys/wait.h>  // Pour waitpid
#include "readcmd.h"
#include <errno.h>
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
			//etape 5 gestion erreur 
			fprintf(stderr, "error: %s\n", l->err);
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

		//etape 6 et 7 
		int nb_cmd = 0;

		// ÉTAPE 3 & 4 : Exécution de commande simple avec redirection
        if (l->seq[0] != NULL) {
			
			//compter le nb de commandes (pour les pipes) 
			while (l->seq[nb_cmd] != NULL) {
				nb_cmd++;
			}

			if (nb_cmd == 1) {
            	pid_t pid = fork();
            	if (pid < 0) {  // Erreur fork
        			fprintf(stderr, "fork: fork failed\n");
        			continue;
    			}	
            	if (pid == 0) { // Fils
                	// Redirection d'entrée 
                	if (l->in) {
                    	int fd_in = open(l->in, O_RDONLY);
                    	if (fd_in < 0) {
                        	if (errno == EACCES) { //etape 5 gestion erreur 
								fprintf(stderr, "%s: Permission denied\n", l->in);
							} else if (errno == ENOENT) {
								fprintf(stderr, "%s: No such file or directory\n", l->in);
							} else if (errno == ENOTDIR) {
    							fprintf(stderr, "%s: Not a directory\n", l->in);
							}else {
								fprintf(stderr, "%s: %s\n", l->in, strerror(errno));
							}
							exit(1);
						}
						dup2(fd_in, STDIN_FILENO);
						close(fd_in);
					}
                
                // Redirection de sortie
                if (l->out) {
                    int fd_out = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out < 0) {
                        if (errno == EACCES) {
							fprintf(stderr, "%s: Permission denied\n", l->out);
						} else {
							fprintf(stderr, "%s: %s\n", l->out, strerror(errno));
						}
						exit(1);
					}
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
                
                execvp(l->seq[0][0], l->seq[0]);
                
                // Si execvp échouesh
				//etape 5 gestion erreur 
                if (errno == ENOENT) {
					fprintf(stderr, "%s: command not found\n", l->seq[0][0]);
				} else if (errno == EACCES) {
					fprintf(stderr, "%s: Permission denied\n", l->seq[0][0]);
				} else {
					fprintf(stderr, "%s: %s\n", l->seq[0][0], strerror(errno));
				}
				exit(127); //127 : convention "command not found"
            }
			
			// Père
			if (!l->background) {
				waitpid(pid, NULL, 0);  // Bloquant
			} else {
				printf("[%d] %d\n", (int)getpid(), (int)pid);  // Message job
			}
		}

		// Plusieurs commandes → pipes
		else if(nb_cmd > 1) {
				int pipefd[nb_cmd-1][2];  // N-1 pipes pour N commandes
				int pipe_ok = 1; 
					
				// Créer tous les pipes
				for (int i = 0; i < nb_cmd-1; i++) {
					if (pipe(pipefd[i]) < 0) {
						fprintf(stderr, "pipe failed\n");
						pipe_ok = 0;
						break;
					}
				}
					
				if (!pipe_ok) {
					// Nettoyer les pipes créés et passer à la commande suivante
					for (int i = 0; i < nb_cmd-1; i++) {
						close(pipefd[i][0]);
						close(pipefd[i][1]);
					}
					continue;
				}
				
				// Pour chaque commande
				for (int i = 0; i < nb_cmd; i++) {
					pid_t pid = fork();
					if (pid < 0) {
						fprintf(stderr, "fork failed\n");
						continue;
					}
						
					if (pid == 0) { // Fils ième commande
						// Fermer tous les pipes inutiles
						for (int j = 0; j < nb_cmd-1; j++) {
							if (j != i-1) close(pipefd[j][0]);  // pipe précédent
							if (j != i)   close(pipefd[j][1]);  // pipe suivant
						}
							
						// Redirection ENTRÉE
						if (i > 0) {
							// Cette commande lit du pipe précédent
							dup2(pipefd[i-1][0], STDIN_FILENO);
							close(pipefd[i-1][0]);
						} else if (l->in) {
							// Première commande : redirection fichier
							int fd = open(l->in, O_RDONLY);
							if (fd < 0) { 
								fprintf(stderr, "%s: %s\n", l->in, strerror(errno)); 
								exit(1); 
							}
							dup2(fd, STDIN_FILENO);
							close(fd);
						}
							
						// Redirection SORTIE
						if (i < nb_cmd-1) {
							// Cette commande écrit dans le pipe suivant
							dup2(pipefd[i][1], STDOUT_FILENO);
							close(pipefd[i][1]);
						} else if (l->out) {
							// Dernière commande : redirection fichier
							int fd = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
							if (fd < 0) { 
								fprintf(stderr, "%s: %s\n", l->out, strerror(errno));
								exit(1); 
							}
							
							dup2(fd, STDOUT_FILENO);
							close(fd);
						}
						
						// Exécuter la commande
						execvp(l->seq[i][0], l->seq[i]);
						fprintf(stderr, "%s: command not found\n", l->seq[i][0]);
						exit(127);
					}	
				}
					
				// Parent : fermer tous les pipes
				for (int i = 0; i < nb_cmd-1; i++) {
					close(pipefd[i][0]);
					close(pipefd[i][1]);
				}
				
				//etape 8 : gérer l'arrière-plan
				if (!l->background) {
					for (int i = 0; i < nb_cmd; i++) {
						wait(NULL);
					}
				} else {
					printf("Job launched\n");
				}
			}
		}		
	}
}
