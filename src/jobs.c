/*
 * Copyright (C) 2002, Simon Nieuviarts
 *
 * Ce fichier gère les jobs du shell.
 * Un job correspond à une commande lancée (foreground ou background).
 * On utilise un tableau global jobs[] pour stocker tout ça.
 * Une case est libre si jid == 0.
 */

#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tableau global des jobs */
job_t jobs[MAXJOBS];

/* retourne un jid libre (le plus petit dispo) */
static int next_jid(void)
{
    int used[MAXJOBS + 1];
    memset(used, 0, sizeof(used));

    // on marque les jid déjà utilisés
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid > 0 && jobs[i].jid <= MAXJOBS)
            used[jobs[i].jid] = 1;
    }

    // on cherche le premier libre
    for (int jid = 1; jid <= MAXJOBS; jid++) {
        if (!used[jid])
            return jid;
    }

    return -1;  // plus de place
}

/* initialise complètement le tableau des jobs */
void init_jobs(void)
{
    for (int i = 0; i < MAXJOBS; i++) {
        jobs[i].jid   = 0;
        jobs[i].pid   = 0;
        jobs[i].pgid  = 0;
        jobs[i].state = UNDEF;
        jobs[i].cmd[0] = '\0';
    }
}

/* trouve la première case libre dans le tableau */
int first_free_slot(void)
{
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid == 0)
            return i;
    }
    return -1;
}

/* chercher un job à partir du pid */
job_t *get_job_by_pid(pid_t pid)
{
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid != 0 && jobs[i].pid == pid)
            return &jobs[i];
    }
    return NULL;
}

/* chercher un job à partir du jid */
job_t *get_job_by_jid(int jid)
{
    if (jid <= 0) return NULL;

    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid == jid)
            return &jobs[i];
    }
    return NULL;
}

/* retourne le job en foreground s'il existe */
job_t *get_fg_job(void)
{
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid != 0 && jobs[i].state == FG)
            return &jobs[i];
    }
    return NULL;
}

/* permet de gérer %jid ou pid directement */
job_t *get_job_by_id_str(const char *id_str)
{
    if (!id_str || *id_str == '\0')
        return NULL;

    if (id_str[0] == '%') {
        // format %jid
        int jid = atoi(id_str + 1);
        if (jid <= 0) return NULL;
        return get_job_by_jid(jid);
    } else {
        // format pid
        pid_t pid = (pid_t) atoi(id_str);
        if (pid <= 0) return NULL;
        return get_job_by_pid(pid);
    }
}

/* ajoute un job dans le tableau */
int add_job(pid_t pid, pid_t pgid, job_state state, const char *cmd)
{
    int slot = first_free_slot();
    if (slot < 0) {
        fprintf(stderr, "add_job: plus de place\n");
        return -1;
    }

    int jid = next_jid();
    if (jid < 0) {
        fprintf(stderr, "add_job: impossible de donner un jid\n");
        return -1;
    }

    jobs[slot].jid   = jid;
    jobs[slot].pid   = pid;
    jobs[slot].pgid  = pgid;
    jobs[slot].state = state;

    // copie de la commande (avec sécurité)
    strncpy(jobs[slot].cmd, cmd ? cmd : "", MAXCMD - 1);
    jobs[slot].cmd[MAXCMD - 1] = '\0';

    return jid;
}

/* supprime un job à partir de son pid */
int delete_job_by_pid(pid_t pid)
{
    job_t *j = get_job_by_pid(pid);
    if (!j) return -1;

    j->jid   = 0;
    j->pid   = 0;
    j->pgid  = 0;
    j->state = UNDEF;
    j->cmd[0] = '\0';

    return 0;
}

/* supprime un job à partir de son jid */
int delete_job_by_jid(int jid)
{
    job_t *j = get_job_by_jid(jid);
    if (!j) return -1;

    j->jid   = 0;
    j->pid   = 0;
    j->pgid  = 0;
    j->state = UNDEF;
    j->cmd[0] = '\0';

    return 0;
}

/* convertit l'état en texte pour affichage */
const char *state_to_str(job_state s)
{
    switch (s) {
        case RUNNING: return "Running";
        case STOPPED: return "Stopped";
        case FG:      return "Foreground";
        default:      return "Undefined";
    }
}

/* affiche tous les jobs actifs */
void list_jobs(void)
{
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid != 0) {
            printf("[%d] %d %s %s\n",
                   jobs[i].jid,
                   (int) jobs[i].pid,
                   state_to_str(jobs[i].state),
                   jobs[i].cmd);
        }
    }
}