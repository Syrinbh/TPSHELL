/*
 * Copyright (C) 2002, Simon Nieuviarts
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "readcmd.h"
#include <errno.h>
#include "csapp.h"
#include <signal.h>
#include "jobs.h"

/* on bloque SIGCHLD pour éviter que jobs[] soit modifié en même temps */
static void block_sigchld(sigset_t *prev) {
    sigset_t mask;
    sigemptyset(&mask);&
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, prev);
}

/* on remet le masque comme avant */
static void unblock_sigchld(sigset_t *prev) {
    sigprocmask(SIG_SETMASK, prev, NULL);
}

/* handler appelé quand un fils change d'état */
void sigchld_handler(int signum) {
    (void)signum;
    int status;
    pid_t pid;

    /* on récupère tous les fils terminés ou stoppés */
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        job_t *j = get_job_by_pid(pid);
        if (!j) continue;

        if (WIFSTOPPED(status)) {
            j->state = STOPPED;
            printf("\n[%d] %d Stopped %s\n", j->jid, (int)pid, j->cmd);
            printf("shell> ");
            fflush(stdout);

        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            /* si c'était un bg on affiche Done */
            if (j->state == RUNNING) {
                printf("\n[%d] %d Done %s\n", j->jid, (int)pid, j->cmd);
                printf("shell> ");
                fflush(stdout);
            }
            delete_job_by_pid(pid);
        }
    }
}

/* dans le fils on remet les signaux normaux */
static void reset_signals_in_child(void) {
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);

    signal(SIGCHLD, SIG_DFL);
    signal(SIGINT,  SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
}

/* on attend qu'il n'y ait plus de job en foreground */
static void wait_fg_job(void) {
    while (get_fg_job() != NULL) {
        sleep(1);  // simple boucle, pas optimal mais ok
    }
}

/* quit ou q pour quitter */
static void quitteCommande(struct cmdline *l) {
    if (l->seq && l->seq[0] && l->seq[0][0] &&
        (strcmp(l->seq[0][0], "quit") == 0 ||
         strcmp(l->seq[0][0], "q")    == 0)) {
        printf("exit prog shell\n");
        exit(0);
    }
}

/* on reconstruit la commande en string pour l'affichage jobs */
static void build_cmd_str(struct cmdline *l, char *buf, int buflen) {
    buf[0] = '\0';

    for (int i = 0; l->seq[i] != NULL; i++) {
        for (int j = 0; l->seq[i][j] != NULL; j++) {
            strncat(buf, l->seq[i][j], buflen - strlen(buf) - 2);
            strncat(buf, " ", buflen - strlen(buf) - 1);
        }

        if (l->seq[i+1] != NULL)
            strncat(buf, "| ", buflen - strlen(buf) - 1);
    }
}

/* affiche tous les jobs */
static void builtin_jobs(void) {
    list_jobs();
}

/* met un job en foreground */
static void builtin_fg(const char *id_str) {
    if (!id_str) { fprintf(stderr, "fg: argument manquant\n"); return; }

    sigset_t prev;
    block_sigchld(&prev);

    job_t *j = get_job_by_id_str(id_str);
    if (!j) {
        fprintf(stderr, "fg: job introuvable : %s\n", id_str);
        unblock_sigchld(&prev);
        return;
    }

    printf("%s\n", j->cmd);
    j->state = FG;

    unblock_sigchld(&prev);

    kill(-(j->pgid), SIGCONT);
    wait_fg_job();
}

/* met un job en background */
static void builtin_bg(const char *id_str) {
    if (!id_str) { fprintf(stderr, "bg: argument manquant\n"); return; }

    sigset_t prev;
    block_sigchld(&prev);

    job_t *j = get_job_by_id_str(id_str);
    if (!j) {
        fprintf(stderr, "bg: job introuvable : %s\n", id_str);
        unblock_sigchld(&prev);
        return;
    }

    j->state = RUNNING;
    printf("[%d] %d %s\n", j->jid, (int)j->pid, j->cmd);

    unblock_sigchld(&prev);

    kill(-(j->pgid), SIGCONT);
}

/* stop un job */
static void builtin_stop(const char *id_str) {
    if (!id_str) { fprintf(stderr, "stop: argument manquant\n"); return; }

    sigset_t prev;
    block_sigchld(&prev);

    job_t *j = get_job_by_id_str(id_str);
    if (!j) {
        fprintf(stderr, "stop: job introuvable : %s\n", id_str);
        unblock_sigchld(&prev);
        return;
    }

    unblock_sigchld(&prev);
    kill(-(j->pgid), SIGTSTP);
}

/* check si c'est une commande builtin */
static int handle_builtins(struct cmdline *l) {
    if (!l->seq || !l->seq[0] || !l->seq[0][0]) return 0;

    char *cmd = l->seq[0][0];

    if (strcmp(cmd, "jobs") == 0) { builtin_jobs(); return 1; }
    if (strcmp(cmd, "fg")   == 0) { builtin_fg(l->seq[0][1]); return 1; }
    if (strcmp(cmd, "bg")   == 0) { builtin_bg(l->seq[0][1]); return 1; }
    if (strcmp(cmd, "stop") == 0) { builtin_stop(l->seq[0][1]); return 1; }

    return 0;
}

/* programme principal */
int main()
{
    init_jobs();

    /* on installe le handler SIGCHLD */
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction SIGCHLD");
        exit(1);
    }

    while (1) {
        struct cmdline *l;
        int i, j;

        printf("shell> ");
        fflush(stdout);

        l = readcmd();

        if (!l) {
            printf("exit\n");
            exit(0);
        }

        quitteCommande(l);

        if (l->err) {
            fprintf(stderr, "error: %s\n", l->err);
            continue;
        }

        /* debug affichage */
        if (l->in)  printf("in: %s\n", l->in);
        if (l->out) printf("out: %s\n", l->out);

        for (i = 0; l->seq[i] != 0; i++) {
            char **cmd = l->seq[i];
            printf("seq[%d]: ", i);
            for (j = 0; cmd[j] != 0; j++) printf("%s ", cmd[j]);
            printf("\n");
        }

        if (handle_builtins(l)) continue;

        int nb_cmd = 0;
        if (l->seq[0] == NULL) continue;
        while (l->seq[nb_cmd] != NULL) nb_cmd++;

        char cmd_str[MAXCMD];
        build_cmd_str(l, cmd_str, MAXCMD);

        /* cas simple */
        if (nb_cmd == 1) {
            sigset_t prev;
            block_sigchld(&prev);  // important avant fork

            pid_t pid = fork();

            if (pid < 0) {
                fprintf(stderr, "fork: failed\n");
                unblock_sigchld(&prev);
                continue;
            }

            if (pid == 0) {
                reset_signals_in_child();
                setpgid(0, 0);

                /* redirection entrée */
                if (l->in) {
                    int fd_in = open(l->in, O_RDONLY);
                    if (fd_in < 0) {
                        fprintf(stderr, "%s: %s\n", l->in, strerror(errno));
                        exit(1);
                    }
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                }

                /* redirection sortie */
                if (l->out) {
                    int fd_out = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out < 0) {
                        fprintf(stderr, "%s: %s\n", l->out, strerror(errno));
                        exit(1);
                    }
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }

                execvp(l->seq[0][0], l->seq[0]);

                fprintf(stderr, "%s: command not found\n", l->seq[0][0]);
                exit(127);
            }

            setpgid(pid, pid);

            if (!l->background) {
                add_job(pid, pid, FG, cmd_str);
                unblock_sigchld(&prev);
                wait_fg_job();
            } else {
                int jid = add_job(pid, pid, RUNNING, cmd_str);
                unblock_sigchld(&prev);
                printf("[%d] %d\n", jid, (int)pid);
            }
        }

        /* pipeline */
        else if (nb_cmd > 1) {
            int pipefd[nb_cmd-1][2];

            for (int i = 0; i < nb_cmd-1; i++) {
                if (pipe(pipefd[i]) < 0) {
                    fprintf(stderr, "pipe failed\n");
                    continue;
                }
            }

            sigset_t prev;
            block_sigchld(&prev);

            pid_t first_pid = -1;

            for (int i = 0; i < nb_cmd; i++) {
                pid_t pid = fork();

                if (pid == 0) {
                    reset_signals_in_child();

                    if (first_pid == -1) setpgid(0, 0);
                    else setpgid(0, first_pid);

                    if (i > 0) {
                        dup2(pipefd[i-1][0], STDIN_FILENO);
                    }

                    if (i < nb_cmd-1) {
                        dup2(pipefd[i][1], STDOUT_FILENO);
                    }

                    execvp(l->seq[i][0], l->seq[i]);
                    exit(127);
                }

                if (first_pid == -1) {
                    first_pid = pid;
                    setpgid(pid, pid);
                } else {
                    setpgid(pid, first_pid);
                }
            }

            for (int i = 0; i < nb_cmd-1; i++) {
                close(pipefd[i][0]);
                close(pipefd[i][1]);
            }

            if (!l->background) {
                add_job(first_pid, first_pid, FG, cmd_str);
                unblock_sigchld(&prev);
                wait_fg_job();
            } else {
                int jid = add_job(first_pid, first_pid, RUNNING, cmd_str);
                unblock_sigchld(&prev);
                printf("[%d] %d\n", jid, (int)first_pid);
            }
        }
    }
}