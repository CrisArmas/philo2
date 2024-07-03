/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carmas <carmas@student.42lehavre.fr>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/03/18 18:06:43 by carmas            #+#    #+#             */
/*   Updated: 2024/07/03 17:33:36 by carmas           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#define MAX_PHILOSOPHERS 5

typedef struct Philosopher {
    int id;
    pthread_mutex_t *left_fork;
    pthread_mutex_t *right_fork;
    pthread_cond_t can_eat;
    int *time_to_eat;   // Pointeur vers le temps pour manger
    int *time_to_sleep; // Pointeur vers le temps pour dormir
    int time_to_die;    // Temps avant qu'un philosophe meurt de faim
    struct Philosopher *next;
    long last_meal_time; // Temps du dernier repas (en millisecondes)
    int meals_eaten;     // Nombre de repas mangés par ce philosophe
    int times_to_eat;    // Nombre de fois qu'il doit manger
    pthread_t tid;
} Philosopher;

typedef struct {
    Philosopher *philosophers;
    pthread_mutex_t *forks;
} Simulation;

// Prototypes de fonction
void *philosopher_function(void *arg);
void log_event(int philosopher_id, const char *event);
long get_current_time_ms();
void check_for_starvation(Philosopher *philosopher);

// Fonction pour obtenir l'heure actuelle en millisecondes
long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Fonction pour journaliser les événements des philosophes
void log_event(int philosopher_id, const char *event) {
    long timestamp_ms = get_current_time_ms();
    printf("%ld %d %s\n", timestamp_ms, philosopher_id, event);
}

// Fonction pour vérifier si un philosophe est mort de faim
void check_for_starvation(Philosopher *philosopher) {
    long current_time_ms = get_current_time_ms();    
    if (current_time_ms - philosopher->last_meal_time > philosopher->time_to_die) {
        printf("%ld %d died\n", current_time_ms, philosopher->id);
        exit(EXIT_FAILURE);
    }
}

// Fonction pour créer et initialiser un philosophe
Philosopher *create_philosopher(int id) {
    Philosopher *philosopher = malloc(sizeof(Philosopher));
    if (philosopher == NULL) {
        perror("Erreur lors de l'allocation du philosophe");
        exit(EXIT_FAILURE);
    }
    philosopher->id = id;
    philosopher->left_fork = malloc(sizeof(pthread_mutex_t));
    if (philosopher->left_fork == NULL) {
        perror("Erreur lors de l'allocation de la fourchette gauche");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(philosopher->left_fork, NULL);
    philosopher->right_fork = malloc(sizeof(pthread_mutex_t));
    if (philosopher->right_fork == NULL) {
        perror("Erreur lors de l'allocation de la fourchette droite");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(philosopher->right_fork, NULL);
    philosopher->time_to_eat = NULL;
    philosopher->time_to_sleep = NULL;
    philosopher->next = NULL;
    philosopher->last_meal_time = get_current_time_ms(); // Initialisation du temps du dernier repas
    philosopher->meals_eaten = 0;                       // Aucun repas mangé initialement
    philosopher->times_to_eat = -1;                     // Par défaut, aucun nombre spécifique de repas à manger
    return philosopher;
}

// Fonction pour initialiser une liste chaînée de philosophes
Philosopher *initialize_philosophers(int num_philosophers, int time_to_eat, int time_to_sleep, int time_to_die, int times_to_eat, pthread_mutex_t *forks) {
    Philosopher *head = NULL;
    Philosopher *current = NULL;
    int i = 1;

    while (i < num_philosophers) {
        Philosopher *new_philosopher = create_philosopher(i + 1);
        new_philosopher->time_to_eat = &time_to_eat;
        new_philosopher->time_to_sleep = &time_to_sleep;
        new_philosopher->time_to_die = time_to_die;
        new_philosopher->times_to_eat = times_to_eat;

        new_philosopher->left_fork = &forks[i];
        new_philosopher->right_fork = &forks[(i + 1) % num_philosophers];
        
        if (head == NULL) {
            head = new_philosopher;
            current = head;
        } else {
            current->next = new_philosopher;
            current = new_philosopher;
        }
        i++;
    }

    // Faire en sorte que la liste soit circulaire (le dernier philosophe pointe vers le premier)
    if (current != NULL) {
        current->next = head;
    }

    return head;
}

// Fonction pour démarrer la simulation avec les philosophes
void start_simulation(Simulation *simulation) {
    Philosopher *current = simulation->philosophers;

    while (current != NULL) {
        // Créer un thread pour chaque philosophe et lancer leur cycle de vie
        pthread_create(&current->tid, NULL, philosopher_function, (void *)current);
        current = current->next;

        // Attendre un court instant avant de passer au philosophe suivant
        usleep(1000000); // 100ms
    }
    
    // Attendre la fin de tous les threads
    current = simulation->philosophers;
    while (current != NULL) {
        pthread_join(current->tid, NULL);
        current = current->next;
    }
}

// Fonction de cycle de vie d'un philosophe
void *philosopher_function(void *arg) {
    Philosopher *philosopher = (Philosopher *)arg;
    int philosopher_id = philosopher->id;
    int time_to_eat = *(philosopher->time_to_eat) * 1000; 
    int time_to_sleep = *(philosopher->time_to_sleep) * 1000; 
    int times_to_eat = philosopher->times_to_eat;
    
    while (1) {
        // Penser
        log_event(philosopher_id, "pense");

        // Manger
        pthread_mutex_lock(philosopher->left_fork);
        pthread_mutex_lock(philosopher->right_fork);

        log_event(philosopher_id, "mange");
        usleep(time_to_eat); 
        philosopher->last_meal_time = get_current_time_ms(); // Met à jour le temps du dernier repas
        philosopher->meals_eaten++;                         // Incrémente le nombre de repas mangés

        pthread_mutex_unlock(philosopher->right_fork);
        pthread_mutex_unlock(philosopher->left_fork);

        // Vérifier s'il a mangé le nombre requis de fois
        if (times_to_eat > 0 && philosopher->meals_eaten >= times_to_eat) {
            log_event(philosopher_id, "a mangé suffisamment");
            break; // Sortir de la boucle si le philosophe a mangé le nombre de fois requis
        }

        // Dormir
        log_event(philosopher_id, "dort");
        usleep(time_to_sleep); 

        // Vérifier s'il est mort de faim
        check_for_starvation(philosopher);
    }

    return NULL;
}


void    cleanup_philosophers(Philosopher *head){
    Philosopher *current = head;
    do {
        pthread_mutex_destroy(current->left_fork);
        pthread_mutex_destroy(current->right_fork);
        pthread_cond_destroy(&current->can_eat);
        Philosopher *next = current->next;
        free(current);
        current = next;
    }
    while (current != head);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s number_of_philosophers time_to_die time_to_eat time_to_sleep [number_of_times_each_philosopher_must_eat]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Récupérer les arguments du programme
    int number_of_philosophers = atoi(argv[1]);
    int time_to_die = atoi(argv[2]);
    int time_to_eat = atoi(argv[3]);
    int time_to_sleep = atoi(argv[4]);
    int number_of_times_each_philosopher_must_eat;
    if (argc > 5) {
        number_of_times_each_philosopher_must_eat = atoi(argv[5]);
    } else {
        number_of_times_each_philosopher_must_eat = -1;
    }

    if (number_of_philosophers <= 0 || time_to_die <= 0 || time_to_eat <= 0 || time_to_sleep <= 0 || number_of_times_each_philosopher_must_eat < -1) {
        fprintf(stderr, "Error: invalid argument\n");
        return EXIT_FAILURE;
    }
    
    pthread_mutex_t *forks = malloc(sizeof(pthread_mutex_t) * number_of_philosophers);
    int i = 0;
    while (i < number_of_philosophers) {
        pthread_mutex_init(&forks[i], NULL);
        i++;
    }

    Simulation simulation;
    simulation.forks = forks;
    simulation.philosophers;
        
    // Initialiser les philosophes
    Philosopher *head = initialize_philosophers(number_of_philosophers, time_to_eat, time_to_sleep, time_to_die, number_of_times_each_philosopher_must_eat, forks);

    // Démarrer la simulation avec les philosophes
    start_simulation(&simulation);

    free(forks);
    //Liberation des ressources
   // cleanup_philosophers(simulation.philosophers);

    return EXIT_SUCCESS;
}
