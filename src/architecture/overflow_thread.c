/*
 * overflow_thread.c
 *
 *  Created on: 22.06.2018
 *      Author: rschoene
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../include/overflow_thread.h"




static struct thread_info * get_thread_info(struct ov_struct * ov,int cpu)
{
    if ( ov->thread_infos == NULL ) return NULL;
    for (int i=0;i<ov->nr_thread_infos;i++)
        if (ov->thread_infos[i]->cpu==cpu)
            return ov->thread_infos[i];
    return NULL;
}
static struct thread_info * add_thread_info(struct ov_struct * ov,int cpu)
{
    struct thread_info ** new_infos=realloc(ov->thread_infos,sizeof(struct thread_info *)*(ov->nr_thread_infos+1));
    if (new_infos==NULL)
        return NULL;
    ov->thread_infos=new_infos;
    ov->nr_thread_infos++;
    struct thread_info * info = malloc(sizeof(struct thread_info));
    if (info == NULL)
        return NULL;
    memset(info,0,sizeof(struct thread_info));
    pthread_mutex_init(&info->mutex,NULL);
    ov->thread_infos[ov->nr_thread_infos-1]=info;
    return info;
}

static int add_call(struct thread_info * info,
        double ( *read )( x86_energy_single_counter_t ),
        x86_energy_single_counter_t t)
{
    pthread_mutex_lock(&info->mutex);
    read_function_t * new_functions=realloc(info->functions,sizeof(read_function_t)*(info->nr_functions+1));
    if ( new_functions == NULL )
    {
        pthread_mutex_unlock(&info->mutex);
        return 1;
    }
    x86_energy_single_counter_t * new_t=realloc(info->t,sizeof(x86_energy_single_counter_t)*(info->nr_functions+1));
    if ( new_t == NULL )
    {
        info->functions = new_functions;
        pthread_mutex_unlock(&info->mutex);
        return 1;
    }
    info->functions = new_functions;
    info->functions[info->nr_functions]=read;
    info->t = new_t;
    info->t[info->nr_functions]=t;
    info->nr_functions++;
    pthread_mutex_unlock(&info->mutex);
    return 0;
}

static void* on_overflow(void * arg)
{
    struct thread_info * info = (struct thread_info *) arg;
    while (1)
    {
        sleep(5);
        for (int i=0;i<info->nr_functions;i++)
            info->functions[i](info->t[i]);
    }
    /* will be canceled anyway */
    return NULL;
}

int overflow_thread_create(struct ov_struct * ov, int cpu, pthread_t *thread, pthread_mutex_t *mutex,
        double ( *read )( x86_energy_single_counter_t ),
        x86_energy_single_counter_t t)
{
    struct thread_info * info = get_thread_info(ov,cpu);
    if (info == NULL)
        if (add_thread_info(ov,cpu) == NULL)
            return 1;
    info = get_thread_info(ov,cpu);
    add_call(info, read,t);
    if (info->thread == 0 )
    {
        if (pthread_create(&(info->thread),NULL,on_overflow,info) != 0)
            return 1;
    }
    *thread=info->thread;
    *mutex=info->mutex;
    return 0;
}

void overflow_thread_remove_call(struct ov_struct * ov, int cpu, double ( *read )( x86_energy_single_counter_t ),
        x86_energy_single_counter_t t)
{
    struct thread_info * info = get_thread_info(ov,cpu);
    if (info == NULL)
        return;
    pthread_mutex_lock(&info->mutex);
    size_t i;
    for (i=0; i<info->nr_functions;i++)
    {
        if ( ( info->functions[i] == read ) && ( info->t[i] == t ) )
            break;
    }
    if ( i == info->nr_functions )
        return;

    memcpy(&(info->functions[i]),&(info->functions[i+1]),sizeof(read_function_t)*(info->nr_functions-i-1));
    memcpy(&(info->t[i]),&(info->t[i+1]),sizeof(read_function_t)*(info->nr_functions-i-1));

    info->nr_functions--;
    pthread_mutex_unlock(&info->mutex);

}

int overflow_thread_killall(struct ov_struct * ov)
{
    if ( ov->thread_infos == NULL ) return 0;
    int ret=0;
    for (int i=0;i<ov->nr_thread_infos;i++)
        ret |= pthread_cancel(ov->thread_infos[i]->thread);
    return ret;
}

void overflow_freeall(struct ov_struct * ov)
{
    if ( ov->thread_infos == NULL ) return;
    for (int i=0;i<ov->nr_thread_infos;i++)
        free(ov->thread_infos[i]);
    free(ov->thread_infos);
    ov->thread_infos=NULL;
}

