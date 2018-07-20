/*
 * error.c
 *
 *  Created on: 17.07.2018
 *      Author: rschoene
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define ERROR_LEN 4096

static char error_string[4096]={'\0'};

static char * not_enough_space="Not enough space for writing error\n";
static char * internal_error="Error while writing error (errorception)\n";


char * x86_energy_error_string( void )
{
    return error_string;
}

static int x86_energy_handle_problematic_error_string( const int printf_return);

void x86_energy_set_error_string(const char * error_file, const char * error_func, int error_line, const char * fmt, ... )
{
    va_list valist;
    if(! x86_energy_handle_problematic_error_string(snprintf(error_string, ERROR_LEN, "Error in function %s at %s:%d: ", error_func, error_file, error_line)))
    {
        va_start(valist, fmt);
    	x86_energy_handle_problematic_error_string(vsnprintf(&error_string[strlen(error_string)], ERROR_LEN - strlen(error_string), fmt, valist));
        va_end(valist);
        int old_len = strlen(error_string);
        if(old_len + 2 < ERROR_LEN)
        {
          error_string[old_len] = '\n';
          error_string[old_len + 1] = '\0';
        }
    }
    return;
}
void x86_energy_append_error_string(const char * error_file, const char * error_func, int error_line, const char * fmt, ... )
{
    va_list valist;
    if(! x86_energy_handle_problematic_error_string(snprintf(&error_string[strlen(error_string)], ERROR_LEN - strlen(error_string), "Error in function %s at %s:%d: ", error_func, error_file, error_line)))
    {
        va_start(valist, fmt);
    	x86_energy_handle_problematic_error_string(vsnprintf(&error_string[strlen(error_string)], ERROR_LEN - strlen(error_string), fmt, valist));
        va_end(valist);
        int old_len = strlen(error_string);
        if(old_len + 2 < ERROR_LEN)
        {
          error_string[old_len] = '\n';
          error_string[old_len + 1] = '\0';
        }
    }
    return;
}
static int x86_energy_handle_problematic_error_string( const int printf_return)
{
    if ( printf_return+strlen( error_string ) >= 4096 )
    {
        sprintf(error_string,"%s",not_enough_space);
        return -1;
    }
    if ( printf_return < 0 )
    {
        sprintf(error_string,"%s",internal_error);
        return -2;
    }

    return 0;
}
