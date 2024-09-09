
 # sub_delims  = "!" | "$" | "&" | "'" | "(" | ")"
 #             | "*" | "+" | "," | ";" | "="

set sub_delims = '!' + '$' + '&' + ''' + '(' + ')' + '*' 
               + '+' + ',' + ';' + '=';

# gen_delims = ":" | "/" | "?" | "#" | "[" | "]" | "@"

set gen_delims = ':' + '/' + '?' + '#' + '[' + ']' + '@';

# reserved = gen_delims | sub_delims

let reserved = gen_delims | sub_delims;

set ALPHA = isalpha;
set DIGIT = isdigit; 

# unreserved = ALPHA | DIGIT | "-" | "." | "_" | "~"
set unreserved = ALPHA + DIGIT + '-' + '.' + '_' + '~';

# scheme = ALPHA *( ALPHA | DIGIT | "+" | "-" | "." )
set schemec = ALPHA + DIGIT + '+' + '-' + '.';
let scheme = ALPHA 0_64(schemec);
let capscheme = law_uri_capscheme $ scheme;

# dec-octet     = DIGIT                 ; 0-9
#               | %x31-39 DIGIT         ; 10-99
#               | "1" 2DIGIT            ; 100-199
#               | "2" %x30-34 DIGIT     ; 200-249
#               | "25" %x30-35          ; 250-255

set one_to_nine = DIGIT - '0';
set zero_to_four = '0' + '1' + '2' + '3' + '4';
set zero_to_five = zero_to_four + '5';

# We use charsets instead of UTF8 range for efficiency and safety.

let dec_octet = ('2' '5' zero_to_five)
              | ('2' zero_to_four DIGIT) 
              | ('1' 2_2DIGIT)
              | (one_to_nine DIGIT) 
              | DIGIT;

# IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet

let IPv4address = dec_octet '.' dec_octet '.' dec_octet '.' dec_octet;

# h16         = 1*4HEXDIG
#             ; 16 bits of an IPv6 address represented in hexadecimal
# 
# ls32        = ( h16 ":" h16 ) | IPv4address
#             ; least-significant 32 bits of an IPv6 address

set HEXDIG = isxdigit;
let COLON = ':';

let h16 = 1_4HEXDIG;
let ls32 = (h16 COLON h16) | IPv4address;

# This definition for IPv6address (RFC3986) is incorrect as it can not parse 
# addresses like "2001:db8::". 
#
# IPv6address =                                 6( h16 ":" ) ls32
#                  |                       "::" 5( h16 ":" ) ls32
#                  | [               h16 ] "::" 4( h16 ":" ) ls32
#                  | [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
#                  | [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
#                  | [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
#                  | [ *4( h16 ":" ) h16 ] "::"              ls32
#                  | [ *5( h16 ":" ) h16 ] "::"              h16
#                  | [ *6( h16 ":" ) h16 ] "::"
#
# The problem lies in the optional pairs before the "::".  Here the
# repeated pattern eats into the "::" separator, and then h16 fails at 
# at the separator's unexpected second colon.
#
#       [ *1( h16 ":" ) h16 ]   
#
#       ...needs to be 
# 
#       [ h16 *1(":" h16) ]
#  
# Both the inner and outer expressions need to be swapped in the appopriate
# patterns (4-9).

let h16_COLON = h16 COLON;
let COLON_h16 = COLON h16;
let COLON_COLON = ':' ':'; 

let IPv6address = (6_6h16_COLON ls32)
                | (COLON_COLON 5_5h16_COLON ls32)
                | (0_1(h16) COLON_COLON 4_4h16_COLON ls32)
                | (0_1(h16 0_1COLON_h16) COLON_COLON 3_3h16_COLON ls32)
                | (0_1(h16 0_2COLON_h16) COLON_COLON 2_2h16_COLON ls32)
                | (0_1(h16 0_3COLON_h16) COLON_COLON h16_COLON ls32)
                | (0_1(h16 0_4COLON_h16) COLON_COLON ls32)
                | (0_1(h16 0_5COLON_h16) COLON_COLON h16)
                | (0_1(h16 0_6COLON_h16) COLON_COLON);

###
# Here is RFC2373's (made obsolete by RFC3986) definition of IPv6 just in case 
# we run into trouble again. Unfortunately, this is a bad definition that lets 
# through a lot of improperly formed addresses although we may need to use it 
# if the newer definition proves to be too restrictive.
#
# hex4 = 1*4HEXDIG
# hexseq = hex4 *( ":" hex4)
# hexpart = hexseq | hexseq "::" [ hexseq ] | "::" [ hexseq ]
# IPv6address = hexpart [ ":" IPv4address ]
# 
# let COLON = ':';
# let COLON_COLON = ':' ':'; 
# set HEXDIG = isxdigit;
# let hex4 = 1_4HEXDIG;
# let hexseq = hex4 0_8(COLON hex4);
# let hexpart = (COLON_COLON 0_1(hexseq))
#             | (hexseq COLON_COLON 0_1(hexseq))
#             | hexseq;
# let IPv6address = hexpart 0_1(IPv4address);
###

# IP_literal = "[" ( IPv6address | IPvFuture  ) "]"
let IP_literal = '[' IPv6address ']';

# pct-encoded = "%" HEXDIG HEXDIG
let pct_encoded = '%' HEXDIG HEXDIG;

# reg_name = *( unreserved | pct_encoded | sub_delims )
let reg_name = 0_127(unreserved | pct_encoded | sub_delims);

# host = IP_literal | IPv4address | reg_name
let host = IP_literal | IPv4address | reg_name;

# port = *DIGIT
let port = 0_5DIGIT;

# authority = host [ ":" port ]
let authority = host 0_1(COLON port);

# pchar = unreserved | pct_encoded | sub_delims | ":" | "@"
#  
# segment_nz_nc = 1*( unreserved | pct_encoded | sub_delims | "@" )
#                   ; non-zero-length segment without any colon ":"
# 
# segment_nz = 1*pchar
# 
# segment = *pchar

let AT = '@';

let pchar = unreserved | pct_encoded | sub_delims | COLON | AT;

let segment_nz_nc = 1_127(unreserved | pct_encoded | sub_delims | AT);

let segment_nz = 1_127(pchar);

let segment = 0_127(pchar);

# path_noscheme = segment_nz_nc *( "/" segment )
#
# path_abempty = *( "/" segment )
#
# path_absolute = "/" [ segment_nz *( "/" segment ) ]
#
# path_rootless = segment_nz *( "/" segment )
#
# path_empty = 0<pchar>

let FSLASH = '/';

let path_noscheme = segment_nz_nc 0_127(FSLASH segment);

let path_abempty = 0_127(FSLASH segment);

let path_absolute = FSLASH 0_1(segment_nz 0_127(FSLASH segment));

let path_rootless = segment_nz 0_127(FSLASH segment);

let path_empty = 0_0(pchar);

# hier_part = "//" authority path_abempty
#             | path_absolute
#             | path_rootless
#             | path_empty
#
# query = *( pchar | "/" | "?" )
#
# fragment = *( pchar | "/" | "?" )
#
# URI = scheme ":" hier_part [ "?" query ] [ "#" fragment ]

let hier_part = (FSLASH FSLASH authority path_abempty)
              | path_absolute 
              | path_rootless
              | path_empty;

let QMARK = '?';
let HASH = '#';
let query = 0_127(pchar | FSLASH | QMARK);
let fragment = 0_127(pchar | FSLASH | QMARK);

let URI = scheme COLON hier_part 0_1(QMARK query) 0_1(HASH fragment);

# relative_part = "//" authority path_abempty
#                   | path_absolute
#                   | path_noscheme
#                   | path_empty

let relative_part = (FSLASH FSLASH authority path_abempty)
                  | path_absolute
                  | path_noscheme 
                  | path_empty;

# relative_ref  = relative_part [ "?" query ] [ "#" fragment ]
#
# URI_reference = URI | relative_ref
#
# absolute_URI = scheme ":" hier_part [ "?" query ]

let relative_ref = relative_part 0_1(QMARK query) 0_1(HASH fragment);

let URI_reference = URI | relative_ref;

let absolute_URI = scheme COLON hier_part 0_1(QMARK query);

# request_target = origin_form
#                 | absolute_form
#                 | authority_form
#                 | asterisk_form

let request_target = origin_form
                   | absolute_form
                   | authority_form
                   | asterisk_form;
