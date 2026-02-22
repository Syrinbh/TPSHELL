#ifndef __JOBS_H__
#define __JOBS_H__

#include <sys/types.h>

/* ── Paramètres ── */
#define MAXJOBS  10       /* Taille maximale du tableau de jobs          */
#define MAXCMD  256       /* Longueur max de la ligne de commande stockée */

/* ── États possibles d'un job ── */
typedef enum {
    UNDEF   = 0,  /* Case libre / non initialisée                        */
    RUNNING = 1,  /* En cours d'exécution (arrière-plan ou 1er plan)     */
    STOPPED = 2,  /* Suspendu (SIGTSTP)                                  */
    FG      = 3   /* En cours d'exécution au premier plan                */
} job_state;

/* ── Structure représentant un job ── */
typedef struct {
    int        jid;            /* Numéro de job (>0), 0 = case libre      */
    pid_t      pid;            /* PID du processus leader du job          */
    pid_t      pgid;           /* PGID du groupe de processus             */
    job_state  state;          /* État courant du job                     */
    char       cmd[MAXCMD];   /* Ligne de commande (pour affichage)       */
} job_t;

/* ── Tableau global des jobs ── */
extern job_t jobs[MAXJOBS];

/* ════════════════════════════════════════════════
   Fonctions d'accès au tableau des jobs
   ════════════════════════════════════════════════ */

/* Initialise toutes les cases du tableau (à appeler au démarrage) */
void init_jobs(void);

/* Retourne l'index de la première case libre, ou -1 si le tableau est plein */
int  first_free_slot(void);

/* Ajoute un nouveau job ; retourne son jid (>0) ou -1 si tableau plein.
   - pid   : PID du processus leader
   - pgid  : PGID du groupe
   - state : état initial (FG ou RUNNING)
   - cmd   : texte de la commande */
int  add_job(pid_t pid, pid_t pgid, job_state state, const char *cmd);

/* Supprime le job identifié par son pid ; retourne 0 si OK, -1 si non trouvé */
int  delete_job_by_pid(pid_t pid);

/* Supprime le job identifié par son jid ; retourne 0 si OK, -1 si non trouvé */
int  delete_job_by_jid(int jid);

/* Retourne un pointeur vers le job de premier plan (état FG), ou NULL */
job_t *get_fg_job(void);

/* Retourne un pointeur vers le job dont le pid correspond, ou NULL */
job_t *get_job_by_pid(pid_t pid);

/* Retourne un pointeur vers le job dont le jid correspond, ou NULL */
job_t *get_job_by_jid(int jid);

/* Retourne un pointeur vers le job désigné par une chaîne de type
   "%3" (jid) ou "1234" (pid) ; NULL si non trouvé ou format invalide */
job_t *get_job_by_id_str(const char *id_str);

/* Affiche la liste de tous les jobs actifs (commande intégrée "jobs") */
void  list_jobs(void);

/* Retourne une chaîne lisible de l'état d'un job */
const char *state_to_str(job_state s);

#endif /* __JOBS_H__ */
