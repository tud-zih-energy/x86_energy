/*
 * error.c
 *
 *  Created on: 17.07.2018
 *      Author: rschoene
 */

#include <stdio.h>
#include <string.h>

#define ERROR_LEN 4096

static char error_string[4096]={'\0'};

static char * not_enough_space="Not enough space for writing error\n";
static char * internal_error="Error while writing error (errorception)\n";

char * x86_energy_error_string( void )
{
    return error_string;
}


void x86_energy_set_error_string( const char * in, ... )
{
    va_list valist;
    int ret = snprintf(error_string, ERROR_LEN, in, valist);
    if ( ret >= 4096 )
    {
        sprintf(error_string,"%s",not_enough_space);
    }
    if ( ret < 0 )
    {
        sprintf(error_string,"%s",internal_error);
    }
    return;
}

void x86_energy_append_error_string( const char * in, ... )
{
    va_list valist;
    int ret = snprintf(
            &error_string[ strlen( error_string ) ],
            ERROR_LEN - strlen(error_string),
            in,valist);
    if ( ret+strlen( error_string ) >= 4096 )
    {
        sprintf(error_string,"%s",not_enough_space);
    }
    if ( ret < 0 )
    {
        sprintf(error_string,"%s",internal_error);
    }
    return;
}
