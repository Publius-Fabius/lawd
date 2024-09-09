
set DIGIT = isdigit;
set ALPHA = isalpha;

# VCHAR = "any visible USASCII character"
# SP = ' '
# HTAB = '\t' 
# OWS = *( SP | HTAB )

set VCHAR = isgraph;
let SP = ' ';
let HTAB = %09;
let OWS = 0_127(SP | HTAB);

let CRLF = %0D %0A;

# tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*"
#       / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
#       / DIGIT / ALPHA

set tchar = '!' + '#' + '$' + '%' + '&' + ''' + '*' 
		  + '+' + '-' + '.' + '^' + '_' + '`' + '|' + '~'
          + DIGIT + ALPHA;

# token = 1*tchar

let token = 1_127(tchar);

# method = token
#
# let method = token;
 
# origin_form = absolute_path [ "?" query ]
# absolute_form = absolute_URI
# authority_form = authority
# asterisk_form = "*"

# These must be linked with URI parsers at runtime.
dec path_absolute;
dec absolute_URI;
dec authority;
dec query;

let origin_form = path_absolute 0_1('?' query);
let asterisk_form = '*';

#  request_target = origin_form
#                 | absolute_form
#                 | authority_form
#                 | asterisk_form

let request_target = origin_form 
                   | absolute_URI 
                   | authority 
                   | asterisk_form;

# HTTP_name = "HTTP" ; HTTP
# HTTP_version = HTTP_name "/" DIGIT "." DIGIT

let HTTP_name = 'H' 'T' 'T' 'P';
let HTTP_version = HTTP_name '/' DIGIT '.' DIGIT;

# start_line = method SP request_target SP HTTP_version CRLF
let start_line = token SP request_target SP HTTP_version CRLF;

# field_vchar = VCHAR
#
# field_content  = field_vchar [ 1*( SP | HTAB ) field_vchar ]
#
# field_value = *( field_content )

let field_content = VCHAR 0_1(1_63(SP | HTAB) VCHAR);

let field_value = 0_127(field_content);

# field-name     = token
#
# header_field = field_name ":" OWS field_value OWS

let header_field = token ':' OWS field_value OWS;

# HTTP_message = start_line
#              *( header_field CRLF )
#              CRLF
#              [ message_body ]

let HTTP_head = start_line
                0_127(header_field CRLF)
                CRLF;
