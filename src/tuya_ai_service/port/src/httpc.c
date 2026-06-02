/**
 * @file httpc.c
 * @brief httpc module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "httpc.h"

#include <string.h>

#include "tuya_error_code.h"
#include "core_http_client.h"

#include "http_port_internal.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define DEFAULT_HTTP_PORT    (80)

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static int _http_parse_URL(char *URL, parsed_url_t *parsed_url)
{
    char *pos = URL;
    const char *r;
    char *token;
    bool_t found_portno = false;

    parsed_url->portno = DEFAULT_HTTP_PORT;

    /* Check for the scheme */
    token = strstr(pos, "://");
    if (token) {
        *token = 0;
        parsed_url->scheme = pos;
        pos = token + 3;
    } else {
        parsed_url->scheme = NULL;
    }

    parsed_url->hostname = pos;

    int tmp = 0;
    char *tmp_token = strchr(pos, '/');
    if (tmp_token) {
        *tmp_token = 0;
        tmp = 1;
    }

    /* Check for the port number */
    token = strchr(pos, ':');

    if (tmp) {
        *tmp_token = '/';
    }

    if (token) {
        *token = 0;
        /* Skip the ':' */
        token++;
        /*
         * Set r to start of port string so that we can search for
         * start of resource later.
         */
        r = token;
        parsed_url->portno = atoi(token);

        found_portno = true;
    } else {
        /*
         * No port number given. So have to search from start of
         * hostname for resource start
         */
        r = parsed_url->hostname;
    }

    /* Check for the resource */
    token = strchr(r, '/');
    if (token) {
        /*
         * Resource is found. We have to move the resource string
         * (including the NULL termination) to he right by one byte
         * to NULL terminate the hostname string. However, this can
         * be avoided if the hostname was postfixed by the port no.
         */
        if (!found_portno) {
            memmove(token + 1, token, strlen(token) + 1);
            /* NULL terminate the hostname */
            *token = 0;
            token++;
        } else {
            *(token - 1) = 0;
        }

        /* point token to the resource */
        parsed_url->resource = token;
    } else {
        parsed_url->resource = NULL;
    }

    return 0;
}

int http_parse_URL(const char *URL, char *tmp_buf, int tmp_buf_len,
                   parsed_url_t *parsed_url)
{
    if (!URL || !tmp_buf || !parsed_url) {
        PR_ERR("Cannot parse URL");
        PR_ERR("NULL pointer(s) received as argument.");
        return -1;
    }

    int min_size_req = strlen(URL) + PARSE_EXTRA_BYTES;
    if (min_size_req > tmp_buf_len) {
        PR_ERR("Cannot parse URL");
        PR_ERR("Temporary buffer size is less than required");
        return -1;
    }

    strncpy(tmp_buf, URL, tmp_buf_len);
    return _http_parse_URL(tmp_buf, parsed_url);
}