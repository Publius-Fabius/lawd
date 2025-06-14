#ifndef LAWD_COROUTINE_H
#define LAWD_COROUTINE_H

#include "lawd/safemem.h"

/** Coroutine Calling Environment */
typedef struct law_cor law_cor_t;                                 

/**
 * Allocate a new coroutine calling environment. 
 * @return A newly allocated environment.
 */
extern law_cor_t *law_cor_create();

/**
 * Free the coroutine calling environment.
 * @param env The environment to free.
 */
extern void law_cor_destroy(law_cor_t *env);

/**
 * Coroutine callback.
 * @param init_env The initial context.
 * @param cor_env The coroutine context.
 * @param state The user-defined state.
 * @return A status code.
 */
typedef int (*law_cor_fun_t)(
        law_cor_t *init_env, 
        law_cor_t *cor_env,
        void *state);

/**
 * Call the function in a new coroutine environment.
 * @param init_env The caller's (initial) calling environment.
 * @param cor_env The callee's (coroutine) calling environment.
 * @param cor_stk The coroutine's stack space.
 * @param fun The coroutine's callback (entry point).
 * @param arg The callback's argument.
 * @return A status code.
 */
extern int law_cor_call(
        law_cor_t *init_env,
        law_cor_t *cor_env,
        struct law_smem *cor_stk,
        law_cor_fun_t fun,
        void *arg);

/**
 * Resume the execution of a previously suspended coroutine.
 * @param init_env The initial calling environment.
 * @param cor_env The coroutine's calling environment.
 * @return A status code.
 */
extern int law_cor_resume(
        law_cor_t *init_env, 
        law_cor_t *cor_env,
        const int signal);

/**
 * Yield a coroutine, suspending it.
 * @param init_env The initial calling environment.
 * @param cor_env The coroutine's calling environment.
 * @return A status code.
 */
extern int law_cor_yield(
        law_cor_t *init_env, 
        law_cor_t *cor_env, 
        const int signal);

#endif