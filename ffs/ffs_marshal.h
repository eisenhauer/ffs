#include "fm.h"

typedef enum {
    FFSMarshalEnd, FFSDropField, FFSSubsampleArrayField
} MarshalType;

typedef struct field_marshal_info {
    FMTypeDesc *t;
    MarshalType type;
    int (*drop_row_func)(void *);
    int (*subsample_array_func)(void *);
}*field_marshal_info;

typedef struct marshal_info {
    int count;
    struct field_marshal_info *field_info;
} *format_marshal_info;

extern field_marshal_info
get_marshal_info(FMFormat f, FMTypeDesc *t);
