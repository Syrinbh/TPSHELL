#ifndef __JOBS_H__
#define __JOBS_H__

#include <sys/types.h>

/* ── Quelques constantes utiles ── */
#define MAXJOBS  10       /* On limite volontairement le nombre de jobs */
#define MAXCMD  256       /* Taille max qu’on garde pour une commande   */

/* ── Les différents états possibles d’un job ── */
typedef enum {
    UNDEF   = 0,  /* Case vide ou pas encore utilisée */
    RUNNING = 1,  /* Le job tourne (en arrière-plan en général) */
    STOPPED = 2,  /* Mis en pause (genre Ctrl+Z) */
    FG      = 3   /* En train de s’exécuter au premier plan */
} job_state;

/* ── Représentation d’un job ── */
typedef struct {
    int        jid;          /* Identifiant interne du job (0 = libre) */
    pid_t      pid;          /* PID du processus principal */
    pid_t      pgid;         /* Groupe de processus associé */
    job_state  state;        /* Où en est le job actuellement */
    char       cmd[MAXCMD];  /* La commande telle qu’elle a été tapée */
} job_t;

/* ── Tableau global qui contient tous les jobs ── */
extern job_t jobs[MAXJOBS];

/* Fonctions pour manipuler les jobs */

/* À appeler au début : met toutes les cases à zéro */
void init_jobs(void);

/* Cherche une place libre dans le tableau (-1 si c’est plein) */
int  first_free_slot(void);

/* Ajoute un job dans le tableau
   → retourne son jid si ça marche, sinon -1 */
int  add_job(pid_t pid, pid_t pgid, job_state state, const char *cmd);

/* Supprime un job à partir de son pid */
int  delete_job_by_pid(pid_t pid);

/* Supprime un job à partir de son jid */
int  delete_job_by_jid(int jid);

/* Récupère le job actuellement au premier plan (ou NULL s’il n’y en a pas) */
job_t *get_fg_job(void);

/* Cherche un job avec son pid */
job_t *get_job_by_pid(pid_t pid);

/* Cherche un job avec son jid */
job_t *get_job_by_jid(int jid);

/* Permet d’accepter soit "%3" (jid), soit "1234" (pid) */
job_t *get_job_by_id_str(const char *id_str);

/* Affiche tous les jobs (comme la commande jobs du shell) */
void  list_jobs(void);

/* Convertit un état en texte lisible */
const char *state_to_str(job_state s);

#endif