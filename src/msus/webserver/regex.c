/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
#include "dedos.h"
#include <string.h>
#include <stdio.h>
#include <pcre.h>

#define EVIL_REGEX "^(a+)+$"
#define HTML "\
<!DOCTYPE html>\n\
<html>\n\
    <body>\n\
        <h1>Does %s match %s?</h1> <br/>\n\
        <p>%s.</p>\n\
    </body>\n\
</html>\
"

int html_len(){
    return strlen(HTML) + 200;
}

int regex_html(char *to_match, char *htmlDoc){
    const char *pcreErrorStr;
    int errOffset;
    pcre *reCompiled = pcre_compile(EVIL_REGEX, 0, &pcreErrorStr, &errOffset, NULL);
    
    pcre_extra pcreExtra;
    pcreExtra.match_limit = -1;
    pcreExtra.match_limit_recursion = -1;
    pcreExtra.flags = PCRE_EXTRA_MATCH_LIMIT | PCRE_EXTRA_MATCH_LIMIT_RECURSION;
    
    int len = strlen(to_match);
    int x[1];

    int ret = pcre_exec(reCompiled, &pcreExtra, to_match, len, 0, 0, x, 1);
    
    char resp[5];
    if (ret < 0){
        sprintf(resp, "%s", "NO");
    } else {
        sprintf(resp, "%s", "YES");
    }
    sprintf(htmlDoc, HTML, to_match, EVIL_REGEX, resp);
    pcre_free(reCompiled);
    return 0;
}


