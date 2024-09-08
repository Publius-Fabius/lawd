
#include "lawd/http.h"
#include "lawd/safemem.h"
#include "pgenc/buffer.h"
#include "pgenc/stack.h"

struct law_http {
        struct law_http_cfg *cfg;               /** Configuration */
};

struct law_http law_http_create(struct law_http_cfg *cfg)
{
        struct law_http *http = malloc(sizeof(struct law_http));
        http->cfg = cfg;
}

void law_http_destroy(struct law_http *http)
{
        free(http);
}

sel_err_t law_http_entry_reqline(
        struct law_srv *server,
        int socket,
        void *state,
        struct pgc_buf *in,
        struct pgc_buf *out)
{
        static const char *CRLF = "\r\n";

        enum pgc_err err = pgc_buf_scan(in, CRLF, 2);
        switch(err) {
                case PGC_ERR_OK:
                        
                case PGC_ERR_OOB:

                default: return err;
        }
}

sel_err_t law_http_entry_launch(
        struct law_srv *server,
        int socket,
        void *state,
        struct pgc_buf *in,
        struct pgc_buf *out)
{
        static const char *CRLF = "\r\n";

        enum pgc_err err = pgc_buf_scan(in, CRLF, 2);
        switch(err) {
                case PGC_ERR_OK:

                case PGC_ERR_OOB:
                default: return err;
        }
  
}

sel_err_t law_http_accept(
        struct law_srv *srv,
        int sock,
        void *state)
{
        struct law_http *http = state;
        const size_t in_length = http->cfg->in_length;
        const size_t in_guard = http->cfg->in_guard;
        const size_t out_length = http->cfg->out_length;
        const size_t out_guard = http->cfg->out_guard;
        const size_t heap_length = http->cfg->heap_length;
        const size_t heap_guard = http->cfg->heap_guard;

        struct law_smem *in_mem = law_smem_create(in_length, in_guard);
        struct law_smem *out_mem = law_smem_create(in_length, in_guard);
        struct law_smem *heap_mem = law_smem_create(heap_length, heap_guard);

        struct pgc_stk *heap = law_smem_address(heap_mem);
        SEL_ASSERT(sizeof(struct pgc_stk) <= heap_length);
        pgc_stk_init(
                heap, 
                heap_length - sizeof(struct pgc_stk), 
                heap + 1);

        struct pgc_buf *in = pgc_stk_push(heap, sizeof(struct pgc_buf));
        struct pgc_buf *out = pgc_stk_push(heap, sizeof(struct pgc_buf));

        pgc_buf_init(in, law_smem_address(in_mem), in_length, 0);
        pgc_buf_init(out, law_smem_address(out_mem), out_length, 0);

        sel_err_t error = law_http_entry_launch(srv, sock, http, in, out);

        law_smem_destroy(heap_mem);
        law_smem_destroy(out_mem);
        law_smem_destroy(in_mem);

        return error;
}       

// struct law_http {
//         int socket;                     /** Server Socket */
//         SSL_CTX *ssl_ctx;               /** Global SSL Context */
//         size_t inlen;                   /** Per-Connection Input Buffer */
//         size_t outlen;                  /** Per-Connection Output Buffer */
//         size_t alloclen;                /** Per-Connection Heap Allotment */
//         time_t timeout;                 /** Request Timeout */
//         law_http_accept_t accept;       /** Accept Function */
//         void *state;                    /** User State */
// };

// /** HTTP/S Connection for storing socket, IO buffers, and SSL state. */
// struct law_http_conn {
//         enum law_http_connmode mode;            /** Connection Mode */
//         int socket;                             /** POSIX Socket */
//         struct pgc_buf *in;                     /** Input Buffer */
//         struct pgc_buf *out;                    /** Output Buffer */
//         struct pgc_stk *alloc;                  /** Allocator for Parser */
//         uint64_t id;                            /** Unique ID */
//         SSL *ssl;                               /** SSL State */
// };

// /** Get a status code's description. */
// char *law_http_code(const int code)
// {
//         switch(code) {

//                 /* Informational 1xx */
//                 case 100: return "Continue";
//                 case 101: return "Switching Protocols";

//                 /* Successful 2xx */
//                 case 200: return "OK";
//                 case 201: return "Created";
//                 case 202: return "Accepted";
//                 case 203: return "Non-Authoritative Information";
//                 case 204: return "No Content";
//                 case 205: return "Reset Content";

//                 /* Redirection 3xx */
//                 case 300: return "Multiple Choices";
//                 case 301: return "Moved Permanently";
//                 case 302: return "Found";
//                 case 303: return "See Other";
//                 case 304: return "Not Modified";
//                 case 305: return "Use Proxy";
//                 case 307: return "Temporary Redirect";

//                 /* Client Error 4xx */
//                 case 400: return "Bad Request";
//                 case 401: return "Unauthorized";
//                 case 402: return "Payment Required";
//                 case 403: return "Forbidden";
//                 case 404: return "Not Found";
//                 case 405: return "Method Not Allowed";
//                 case 406: return "Not Acceptable";
//                 case 407: return "Proxy Authentication Required";
//                 case 408: return "Request Timeout";
//                 case 409: return "Conflict";
//                 case 410: return "Gone";
//                 case 411: return "Length Required";
//                 case 412: return "Precondition Failed";
//                 case 413: return "Request Entity Too Large";
//                 case 414: return "Request-URI Too Long";
//                 case 415: return "Unsupported Media Type";
//                 case 416: return "Requested Range Not Satisfiable";
//                 case 417: return "Expectation Failed";

//                 /* Server Error 5xx */
//                 case 500: return "Internal Server Error";
//                 case 501: return "Not Implemented";
//                 case 502: return "Bad Gateway";
//                 case 503: return "Service Unavailable";
//                 case 504: return "Gateway Timeout";
//                 case 505: return "HTTP Version Not Supported";

//                 default: return NULL;
//         }
// }