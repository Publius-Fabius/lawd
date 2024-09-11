
# DIGIT = '0' ... '9'
set DIGIT = isdigit;

# ALPHA = 'a' ... 'z' | 'A' ... 'Z'
set ALPHA = isalpha;

# VCHAR = "any visible USASCII character"
set VCHAR = isgraph;

# SP = ' '
let SP = ' ';

# HTAB = '\t' 
let HTAB = %09;

# OWS = *( SP | HTAB )
let OWS = 0_127(SP | HTAB);

# CRLF = Carriage-Return Line-Feed
let CRLF = %0D %0A;

# tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*"
#       / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
#       / DIGIT / ALPHA
set tchar = '!' + '#' + '$' + '%' + '&' + ''' + '*' 
		  + '+' + '-' + '.' + '^' + '_' + '`' + '|' + '~'
          + DIGIT + ALPHA;

# token = 1*tchar
let token = 1_127(tchar);
 
# origin_form = absolute_path [ "?" query ]
# absolute_form = absolute_URI
# authority_form = authority
# asterisk_form = "*"

# These must be linked with URI parsers at runtime.
dec cap_absolute_URI;
dec cap_origin_URI;
dec cap_authority;

# Since asterisk is a perfectly valid authority (asterisk is a sub-delim)
# that form is left out.
#
#  request_target = origin_form
#                 | absolute_form
#                 | authority_form
#                 | asterisk_form 
let request_target = 
          (law_http_cap_origin_form $ cap_origin_URI) 
        | (law_http_cap_absolute_form $ cap_absolute_URI)
        | (law_http_cap_authority_form $ cap_authority);

# HTTP_name = "HTTP" ; HTTP
# HTTP_version = HTTP_name "/" DIGIT "." DIGIT
let HTTP_name = 'H' 'T' 'T' 'P';
let HTTP_version = HTTP_name '/' DIGIT '.' DIGIT;

# method = token
# start_line = method SP request_target SP HTTP_version CRLF
let start_line = 
    (law_http_cap_method $ token) SP 
    request_target SP 
    (law_http_cap_version $ HTTP_version) CRLF;

# field_vchar = VCHAR
# field_content  = field_vchar [ 1*( SP | HTAB ) field_vchar ]
let field_content = VCHAR 0_1(1_127(SP | HTAB) VCHAR);

# field_value = *( field_content )
let field_value = 0_127(field_content);

# field-name     = token
# header_field = field_name ":" OWS field_value OWS
let header_field = 
        (law_http_cap_field_name $ token) ':' 
        OWS (law_http_cap_field_value $ field_value) OWS;

# HTTP_message = start_line
#              *( header_field CRLF )
#              CRLF
#              [ message_body ]
let HTTP_message_head = 
        start_line
        0_127((law_http_cap_field $ header_field) CRLF)
        CRLF;

