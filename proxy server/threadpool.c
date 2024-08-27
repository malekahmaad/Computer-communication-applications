#include <stdio.h>
#include<stdlib.h>
#include <unistd.h>
#include "threadpool.h"

threadpool* create_threadpool(int num_threads_in_pool){
    if(num_threads_in_pool <=0 || num_threads_in_pool >MAXT_IN_POOL){
        return NULL;
    }
    threadpool* tp = (threadpool*)malloc(sizeof (threadpool));
    if(tp == NULL){
        perror("error: malloc\n");
        exit(0);
    }
    tp->num_threads = num_threads_in_pool;
    tp->qsize = 0;
    tp->qhead = NULL;
    tp->qtail = NULL;
    tp->shutdown = 0;
    tp->dont_accept = 0;
    if(pthread_mutex_init(&(tp->qlock),NULL)!=0){
        perror("error: pthread_mutex_init\n");
        exit(0);
    }
    if(pthread_cond_init(&(tp->q_empty),NULL) != 0){
        perror("error: pthread_cond_init\n");
        exit(0);
    }
    if(pthread_cond_init(&(tp->q_not_empty),NULL) != 0){
        perror("error: pthread_cond_init\n");
        exit(0);
    }
    tp->threads = (pthread_t*)malloc(sizeof (pthread_t)*(tp->num_threads));
    if(tp->threads == NULL){
        perror("error: malloc");
        free(tp);
        exit(0);
    }
    for(int i = 0 ;i < (tp->num_threads) ; i++){
        if(pthread_create((tp->threads) + i ,NULL, do_work,tp) != 0){
            perror("error: pthread_create");
            free(tp->threads);
            free(tp);
            exit(0);
        }
    }
    return tp;
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    work_t* wt = (work_t*) malloc(sizeof (work_t));
    if(wt == NULL){
        perror("error: malloc");
        free(from_me->threads);
        free(from_me);
        exit(0);
    }
    wt->arg = arg;
    wt->routine = dispatch_to_here;
    wt->next = NULL;
    if(pthread_mutex_lock(&(from_me->qlock)) != 0){
        perror("error: pthread_mutex_lock");
        free(from_me->threads);
        free(wt);
        free(from_me);
        exit(0);
    }
    if(from_me->dont_accept == 1){
        if(pthread_mutex_unlock(&(from_me->qlock)) !=0){
            perror("error: pthread_mutex_unlock");
            free(from_me->threads);
            free(wt);
            free(from_me);
            exit(0);
        }
        return;
    }
    if(from_me->qhead == NULL){
        from_me->qhead = wt;
    }else{
        if(from_me->qtail == NULL){
            from_me->qhead->next = wt;
        }else{
            from_me->qtail->next = wt;
        }
        from_me ->qtail = wt;
    }
    from_me->qsize +=1;
    pthread_cond_signal(&(from_me->q_not_empty));
    if(pthread_mutex_unlock(&(from_me->qlock)) !=0){
        perror("error: pthread_mutex_unlock");
        free(from_me->threads);
        free(wt);
        free(from_me);
        exit(0);
    }
}

void* do_work(void* p){
    threadpool* tp = (threadpool *)p;
    while(1){
        if(pthread_mutex_lock(&(tp->qlock))!=0){
            perror("error: pthread_mutex_lock");
            pthread_exit(NULL);
        }
        while(tp->qsize == 0 && tp->shutdown != 1){
            if(pthread_cond_wait(&(tp->q_not_empty),&(tp->qlock)) !=0){
                perror("error: pthread_cond_wait");
                pthread_exit(NULL);
            }
        }
        if(tp->shutdown == 1){
            if(pthread_mutex_unlock(&(tp->qlock))!=0){
                perror("error: pthread_mutex_unlock");
                pthread_exit(NULL);
            }
            pthread_exit(NULL);
        }
        work_t* wt = tp->qhead;
        tp->qhead = wt->next;
        tp->qsize -=1;
        if(tp->dont_accept == 1 && tp->qsize == 0){
            if(pthread_cond_signal(&(tp->q_empty))!=0){
                perror("error: pthread_cond_signal");
                free(wt);
                pthread_exit(NULL);
            }
        }
        if(pthread_mutex_unlock(&(tp->qlock))!=0){
            perror("error: pthread_mutex_unlock");
            free(wt);
            pthread_exit(NULL);
        }
        wt->routine(wt->arg);
        free(wt);
    }
}

void destroy_threadpool(threadpool* destroyme){
    if(pthread_mutex_lock(&(destroyme->qlock))!=0){
        perror("error: pthread_mutex_lock");
        free(destroyme->threads);
        free(destroyme);
        exit(0);
    }
    destroyme->dont_accept = 1;
    while(destroyme->qsize > 0){
        if(pthread_cond_wait(&(destroyme->q_empty), &(destroyme->qlock))!=0){
            perror("error: pthread_cond_wait");
            free(destroyme->threads);
            free(destroyme);
            exit(0);
        }
    }
    destroyme->shutdown = 1;
    if(pthread_cond_broadcast(&(destroyme->q_not_empty))!=0){
        perror("error: pthread_cond_broadcast");
        free(destroyme->threads);
        free(destroyme);
        exit(0);
    }
    if(pthread_mutex_unlock(&(destroyme->qlock))!=0){
        perror("error: pthread_mutex_unlock");
        free(destroyme->threads);
        free(destroyme);
        exit(0);
    }
    for(int i=0;i<destroyme->num_threads;i++){
        if(pthread_join(destroyme->threads[i], NULL)!=0){
            perror("error: pthread_join");
            free(destroyme->threads);
            free(destroyme);
            exit(0);
        }
    }
    free(destroyme->threads);
    free(destroyme);
}
