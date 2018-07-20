/*
 * error.h
 *
 *  Created on: 17.07.2018
 *      Author: rschoene
 */

#include "../../include/x86_energy.h"

#define X86_ENERGY_SET_ERROR(...) x86_energy_set_error_string (__FILE__, __func__, __LINE__, __VA_ARGS__)
#define X86_ENERGY_APPEND_ERROR(...) x86_energy_append_error_string (__FILE__, __func__, __LINE__, __VA_ARGS__)

void x86_energy_set_error_string( const char * error_file, const char * error_func, int error_line, const char * fmt, ... );
void x86_energy_append_error_string( const char * error_file, const char * error_func, int error_line, const char * fmt, ... );
