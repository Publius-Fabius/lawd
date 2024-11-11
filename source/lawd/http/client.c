
#include "lawd/http/client.h"

struct law_ht_creq {
        struct law_htconn conn;
        struct pgc_stk *heap;
        struct law_smem *in_mem;
        struct law_smem *out_mem;
        struct law_smem *heap_mem;  
};

