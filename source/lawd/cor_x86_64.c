/* law_cor_amd64.c - coroutines */

#include "lawd/coroutine.h"

#include <stdlib.h>

int law_cor_call_x86_64(
        struct law_cor *init_env,
        struct law_cor *cor_env,
        void *arg,
        law_cor_fun fun);

int law_cor_swap_x86_64(
        struct law_cor *src,
        struct law_cor *dest,
        const int signal);

inline struct law_cor *law_cor_create()
{
        return calloc(2, sizeof(void*));
}

inline void law_cor_destroy(struct law_cor *env)
{
        free(env);
}

int law_cor_call(
        struct law_cor *init_env,
        struct law_cor *cor_env,
        struct law_smem *cor_stk,
        law_cor_fun fun,
        void *arg)
{
        void ** cor_env_ = (void**)cor_env;
        cor_env_[0] = 
                (char*)law_smem_address(cor_stk) + 
                law_smem_length(cor_stk);
        cor_env_[1] = cor_env_[0];
        return law_cor_call_x86_64(init_env, cor_env, arg, fun);
}

int law_cor_resume(
        struct law_cor *init_env, 
        struct law_cor *cor_env,
        const int signal)
{
        return law_cor_swap_x86_64(init_env, cor_env, signal);
}

int law_cor_yield(
        struct law_cor *init_env, 
        struct law_cor *cor_env,
        const int signal)
{
        return law_cor_swap_x86_64(cor_env, init_env, signal);
}