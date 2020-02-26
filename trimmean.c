#include <string.h>
#include "mysql.h"

#ifndef __cplusplus
#include <stdbool.h>
#endif

#define BUFFERSIZE 1024

bool trimmean_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void trimmean_deinit(UDF_INIT *initid);
void trimmean_clear(UDF_INIT *initid, char *is_null, char *is_error);
void trimmean_reset(UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error);
void trimmean_add(UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error);
double trimmean(UDF_INIT *initid __attribute__ ((unused)), UDF_ARGS *args, char *is_null, char *is_error);


struct trimmean_data {
  unsigned long count;
  unsigned long abscount;
  unsigned long pages;
  double *values;
};


bool trimmean_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
	if (args->arg_count < 2 || args->arg_count > 3) {
		strcpy(message, "`trimmean`() requires 2 parameter: the values to trim and average, and the fractional number of data points to exclude from the calculation");
		return 1;
	}

	args->arg_type[0] = REAL_RESULT;
    args->arg_type[1] = REAL_RESULT;

    struct trimmean_data *buffer = malloc(sizeof(struct trimmean_data));
    buffer->count = 0;
    buffer->abscount = 0;
    buffer->pages = 1;
    buffer->values = NULL;

    initid->decimals = 4;
    if (args->arg_count == 3 && args->arg_type[2] == INT_RESULT
        && (*((unsigned int*)args->args[2]) <= 16)) {
        initid->decimals = *((unsigned int*)args->args[2]);
    }

	initid->maybe_null = 1; // can return null
    initid->max_length  = 32;
    initid->ptr = (char *)buffer;

	return 0;
}

void trimmean_deinit(UDF_INIT* initid) {
    struct trimmean_data *buffer = (struct trimmean_data*)initid->ptr;

    if (buffer->values != NULL) {
        free(buffer->values);
        buffer->values = NULL;
    }
    free(initid->ptr);
    initid->ptr = NULL;
}

void trimmean_clear(UDF_INIT *initid, char *is_null, char *is_error) {
    struct trimmean_data *buffer = (struct trimmean_data*)initid->ptr;
    buffer->count = 0;
    buffer->abscount = 0;
    buffer->pages = 1;
    *is_null = 0;
    *is_error = 0;

    if (buffer->values != NULL) {
        free(buffer->values);
        buffer->values = NULL;
    }

    buffer->values=(double *) malloc(BUFFERSIZE*sizeof(double));
}

void trimmean_reset(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *is_error) {
    trimmean_clear(initid, is_null, is_error);
    trimmean_add(initid, args, is_null, is_error);
}

void trimmean_add(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *is_error) {
    if (args->args[0] != NULL) {
        struct trimmean_data *buffer = (struct trimmean_data*)initid->ptr;
        if (buffer->count >= BUFFERSIZE) {
            buffer->pages++;
            buffer->count = 0;
            buffer->values = (double *) realloc(buffer->values,BUFFERSIZE*buffer->pages*sizeof(double));
        }
        buffer->values[buffer->abscount++] = *((double*)args->args[0]);
        buffer->count++;
    }
}

int compare_doubles (const void *a, const void *b) {
  const double *da = (const double *) a;
  const double *db = (const double *) b;

  return (*da > *db) - (*da < *db);
}

double trimmean(UDF_INIT *initid __attribute__ ((unused)), UDF_ARGS *args, char *is_null, char *is_error) {
    struct trimmean_data* buffer = (struct trimmean_data*)initid->ptr;

    if (buffer->abscount == 0 || *is_error != 0) {
        *is_null = 1;
        return 0.0;
    }

    *is_null = 0;
    if (buffer->abscount == 1) {
        return buffer->values[0];
    }

    qsort(buffer->values, buffer->abscount, sizeof(double), compare_doubles);

    int trim = (int)(*((double*)args->args[1]) * buffer->abscount / 2);

    double sum = 0.0;
    int i;
    for (i = trim; i < buffer->abscount-trim; i++) {
        sum += buffer->values[i];
    }

	return sum/(buffer->abscount-trim*2);
}
