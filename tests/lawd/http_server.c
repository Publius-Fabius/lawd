
#include "lawd/http_server.h"
#include "lawd/private/http_server.h"
#include "lawd/error.h"

law_hts_buf_t *law_hts_buf_create(const size_t length);
void law_hts_buf_destroy(law_hts_buf_t *buf);

void test_buf_create_destroy()
{
        law_hts_buf_t *buf = law_hts_buf_create(4096);
        SEL_ASSERT(buf);
        SEL_ASSERT(law_smem_length(buf->mapping) == 4096);

        law_hts_buf_destroy(buf);
}

int main(int argc, char **args)
{
        SEL_INFO();
}