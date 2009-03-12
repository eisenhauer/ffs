#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "evpath.h"
#include "cm_internal.h"
#include "cod.h"
#include "libltdl/ltdl.h"

typedef enum {Response_Filter, Response_Transform, Response_Router, Response_Multityped} response_types;

struct terminal_spec {
    FMStructDescList format_list;
    void *handler;
    void *client_data;
};

struct filter_spec {
    FMStructDescList format_list;
    char *function;
    void *client_data;
    FMFormat reference_format;
};

struct transform_spec {
    FMStructDescList in_format_list;
    FMStructDescList out_format_list;
    char *function;
    void *client_data;
    FMFormat reference_input_format;
    FMFormat reference_output_format;
    EVsource source_handle;
    int output_base_struct_size;
};

struct multityped_spec {
    FMStructDescList *struct_list;
    FMStructDescList out_format_list;
    char *function;
    void *client_data;
    FMFormat *reference_input_format_list;
};

typedef struct response_spec {
    response_types response_type;
    union {
	struct terminal_spec term;
	struct filter_spec filter;
	struct transform_spec transform;
	struct multityped_spec multityped;
    }u;
} *handler_list;

struct filter_instance {
    int (*func_ptr)(void *, attr_list);
    cod_code code;
    cod_exec_context ec;
    void *client_data;
};

struct transform_instance {
    int (*func_ptr)(void *, void*, attr_list);
    cod_code code;
    cod_exec_context ec;
    int out_size;
    void *client_data;
    FMFormat out_format;
};

struct queued_instance {
    cod_code code;
    cod_exec_context ec;
    void *client_data;
    FMFormat *formats;
};

typedef struct response_instance {
    response_types response_type;
    int stone;
    int proto_action_id;
    union {
	struct filter_instance filter;
	struct transform_instance transform;
	struct queued_instance queued;
    }u;
} *response_instance;



static char *
add_FMfieldlist_to_string(char *str, FMStructDescRec *f)
{
    int index, field_count = 0;
    FMFieldList list = f->field_list;
    int len = strlen(str);
    char *tmp_str;
    len += strlen(f->format_name) + 5 + 35 + 20;
    str = realloc(str, len);
    while(list[field_count].field_name != NULL) field_count++;
    tmp_str = str + strlen(str); 
    sprintf(tmp_str, "FMFormat \"%s\" StructSize %d FieldCount %d\n", 
	    f->format_name, f->struct_size, field_count);
    for (index = 0; index < field_count; index++) {
	len += strlen(list[index].field_name) +strlen(list[index].field_type) + 50;
	str = realloc(str, len);
	tmp_str = str + strlen(str); 
	sprintf(tmp_str, "    FMField \"%s\" \"%s\" %d %d\n",
		list[index].field_name, list[index].field_type,
		list[index].field_size, list[index].field_offset);
    }
    return str;
}

/*static char *
add_IOformat_to_string(char *str, IOFormat ioformat)
{
    return add_IOfieldlist_to_string(str, name_of_IOformat(ioformat),
				     field_list_of_IOformat(ioformat));
}*/

static char *
get_str(char *str, const char **name_p)
{
    int name_len = 0;
    char *name = malloc(1);
    while (*str != '"') {
	name = realloc(name, (name_len + 2));
	name[name_len++] = *(str++);
    }
    name[name_len] = 0;
    str++;
    *name_p = name;
    return str;
}
    
static char *
parse_FMformat_from_string(char *str, FMStructDescRec *f)
{
    char *name;
    FMFieldList list;
    int struct_size;
    f->format_name = NULL;
    f->field_list = NULL;
    f->struct_size = 0;
    f->opt_info = NULL;
    if (strncmp(str, "FMFormat \"", 10) == 0) {
	int field_count;
	int index = 0;
	str += 10;
	str = get_str(str, (const char **)&name);
	str += strlen(" StructSize ");
	if (sscanf(str, "%d", &struct_size) == 1) {
	    while(isdigit((int)*str)) str++;
	}
	str += strlen(" FieldCount ");
	if (sscanf(str, "%d", &field_count) == 1) {
	    while(isdigit((int)*str)) str++;
	}
	str++;
	list = malloc(sizeof(*list) * (field_count + 1));
	for (index = 0; index < field_count; index++) {
	    str += strlen("    FMField \"");
	    str = get_str(str, &(list[index].field_name));
	    str += 2;
	    str = get_str(str, &(list[index].field_type));
	    str++;
	    if (sscanf(str, "%d", &list[index].field_size) == 1) {
		while(isdigit((int)*str)) str++;
	    }
	    str++;
	    if (sscanf(str, "%d", &list[index].field_offset) == 1) {
		while(isdigit((int)*str)) str++;
	    }
	    str = strchr(str, '\n') + 1;
	}
	list[field_count].field_name = NULL;
	list[field_count].field_type = NULL;
	list[field_count].field_size = 0;
	list[field_count].field_offset = 0;
	f->format_name = name;
	f->field_list = list;
	f->struct_size = struct_size;
    }
    return str;
}

void *
install_response_handler(CManager cm, int stone_id, char *response_spec, 
			 void *local_data, FMFormat **ref_ptr)
{
    char *str = response_spec;
    if (strncmp("Terminal Action", str, strlen("Terminal Action")) == 0) {
	int format_count, i;
	FMStructDescList list;
	str += strlen("Terminal Action") + 1;
	sscanf(str, "  Format Count %d\n", &format_count);
	str = strchr(str, '\n') + 1;
	list = malloc(sizeof(list[0]) * (format_count + 1));
	for (i=0; i < format_count; i++) {
	    str = parse_FMformat_from_string(str, &list[i]);
	}
	list[format_count].format_name = NULL;
/*	INT_EVassoc_terminal_action(cm, stone_id, list, local_data, NULL);*/
    }
    if (strncmp("Filter Action", str, strlen("Filter Action")) == 0) {
	struct response_spec *response = malloc(sizeof(struct response_spec));
	int format_count, i;
	char *function;
	FMStructDescList list;
	str += strlen("Filter Action") + 1;
	sscanf(str, "  Format Count %d\n", &format_count);
	str = strchr(str, '\n') + 1;
	list = malloc(sizeof(list[0]) * (format_count + 1));
	for (i=0; i < format_count; i++) {
	    str = parse_FMformat_from_string(str, &list[i]);
	}
	list[format_count].format_name = NULL;
	function = malloc(strlen(str) + 1);
	strcpy(function, str);
	response->response_type = Response_Filter;
	response->u.filter.format_list = list;
	response->u.filter.function = function;
	response->u.filter.client_data = local_data;
	response->u.filter.reference_format = 
	    EVregister_format_set(cm, list);
	if (ref_ptr) {
	    FMFormat *formats = malloc(2*sizeof(FMFormat));
	    formats[1] = NULL;
	    formats[0] = response->u.filter.reference_format;

	    *ref_ptr = formats;
	}
	return (void*)response;
    }
    if (strncmp("Router Action", str, strlen("Router Action")) == 0) {
	struct response_spec *response = malloc(sizeof(struct response_spec));
	int format_count, i;
	char *function;
	FMStructDescList list;
	str += strlen("Router Action") + 1;
	sscanf(str, "  Format Count %d\n", &format_count);
	str = strchr(str, '\n') + 1;
	list = malloc(sizeof(list[0]) * (format_count + 1));
	for (i=0; i < format_count; i++) {
	    str = parse_FMformat_from_string(str, &list[i]);
	}
	list[format_count].format_name = NULL;
	function = malloc(strlen(str) + 1);
	strcpy(function, str);
	response->response_type = Response_Router;
	response->u.filter.format_list = list;
	response->u.filter.function = function;
	response->u.filter.client_data = local_data;
	response->u.filter.reference_format = 
	    EVregister_format_set(cm, list);
	if (ref_ptr) {
	    FMFormat *formats = malloc(2*sizeof(FMFormat));
	    formats[1] = NULL;
	    formats[0] = response->u.filter.reference_format;

	    *ref_ptr = formats;
	}
	return (void*)response;
    }
    if (strncmp("Transform Action", str, strlen("Transform Action")) == 0) {
	struct response_spec *response = malloc(sizeof(struct response_spec));
	int format_count, i;
	char *function;
	FMStructDescList in_list, out_list;
	str += strlen("Transform Action") + 1;
	sscanf(str, "  Input Format Count %d\n", &format_count);
	str = strchr(str, '\n') + 1;
	in_list = malloc(sizeof(in_list[0]) * (format_count + 1));
	for (i=0; i < format_count; i++) {
	    str = parse_FMformat_from_string(str, &in_list[i]);
	}
	in_list[format_count].format_name = NULL;
	in_list[format_count].field_list = NULL;
	if (sscanf(str, "  Output Format Count %d\n", &format_count) != 1) {
	    printf("output format parse failed\n");
	    return 0;
	}
	str = strchr(str, '\n') + 1;
	out_list = malloc(sizeof(out_list[0]) * (format_count + 1));
	for (i=0; i < format_count; i++) {
	    str = parse_FMformat_from_string(str, &out_list[i]);
	}
	out_list[format_count].format_name = NULL;
	out_list[format_count].field_list = NULL;
	function = malloc(strlen(str) + 1);
	strcpy(function, str);
	response->response_type = Response_Transform;
	response->u.transform.in_format_list = in_list;
	response->u.transform.out_format_list = out_list;
	response->u.transform.function = function;
	response->u.transform.client_data = local_data;
	response->u.transform.reference_input_format = NULL;
	if (in_list[0].format_name != NULL) 
	    response->u.transform.reference_input_format = 
		EVregister_format_set(cm, in_list);
	if (ref_ptr) {
	    FMFormat *formats = malloc(2*sizeof(FMFormat));
	    formats[1] = NULL;
	    formats[0] = response->u.transform.reference_input_format;
	    *ref_ptr = formats;
	}
	if (out_list[0].format_name != NULL)
	    response->u.transform.reference_output_format = 
		EVregister_format_set(cm, out_list);
	response->u.transform.output_base_struct_size = out_list[0].struct_size;
	return (void*)response;
    }
    if (strncmp("Multityped Action", str, strlen("Multityped Action")) == 0) {
	struct response_spec *response = malloc(sizeof(struct response_spec));
	int list_count, j, format_count, i;
	char *function;
	FMStructDescList *struct_list, out_list = NULL;
        
	str += strlen("Multityped Action") + 1;
	sscanf(str, "  List Count %d\n", &list_count);
	str = strchr(str, '\n') + 1;
	struct_list = malloc(sizeof(struct_list[0]) * (list_count + 1));
	for (j = 0; j < list_count; j++) {
	    int format_count, i;
	    FMStructDescList in_list;
	    sscanf(str, "Next format   Subformat Count %d\n", &format_count);
	    str = strchr(str, '\n') + 1;

	    in_list = malloc(sizeof(in_list[0]) * (format_count + 1));
	    for (i=0; i < format_count; i++) {
		str = parse_FMformat_from_string(str, &in_list[i]);
	    }
	    in_list[format_count].format_name = NULL;
	    in_list[format_count].field_list = NULL;
	    struct_list[j] = in_list;
	}
	if (sscanf(str, "  Output Format Count %d\n", &format_count) != 1) {
	    printf("output format parse failed\n");
	    return 0;
	}
	str = strchr(str, '\n') + 1;
	out_list = malloc(sizeof(out_list[0]) * (format_count + 1));
	for (i=0; i < format_count; i++) {
	    str = parse_FMformat_from_string(str, &out_list[i]);
	}
	out_list[format_count].format_name = NULL;
	out_list[format_count].field_list = NULL;
	function = malloc(strlen(str) + 1);
	strcpy(function, str);
	response->response_type = Response_Multityped;
	response->u.multityped.struct_list = struct_list;
	response->u.multityped.out_format_list = out_list;
	response->u.multityped.function = function;
	response->u.multityped.client_data = local_data;
	response->u.multityped.reference_input_format_list = 
	    malloc((list_count +1) * sizeof(FMFormat));
	for (j = 0; j < list_count; j++) {
	    if ((struct_list[j])[0].format_name != NULL) 
		response->u.multityped.reference_input_format_list[j] = 
		    EVregister_format_set(cm, struct_list[j]);
	}
	if (ref_ptr) {
	    FMFormat *formats = malloc((list_count + 1)*sizeof(FMFormat));
	    int i = 0;
	    for (i=0; i < list_count; i++) {
		formats[i] = response->u.multityped.reference_input_format_list[i];
	    }
	    formats[list_count] = NULL;
	    *ref_ptr = formats;
	}
	return (void*)response;
    }
    printf("Unparsed action : %s\n", str);
    return NULL;
}


char *
create_terminal_action_spec(FMStructDescList format_list)
{
    int format_count = 0;
    int i;
    char *str;
    while(format_list[format_count].format_name != NULL) format_count++;
    str = malloc(50);
    sprintf(str, "Terminal Action   Format Count %d\n", format_count);

    for (i = 0 ; i < format_count; i++) {
	str = add_FMfieldlist_to_string(str, &format_list[i]);
    }
    return str;
}

char *
INT_create_filter_action_spec(FMStructDescList format_list, char *function)
{
    int format_count = 0;
    int i;
    char *str;
    while(format_list[format_count].format_name != NULL) format_count++;
    str = malloc(50);
    sprintf(str, "Filter Action   Format Count %d\n", format_count);

    for (i = 0 ; i < format_count; i++) {
	str = add_FMfieldlist_to_string(str, &format_list[i]);
    }
    str = realloc(str, strlen(str) + strlen(function) + 1);
    strcpy(&str[strlen(str)], function);
    return str;
}

char *
INT_create_router_action_spec(FMStructDescList format_list, char *function)
{
    int format_count = 0;
    int i;
    char *str;
    while(format_list[format_count].format_name != NULL) format_count++;
    str = malloc(50);
    sprintf(str, "Router Action   Format Count %d\n", format_count);

    for (i = 0 ; i < format_count; i++) {
	str = add_FMfieldlist_to_string(str, &format_list[i]);
    }
    str = realloc(str, strlen(str) + strlen(function) + 1);
    strcpy(&str[strlen(str)], function);
    return str;
}

char *
INT_create_transform_action_spec(FMStructDescList format_list, FMStructDescList out_format_list, char *function)
{
    int format_count = 0;
    int i;
    char *str;
    while(format_list && format_list[format_count].format_name != NULL) 
	format_count++;
    str = malloc(50);
    sprintf(str, "Transform Action   Input Format Count %d\n", format_count);

    for (i = 0 ; i < format_count; i++) {
	str = add_FMfieldlist_to_string(str, &format_list[i]);
    }

    format_count = 0;
    while(out_format_list[format_count].format_name != NULL) format_count++;
    str = realloc(str, strlen(str) + 30);
    sprintf(str + strlen(str), "  Output Format Count %d\n", format_count);

    for (i = 0 ; i < format_count; i++) {
	str = add_FMfieldlist_to_string(str, &out_format_list[i]);
    }
    str = realloc(str, strlen(str) + strlen(function) + 1);
    strcpy(&str[strlen(str)], function);
    return str;
}

extern char *
INT_create_multityped_action_spec(FMStructDescList *input_format_lists, FMStructDescList output_format_list, char *function)
{
    int list_count = 0;
    int out_format_count = 0;
    int l, i;
    char *str;
    while(input_format_lists && input_format_lists[list_count] != NULL) 
	list_count++;

    str = malloc(50);
    sprintf(str, "Multityped Action   List Count %d\n", list_count);

    for (l = 0; l < list_count; l++) {
	int format_count = 0, i;
	FMStructDescList format_list = input_format_lists[l];
	while(format_list && format_list[format_count].format_name != NULL) 
	    format_count++;
	str = realloc(str, strlen(str) + 50);
	sprintf(str + strlen(str), "Next format   Subformat Count %d\n",
		format_count);
	for (i = 0 ; i < format_count; i++) {
	    str = add_FMfieldlist_to_string(str, &format_list[i]);
	}
    }

    while(output_format_list && output_format_list[out_format_count].format_name != NULL) 
	out_format_count++;
    str = realloc(str, strlen(str) + 50);
    sprintf(str + strlen(str), "  Output Format Count %d\n",
	    out_format_count);
    for (i = 0 ; i < out_format_count; i++) {
	str = add_FMfieldlist_to_string(str, &output_format_list[i]);
    }
    str = realloc(str, strlen(str) + strlen(function) + 1);
    strcpy(&str[strlen(str)], function);
    return str;
}

struct ev_state_data {
    CManager cm;
    struct _event_item *cur_event;
    int stone;
    int proto_action_id;
    int out_count;
    int *out_stones;
    queue_item *item;
    struct _queue *queue;
    response_instance instance;
    int did_output;
};

static int
filter_wrapper(CManager cm, struct _event_item *event, void *client_data,
	       attr_list attrs, int out_count, int *out_stones)
{
    response_instance instance = (response_instance)client_data;
    int ret;
    cod_exec_context ec = instance->u.filter.ec;
    struct ev_state_data ev_state;

    ev_state.cm = cm;
    ev_state.cur_event = event;
    ev_state.out_count = out_count;
    ev_state.out_stones = out_stones;
    if (ec != NULL) {
	cod_assoc_client_data(ec, 0x34567890, (long)&ev_state);

	ret = ((int(*)(cod_exec_context, void *, attr_list))instance->u.filter.code->func)(ec, event->decoded_event, attrs);
    } else {
	/* DLL-based handler */
	ret = ((int(*)(void *, attr_list))instance->u.filter.func_ptr)(event->decoded_event, attrs);
    }
    if (ret) {
	CMtrace_out(cm, EVerbose, "Filter function returned %d, submitting further to stone %d\n", ret, out_stones[0]);
	internal_path_submit(cm, out_stones[0], event);
    } else {
	CMtrace_out(cm, EVerbose, "Filter function returned %d, NOT submitting\n", ret);
    }
    return ret;
}
static int
router_wrapper(CManager cm, struct _event_item *event, void *client_data,
	       attr_list attrs, int out_count, int *out_stones)
{
    response_instance instance = (response_instance)client_data;
    int ret;
    if (instance->u.filter.func_ptr) {
	ret = ((int(*)(void *, attr_list))instance->u.filter.func_ptr)(event->decoded_event, attrs);
    } else {
	int (*func)(cod_exec_context, void *, attr_list) = 
	    (int(*)(cod_exec_context, void *, attr_list))instance->u.filter.code->func;
	cod_exec_context ec = instance->u.filter.ec;
	struct ev_state_data ev_state;

	ev_state.cm = cm;
	ev_state.cur_event = event;
	ev_state.out_count = out_count;
	ev_state.out_stones = out_stones;
	cod_assoc_client_data(ec, 0x34567890, (long)&ev_state);
	ret = (func)(ec, event->decoded_event, attrs);
    }
    if (ret >= 0) {
	if (ret >= out_count) {
	    CMtrace_out(cm, EVerbose, "Router function returned %d, larger than the number of associated outputs\n", ret);
	} else if (out_stones[ret] == -1) {
	    CMtrace_out(cm, EVerbose, "Router function returned %d, which has not been set with EVaction_set_output()\n", ret);
	} else {
	    CMtrace_out(cm, EVerbose, "Router function returned %d, submitting further to stone %d\n", ret, out_stones[ret]);
	    internal_path_submit(cm, out_stones[ret], event);
	}
    } else {
	CMtrace_out(cm, EVerbose, "Router function returned %d, NOT submitting\n", ret);
    }
    return ret;
}

static void
transform_free_wrapper(void *data, void *free_data)
{
    response_instance instance = (response_instance)free_data;
    FMfree_var_rec_elements(instance->u.transform.out_format,
			    data);
}

static int
transform_wrapper(CManager cm, struct _event_item *event, void *client_data,
		  attr_list attrs, int out_count, int *out_stones)
{
    response_instance instance = (response_instance)client_data;
    int ret;
    void *out_event = malloc(instance->u.transform.out_size);
    int(*func)(cod_exec_context, void *, void*, attr_list) = NULL;
    cod_exec_context ec = instance->u.transform.ec;
    struct ev_state_data ev_state;

    ev_state.cm = cm;
    ev_state.cur_event = event;
    ev_state.stone = instance->stone;
    ev_state.proto_action_id = instance->proto_action_id;
    ev_state.out_count = out_count;
    ev_state.out_stones = out_stones;

    if (CMtrace_on(cm, EVerbose)) {
	printf("Input Transform Event is :\n");
	if (event->reference_format) {
	    FMdump_data(event->reference_format, event->decoded_event, 10240);
	} else {
	    printf("       ****  UNFORMATTED  ****\n");
	}
    }
    memset(out_event, 0, instance->u.transform.out_size);
    if (ec != NULL) {
	func = (int(*)(cod_exec_context, void *, void*, attr_list))instance->u.transform.code->func;
	cod_assoc_client_data(ec, 0x34567890, (long)&ev_state);
	ret = func(ec, event->decoded_event, out_event, attrs);
    } else {
	/* DLL-based handler */
	ret = ((int(*)(void *, void *, attr_list))instance->u.transform.func_ptr)(event->decoded_event, out_event, attrs);
    }

    if (ret) {
	struct _EVSource s;
	if (CMtrace_on(cm, EVerbose)) {
	    FMFormat f = instance->u.transform.out_format;
	    printf(" Transform function returned %d, submitting further\n", ret);
	    FMdump_data(f, out_event, 10240);
	}
	s.local_stone_id = out_stones[0];
	s.cm = cm;
	s.format = NULL;
	s.reference_format = instance->u.transform.out_format;
	s.free_func = transform_free_wrapper;
	s.free_data = instance;
	s.preencoded = 0;
	INT_EVsubmit(&s, out_event, NULL);
    } else {
	CMtrace_out(cm, EVerbose, "Filter function returned %d, NOT submitting\n", ret);
	transform_free_wrapper(out_event, instance);
    }
    return ret;
}

/* {{{ cod_find_index */
static queue_item *queue_find_index(queue_item *item, int i, FMFormat format) {
    for (;;) {
        if (!item) {
            return NULL;
        }
        if (!format || item->item->reference_format == format) {
            if (i == 0)
                return item;
            --i;
        }
        item = item->next;
    }
}

static queue_item *cod_find_index_rel(struct ev_state_data *ev_state, int queue, int index)
{
    return queue_find_index(
        ev_state->queue->queue_head, index, 
        queue < 0 ?  NULL : ev_state->instance->u.queued.formats[queue]);
}

static queue_item *cod_find_index_abs(struct ev_state_data *ev_state, int queue, int index) {
    queue_item *ret;
    ret = queue_find_index(ev_state->queue->queue_head, index, NULL);
    if (!ret)
        return NULL;
    if (queue < 0 || ret->item->reference_format == 
            ev_state->instance->u.queued.formats[queue])
        return ret;
    else
        return NULL;
}

static queue_item *cod_find_index(int absp, struct ev_state_data *ev_state, int queue, int index) {
    if (absp)
        return cod_find_index_abs(ev_state, queue, index);
    else
        return cod_find_index_rel(ev_state, queue, index);
}

/* }}} */

static void cod_ev_discard(cod_exec_context ec, int absp, int queue, int index) {
    struct ev_state_data *ev_state = (void*)cod_get_client_data(ec, 0x34567890);
    CManager cm = ev_state->cm;
    queue_item *item;

    item = cod_find_index(absp, ev_state, queue, index);

    assert(item);

    EVdiscard_queue_item(cm, ev_state->stone, item);
}

static void cod_ev_discard_rel(cod_exec_context ec, int queue, int index) {
    cod_ev_discard(ec, 0, queue, index);
}

static void cod_ev_discard_abs(cod_exec_context ec, int queue, int index) {
    cod_ev_discard(ec, 1, queue, index);
}

static void cod_ev_discard_and_submit(cod_exec_context ec,
        int absp, EVstone stone, int queue, int index) {
    struct ev_state_data *ev_state = (void*)cod_get_client_data(ec, 0x34567890);
    CManager cm = ev_state->cm;
    queue_item *item;

    item = cod_find_index(absp, ev_state, queue, index);

    if (item == NULL) {
        printf("Item %x not found on queue %d, stone %d\n", index, queue, stone);
	return;
    }

    item->action_id = -1;

    internal_path_submit(cm, stone, item->item);

    ev_state->did_output++;
    
    EVdiscard_queue_item(cm, ev_state->stone, item);
}

static void cod_ev_discard_and_submit_rel(cod_exec_context ec, EVstone stone, int queue,
        int index) {
    cod_ev_discard_and_submit(ec, 0, stone, queue, index);
}

static void cod_ev_discard_and_submit_abs(cod_exec_context ec, EVstone stone, int queue,
        int index) {
    cod_ev_discard_and_submit(ec, 1, stone, queue, index);
}

static void *cod_ev_get_data(cod_exec_context ec, int absp, int queue, int index)
{
    struct ev_state_data *ev_state = (void*)cod_get_client_data(ec, 0x34567890);
    queue_item *item;
    item = cod_find_index(absp, ev_state, queue, index);

    assert(item);
    assert(item->item);

    if (!item->item->decoded_event) {
        item->item = cod_decode_event(ev_state->cm, ev_state->stone,    
				      ev_state->proto_action_id, item->item);
    }
    assert(item->item->decoded_event);

    return item->item->decoded_event;
}

static void *cod_ev_get_data_rel(cod_exec_context ec, int queue, int index) {
    return cod_ev_get_data(ec, 0, queue, index);
}

static void *cod_ev_get_data_abs(cod_exec_context ec, int queue, int index) {
    return cod_ev_get_data(ec, 1, queue, index);
}

static int cod_ev_conforms(cod_exec_context ec, int queue, int index) {
    struct ev_state_data *ev_state = (void*)cod_get_client_data(ec, 0x34567890);
    return cod_find_index_abs(ev_state, queue, index) != NULL;
}

static int cod_ev_present(cod_exec_context ec, int queue, int index) {
    struct ev_state_data *ev_state = (void*) cod_get_client_data(ec, 0x34567890);
    return cod_find_index_rel(ev_state, queue, index) != NULL;
}

static int cod_ev_count(cod_exec_context ec, int queue) {
    struct ev_state_data *ev_state = (void*) cod_get_client_data(ec, 0x34567890);
    FMFormat type = queue < 0 ? NULL : 
        ev_state->instance->u.queued.formats[queue];
    queue_item *item = ev_state->item;
    int count = 1;

    while (item->next) {
        if (!type || item->item->reference_format == type)
            ++count;
        item = item->next;
    }

    return count;
}

static attr_list cod_ev_get_attrs(cod_exec_context ec, int queue, int index) {
    struct ev_state_data *ev_state = (void*) cod_get_client_data(ec, 0x34567890);
    queue_item *item = cod_find_index_rel(ev_state, queue, index);
    attr_list *pattr;

    if (NULL == item) {
	printf("No item at index %d on queue %d\n", index, queue);

	return NULL;
    }
    pattr = &item->item->attrs;
    if (!*pattr) {
        *pattr = CMcreate_attr_list(ev_state->cm);
    }
    return *pattr;
}

static int
queued_wrapper(CManager cm, struct _queue *queue, queue_item *item,
                void *client_data, int out_count, int *out_stones)
{
    response_instance instance = (response_instance)client_data;
    int(*func)(cod_exec_context) =  /* XXX wrong type */
	(int(*)(cod_exec_context))instance->u.queued.code->func;
    cod_exec_context ec = instance->u.queued.ec;
    struct ev_state_data ev_state;

    ev_state.cm = cm;
    ev_state.cur_event = NULL;
    ev_state.stone = instance->stone;
    ev_state.proto_action_id = instance->proto_action_id;
    ev_state.out_count = out_count;
    ev_state.out_stones = out_stones;
    ev_state.queue = queue;
    ev_state.item = item;
    ev_state.instance = instance;
    ev_state.did_output = 0;
    cod_assoc_client_data(ec, 0x34567890, (long)&ev_state);

    func(ec);

    return ev_state.did_output;
}

static response_instance
generate_filter_code(struct response_spec *mrd, stone_type stone, 
		     FMFormat format);
static response_instance
generate_multityped_code(struct response_spec *mrd, stone_type stone, 
			  FMFormat *formats);

static FMFormat
localize_format(CManager cm, FMFormat format)
{
    FMFormat ret;
    FMStructDescList local_formats = get_localized_formats(format);
    ret = EVregister_format_set(cm, local_formats);
    free(local_formats);
    return ret;
}

void
dump_mrd(void *mrdv)
{
    struct response_spec *mrd = (struct response_spec *) mrdv;
    switch (mrd->response_type) {
    case Response_Filter:
	printf("Response Filter, code is %s\n",
	       mrd->u.filter.function);
	break;
    case Response_Router:
	printf("Response Router, code is %s\n",
	       mrd->u.filter.function);
	break;
    case Response_Transform:
	printf("Response Transform, code is %s\n",
	       mrd->u.transform.function);
	break;
    case Response_Multityped:
	printf("Multityped Action, code is %s\n",
	       mrd->u.transform.function);
	break;
    }
}

static int
proto_action_in_stage(proto_action *act, action_class stage) {
    switch (stage) {
    case Immediate_and_Multi:
        if (act->action_type == Action_Multi) return 1;
        /* fallthrough */
    case Immediate:
        switch (act->action_type) {
        case Action_Terminal:
        case Action_Filter:
        case Action_Split:
        case Action_Immediate:
        case Action_Store:
            return 1;
        default:
            return 0;
        }
    case Bridge:
        return act->action_type == Action_Bridge;
    case Congestion:
        return act->action_type == Action_Congestion;     
    default:
        assert(0);
    }
}

extern int
FMformat_compat_cmp2(FMFormat format, FMFormat *formatList,
		     int listSize, FMcompat_formats * older_format);

int
response_determination(CManager cm, stone_type stone, action_class stage, event_item *event)
{
    int nearest_proto_action = -1;
    int return_value = 0;
    FMFormat conversion_target_format = NULL;
    FMFormat matching_format = NULL;
    int i, format_count = 0;
    FMFormat * formatList;
    int *format_map;
    FMcompat_formats older_format = NULL;

    formatList =
	(FMFormat *) malloc((stone->proto_action_count + 1) * sizeof(FMFormat));
    format_map = (int *) malloc((stone->proto_action_count + 1) * sizeof(int));
    for (i = 0; i < stone->proto_action_count; i++) {
	int j = 0;
        if (!proto_action_in_stage(&stone->proto_actions[i], stage)) {
            continue;
        } 
	while (stone->proto_actions[i].matching_reference_formats && 
	       (stone->proto_actions[i].matching_reference_formats[j] != NULL)) {
	    formatList = (FMFormat *) realloc(formatList, (format_count + 2) * sizeof(FMFormat));
	    format_map = realloc(format_map, (format_count + 2) * sizeof(int));
	    formatList[format_count] = stone->proto_actions[i].matching_reference_formats[j];
	    format_map[format_count] = i;
	    format_count++;
	    j++;
	}
    }
    formatList[format_count] = NULL;
    if (event->reference_format == NULL) {
	/* special case for unformatted input */
	int i;
	for (i=0 ; i < stone->proto_action_count ; i++) {
            if (!proto_action_in_stage(&stone->proto_actions[i], stage)) 
		continue;
	    if ((stone->proto_actions[i].matching_reference_formats == NULL) ||
		(stone->proto_actions[i].matching_reference_formats[0] == NULL))
		nearest_proto_action = i;
	}
    } else {
	int map_entry = FMformat_compat_cmp2(event->reference_format, 
						    formatList,
						    format_count,
						    &older_format);
	if (map_entry != -1) {
            nearest_proto_action = format_map[map_entry];
            matching_format = formatList[map_entry];
        }
    }
    if (nearest_proto_action == -1) {
        /* special case for accepting anything */
        int i;
        for (i=0; i < stone->proto_action_count; i++) {
            if (!proto_action_in_stage(&stone->proto_actions[i], stage)) continue;
            if (stone->proto_actions[i].matching_reference_formats 
                && stone->proto_actions[i].matching_reference_formats[0] == NULL
                && stone->proto_actions[i].data_state != Requires_Decoded) {
                nearest_proto_action = i;
            }
        }
    }
    free(formatList);
    free(format_map);
    if (nearest_proto_action != -1) {
	int action_generated = 0;
	proto_action *proto = &stone->proto_actions[nearest_proto_action];
	if (proto->action_type == Action_Immediate) {
	    /* must be immediate action */
	    response_instance instance;
	    struct response_spec *mrd;
	    mrd = 
		proto->o.imm.mutable_response_data;
	    switch(mrd->response_type) {
	    case Response_Filter:
	    case Response_Router:
		if (event->event_encoded) {
		    conversion_target_format = 
			localize_format(cm, event->reference_format);
		} else {
		    conversion_target_format = event->reference_format;
		}
		break;
	    case Response_Transform:
		conversion_target_format = mrd->u.transform.reference_input_format;
		break;
	    case Response_Multityped:
		assert(FALSE);
                break;
	    }

	    instance = generate_filter_code(mrd, stone, conversion_target_format);
	    if (instance == NULL) return 0;
	    instance->stone = stone->local_id;
	    instance->proto_action_id = nearest_proto_action;
	    action_generated++;
	    switch(mrd->response_type) {
	    case Response_Filter:
		INT_EVassoc_mutated_imm_action(cm, stone->local_id, nearest_proto_action, 
					       filter_wrapper, instance, 
					       conversion_target_format);
		break;
	    case Response_Router:
		INT_EVassoc_mutated_imm_action(cm, stone->local_id, nearest_proto_action, 
					       router_wrapper, instance, 
					       conversion_target_format);
		break;
	    case Response_Transform:
		INT_EVassoc_mutated_imm_action(cm, stone->local_id, nearest_proto_action, 
					       transform_wrapper, instance, 
					       conversion_target_format);
		break;
            default:
		assert(FALSE);
		break;
	    }
	    return_value = 1;
	} else 	if (proto->action_type == Action_Multi || proto->action_type == Action_Congestion) {
	    response_instance instance;
	    struct response_spec *mrd;

	    mrd = 
		proto->o.imm.mutable_response_data;
	    instance = generate_multityped_code(mrd, stone, 
						 proto->matching_reference_formats);
	    if (instance == 0) {
                return 0;
            }
	    instance->stone = stone->local_id;
	    instance->proto_action_id = nearest_proto_action;
	    action_generated++;
	    INT_EVassoc_mutated_multi_action(cm, stone->local_id, nearest_proto_action, 
					      queued_wrapper, instance, 
					      proto->matching_reference_formats);

            if (event->event_encoded) {
                conversion_target_format = matching_format;
            }
            return_value = 1;
	} else {
	    response_cache_element *resp;

	    conversion_target_format = proto->matching_reference_formats[0];
	    /* we'll install the conversion later, first map the response */
	    if (stone->response_cache_count == 0) {
		if (stone->response_cache != NULL) free(stone->response_cache);
		stone->response_cache = malloc(sizeof(stone->response_cache[0]));
	    } else {
		stone->response_cache = 
		    realloc(stone->response_cache,
			    (stone->response_cache_count + 1) * sizeof(stone->response_cache[0]));
	    }
	    resp = &stone->response_cache[stone->response_cache_count++];
	    proto_action *proto = &stone->proto_actions[nearest_proto_action];
	    resp->reference_format = conversion_target_format;
	    resp->proto_action_id = nearest_proto_action;
	    resp->action_type = proto->action_type;
	    resp->requires_decoded = (proto->data_state == Requires_Decoded);
            resp->stage = stage;
	}
	if (conversion_target_format != NULL) {
	    if (event->event_encoded) {
		/* create a decode action */
		INT_EVassoc_conversion_action(cm, stone->local_id, stage,
					      conversion_target_format, 
					      event->reference_format);
/*	    printf(" Returning ");
	    dump_action(stone, a, "   ");*/
		return_value = 1;
	    } else {
		if (event->reference_format != conversion_target_format) {
		    printf("Bad things.  Conversion necessary, but event is not encoded\n");
		} else {
		    return_value = 1;
		}
	    }
	} else {
            return_value = 1;
        }
    }
    return return_value;
}

void
response_data_free(){}

#ifdef NOT_DEF
static void
cod_free_wrapper(void *data, void *free_data)
{
    event_item *event = (event_item *)free_data;
    FMfree_var_rec_elements(event->reference_format,
			    data);
}
#endif

static void
internal_cod_submit(cod_exec_context ec, int port, void *data, void *type_info)
{
    struct ev_state_data *ev_state = (void*)cod_get_client_data(ec, 0x34567890);
    CManager cm = ev_state->cm;
    event_path_data evp = ev_state->cm->evp;
    event_item *event;
    assert(CManager_locked(cm));
    ev_state->did_output++;
    if (ev_state->cur_event && data == ev_state->cur_event->decoded_event) {
	CMtrace_out(cm, EVerbose, 
		    "Internal COD submit, resubmission of current input event to stone %d\n",
		    ev_state->out_stones[port]);
	internal_path_submit(ev_state->cm, ev_state->out_stones[port], ev_state->cur_event);
    } else {
	FMFormat event_format = NULL;
	CMtrace_out(cm, EVerbose, 
		    "Internal COD submit, submission of new data to stone %d\n",
		    ev_state->out_stones[port]);
	if (event_format == NULL) {
	    event_format = EVregister_format_set(cm, (FMStructDescList) type_info);
	    if (event_format == NULL) {
		printf("Bad format information on submit\n");
		return;
	    }
	}
	event = get_free_event(evp);
	event->event_encoded = 0;
	event->decoded_event = data;
	event->reference_format = event_format;
	event->format = NULL;
/*	event->free_func = cod_free_wrapper;*/
	event->free_func = NULL;
	event->free_arg = event;
	event->attrs = NULL;
	cod_encode_event(cm, event);  /* map to memory we trust */
	event->event_encoded = 1;
	event->decoded_event = NULL;  /* lose old data */
	internal_path_submit(cm, ev_state->out_stones[port], event);
	return_event(cm->evp, event);
    }
}


static void
add_standard_routines(stone, context)
stone_type stone;
cod_parse_context context;
{
    static char extern_string[] = "\
		int printf(string format, ...);\n\
		void *malloc(int size);\n\
		void sleep(int seconds);\n\
		void free(void *pointer);\n\
		char * strstr(char * haystack, char * needle);\n\
		long lrand48();\n\
		double drand48();\n\
		void EVsubmit(cod_exec_context ec, int port, void* d, cod_type_spec dt);\n\
		attr_list stone_attrs;";

    static cod_extern_entry externs[] = {
	{"printf", (void *) 0},
	{"malloc", (void*) 0},
	{"free", (void*) 0},
	{"strstr", (void*) 0},
	{"lrand48", (void *) 0},
	{"drand48", (void *) 0},
	{"stone_attrs", (void *) 0},
	{"EVsubmit", (void *) 0},
	{"sleep", (void*) 0},
	{(void *) 0, (void *) 0}
    };
    /* 
     * some compilers think it isn't a static initialization to put this
     * in the structure above, so do it explicitly.
     */
    externs[0].extern_value = (void *) (long) printf;
    externs[1].extern_value = (void *) (long) malloc;
    externs[2].extern_value = (void *) (long) free;
    externs[3].extern_value = (void *) (long) strstr;
    externs[4].extern_value = (void *) (long) lrand48;
    externs[5].extern_value = (void *) (long) drand48;
    externs[6].extern_value = (void *) (long) &stone->stone_attrs;
    externs[7].extern_value = (void *) (long) &internal_cod_submit;
    externs[8].extern_value = (void *) (long) &sleep;

    cod_assoc_externs(context, externs);
    cod_parse_for_context(extern_string, context);
}

static void
add_typed_queued_routines(cod_parse_context context, int index, FMFormat format)
{
    const char *fmt_name;
    char *extern_string;
    static char *extern_string_fmt = 
        "%s *EVdata_%s(cod_exec_context ec, cod_closure_context type, int index);\n"
        "%s *EVdata_full_%s(cod_exec_context ec, cod_closure_context type, int index);\n"
        "void EVdiscard_%s(cod_exec_context ec, cod_closure_context type, int index);\n"
        "int EVcount_%s(cod_exec_context ec, cod_closure_context type);\n"
        "int EVpresent_%s(cod_exec_context ec, cod_closure_context queue, int index);\n"
        "void EVdiscard_and_submit_%s(cod_exec_context ec, int target, cod_closure_context queue, int index);\n"
        "attr_list EVget_attrs_%s(cod_exec_context ec, cod_closure_context queue, int index);\n";
    static cod_extern_entry externs_fmt[] = {
        {"EVdata_%s", (void *) 0},
        {"EVdata_full_%s", (void *) 0},
        {"EVdiscard_%s", (void *) 0},
        {"EVcount_%s", (void *) 0},
        {"EVpresent_%s", (void *) 0},
        {"EVdiscard_and_submit_%s", (void *) 0},
        {"EVget_attrs_%s", (void *) 0},
        {NULL, (void *) 0}
    };
    cod_extern_entry *cur;
    cod_extern_entry *externs;

    fmt_name = name_of_FMformat(format);

    extern_string = malloc(strlen(fmt_name) * 9 + strlen(extern_string_fmt));
    assert(extern_string);

    sprintf(extern_string, extern_string_fmt,
        fmt_name, fmt_name, fmt_name, fmt_name,
        fmt_name, fmt_name, fmt_name, fmt_name,
        fmt_name
        );
    externs = malloc(sizeof(externs_fmt));
    assert(externs);
    memcpy(externs, externs_fmt, sizeof(externs_fmt));
    externs[0].extern_value = (void*) cod_ev_get_data_rel;
    externs[1].extern_value = (void*) cod_ev_get_data_abs;
    externs[2].extern_value = (void*) cod_ev_discard_rel;
    externs[3].extern_value = (void*) cod_ev_count;
    externs[4].extern_value = (void*) cod_ev_present;
    externs[5].extern_value = (void*) cod_ev_discard_and_submit_rel;
    externs[6].extern_value = (void*) cod_ev_get_attrs;

    for (cur = externs; cur->extern_name; ++cur) {
        char *real_name = malloc(strlen(cur->extern_name) + strlen(fmt_name));
        assert(real_name);
        sprintf(real_name, cur->extern_name, fmt_name);
        cur->extern_name = real_name;
    }

    cod_assoc_externs(context, externs);
    cod_parse_for_context(extern_string, context);

    for (cur = externs; cur->extern_name; ++cur) {
        cod_set_closure(cur->extern_name, index, context);
        free(cur->extern_name);
    }
    free(externs);
    free(extern_string);
}

static void
add_queued_routines(cod_parse_context context, FMFormat *formats)
{
    static char extern_string[] = "\
        int EVconforms(cod_exec_context ec, int queue, int index);\n\
        void EVdiscard(cod_exec_context ec, int queue, int index);\n\
        void EVdiscard_full(cod_exec_context ec, int queue, int index);\n\
        void EVdiscard_and_submit(cod_exec_context ec, int target,\
                    int queue, int index);\n\
        void EVdiscard_and_submit_full(cod_exec_context ec, int target,\
                    int queue, int index);\n\
        void *EVdata(cod_exec_context ec, int queue, int index);\n\
        void *EVdata_full(cod_exec_context ec, int queue, int index);\n\
        int EVcount(cod_exec_context ec, int queue);\n\
        int EVpresent(cod_exec_context ec, int queue, int index);\n";
    static cod_extern_entry externs[] = {
        {"EVconforms", (void *)0},
        {"EVdiscard", (void *)0},
        {"EVdiscard_full",  (void *)0},
        {"EVdiscard_and_submit", (void *)0},
        {"EVdiscard_and_submit_full", (void *)0},
        {"EVdata", (void *)0},
        {"EVdata_full", (void *)0},
        {"EVcount", (void *)0},
        {"EVpresent", (void *)0},
        {(void *)0, (void *)0}
    };
    int i;
    FMFormat *cur;
    
    externs[0].extern_value = (void*)cod_ev_conforms;
    externs[1].extern_value = (void*)cod_ev_discard_rel;
    externs[2].extern_value = (void*)cod_ev_discard_abs;
    externs[3].extern_value = (void*)cod_ev_discard_and_submit_rel;
    externs[4].extern_value = (void*)cod_ev_discard_and_submit_abs;
    externs[5].extern_value = (void*)cod_ev_get_data_rel;
    externs[6].extern_value = (void*)cod_ev_get_data_abs;
    externs[7].extern_value = (void*)cod_ev_count;
    externs[8].extern_value = (void*)cod_ev_present;

    cod_assoc_externs(context, externs);
    cod_parse_for_context(extern_string, context);

    for (cur = formats, i = 0; *cur; ++cur, ++i) {
        add_typed_queued_routines(context, i, *cur);
    }
}

static void
add_queued_constants(cod_parse_context context, FMFormat *formats)
{
    FMFormat *cur_format;
    int i = 0;
    for (cur_format = formats; *cur_format; ++cur_format, ++i) {
        const char *fmt_name = name_of_FMformat(*cur_format);
        char *name = malloc(4 + strlen(fmt_name));
        sprintf(name, "%s_ID", fmt_name);
        cod_add_int_constant_to_parse_context(name, i, context);
        /* free(name); */
    }
}


extern sm_ref
cod_build_type_node(const char *name, FMFieldList field_list);
extern sm_ref
cod_build_param_node(const char *id, sm_ref typ, int param_num);
extern void
cod_add_decl_to_parse_context(const char *name, sm_ref item, cod_parse_context context);
extern void
cod_add_param(const char *id, const char *typ, int param_num, 
	      cod_parse_context context);

static void
add_param(cod_parse_context parse_context, char *name, int param_num,
	  FMFormat format)
{
    FMStructDescList list = format_list_of_FMFormat(format);
    int i = 1;
    sm_ref type, param;
    while (list[i].format_name != NULL) {
	FMFieldList fl = list[i].field_list;
	/* step through input formats */
	cod_add_struct_type(list[i].format_name, fl, parse_context);
	i++;
    }
    type = cod_build_type_node(list[0].format_name, list[0].field_list);
    cod_add_decl_to_parse_context(list[0].format_name, type, parse_context);

    param = cod_build_param_node(name, type, param_num);

    cod_add_decl_to_parse_context(name, param, parse_context);
}

static void
add_type(cod_parse_context parse_context, FMFormat format)
{
    FMStructDescList list = format_list_of_FMFormat(format);
    for (; list->format_name; ++list) {
        cod_add_struct_type(list->format_name, list->field_list, parse_context);
    }
}

#if 0
static void
add_param_list(cod_parse_context parse_context, char *name, int param_num,
	  FMStructDescList list)
{
    char *tname = malloc(strlen(name) + strlen("_type") +1);
    sm_ref type, param;
    int i = 0;
    while (list[i].format_name != NULL) {
	sm_ref typ;
	/* step through input formats */
	typ = cod_build_type_node(list[i].format_name,
				  list[i].field_list);
	cod_add_decl_to_parse_context(list[i].format_name, typ,
				      parse_context);
	i++;
    }
    sprintf(tname, "%s_type", name);
    type = cod_build_type_node(tname, list[i-1].field_list);
    cod_add_decl_to_parse_context(tname, type, parse_context);

    param = cod_build_param_node(name, type, 0);

    cod_add_decl_to_parse_context(name, param, parse_context);
}
#endif

static int
dll_prefix_present(filter)
char *filter;
{

    if (filter[0] == 'd' && filter[1] == 'l' && filter[2] == 'l' && filter[3] == ':') {
	return 1;
    }
    return 0;
}

static char *
extract_dll_path(filter)
char *filter;
{
    char *copy = strdup(filter);
    char *temp;
    char *path;


    temp = strtok(copy, ":");
    if (strcmp(temp, "dll")) {
    return NULL;
    }
    temp = strtok(NULL, ":");

    if (temp == NULL)
    return NULL;

    path = strdup(temp);

    return path;
}

static char *
extract_symbol_name(filter)
char *filter;
{

    char *copy = strdup(filter);
    char *temp;
    char *symbol;

    temp = strtok(copy, ":");
    if (strcmp(temp, "dll")) {
    return NULL;
    }
    temp = strtok(NULL, ":");
    temp = strtok(NULL, ":");

    if (temp == NULL)
    return NULL;

    symbol = strdup(temp);

    return symbol;
}

static void*
load_dll_symbol(path, symbol_name)
char *path;
char *symbol_name;
{
    lt_dlhandle handle;

	if(lt_dlinit() != 0) {
		fprintf(stderr, "Error in init: %s\n", lt_dlerror());
		return NULL;
	}
    handle = lt_dlopen(path);
    if (!handle) {
    	fprintf(stderr, "failed opening %s  %s\n", path, lt_dlerror());
	    return NULL;
    }
    return lt_dlsym(handle, symbol_name);
}


static response_instance
generate_filter_code(mrd, stone, format)
struct response_spec *mrd;
stone_type stone;
FMFormat format;
{
    response_instance instance = malloc(sizeof(*instance));

    cod_code code;
    cod_parse_context parse_context = new_cod_parse_context();
    /*    sm_ref conn_info_data_type, conn_info_param;*/

    memset(instance, 0, sizeof(*instance));
    add_standard_routines(stone, parse_context);

    switch (mrd->response_type) {
    case Response_Filter:
    case Response_Router:
    case Response_Transform:
	cod_add_param("ec", "cod_exec_context", 0, parse_context);
	if (format) {
	    add_param(parse_context, "input", 1, format);
	} else {
	    cod_add_param("input", "int", 1, parse_context);
	}
	if (mrd->response_type == Response_Transform) {
	    add_param(parse_context, "output", 2, 
		      mrd->u.transform.reference_output_format);
	    cod_add_param("event_attrs", "attr_list", 3, parse_context);
	} else {
	    cod_add_param("event_attrs", "attr_list", 2, parse_context);
	}
	break;
    case Response_Multityped:
        /* this should call generate_multityped_code() */
        assert(FALSE);
	break;
    }
	    
/*    conn_info_data_type = cod_build_type_node("output_conn_info_type",
					      output_conn_field_list);
    cod_add_decl_to_parse_context("output_conn_info_type", 
				  conn_info_data_type, parse_context);
    conn_info_param = cod_build_param_node("output_conn_info",
					   conn_info_data_type, 3);
    cod_add_decl_to_parse_context("output_conn_info", conn_info_param,
				  parse_context);
*/
    switch(mrd->response_type) {
    case Response_Filter:
    case Response_Router:
	if (dll_prefix_present(mrd->u.filter.function)) {
	    /* it is a dll */
	    char *path = NULL;
	    char *symbol_name = NULL;
	    
	    path = extract_dll_path(mrd->u.filter.function);
	    symbol_name = extract_symbol_name(mrd->u.filter.function);
	    if (!path || !symbol_name) {
		fprintf(stderr, "could not parse string \"%s\" for dll path and symbol information\n", mrd->u.filter.function);
		free(instance);
		return NULL;
	    }
	    instance->u.filter.func_ptr = (int(*)(void*,attr_list)) load_dll_symbol(path, symbol_name);
	    if (instance->u.filter.func_ptr == NULL) {
		fprintf(stderr, "Failed to load symbol \"%s\" from file \"%s\"\n",
			symbol_name, path);
		free(instance);
		return NULL;
	    }
	    instance->u.filter.code = NULL;
	} else {
	    code = cod_code_gen(mrd->u.filter.function, parse_context);
	    instance->response_type = mrd->response_type;
	    instance->u.filter.code = code;
	    if (code)
		instance->u.filter.ec = cod_create_exec_context(code);
	    
	    instance->u.filter.func_ptr = NULL;
	}
	break;
    case Response_Transform:
	if (dll_prefix_present(mrd->u.transform.function)) {
	    /* it is a dll */
	    char *path = NULL;
	    char *symbol_name = NULL;
	    
	    path = extract_dll_path(mrd->u.transform.function);
	    symbol_name = extract_symbol_name(mrd->u.transform.function);
	    if (!path || !symbol_name) {
		fprintf(stderr, "could not parse string \"%s\" for dll path and symbol information\n", mrd->u.transform.function);
		free(instance);
		return NULL;
	    }
	    instance->u.transform.func_ptr = 
		(int(*)(void*,void*,attr_list)) load_dll_symbol(path, symbol_name);
	    if (instance->u.transform.func_ptr == NULL) {
		fprintf(stderr, "Failed to load symbol \"%s\" from file \"%s\"\n",
			symbol_name, path);
		free(instance);
		return NULL;
	    }
	    instance->u.transform.code = NULL;
	} else {
	    code = cod_code_gen(mrd->u.transform.function, parse_context);
	    instance->response_type = Response_Transform;
	    instance->u.transform.code = code;
	    if (code)
		instance->u.transform.ec = cod_create_exec_context(code);
	}
	instance->u.transform.out_size = 
	    mrd->u.transform.output_base_struct_size;
	instance->u.transform.out_format = 
	    mrd->u.transform.reference_output_format;
	break;
    case Response_Multityped:
	break;
    }
    cod_free_parse_context(parse_context);

    return instance;
}

static response_instance
generate_multityped_code(mrd, stone, formats)
struct response_spec *mrd;
stone_type stone;
FMFormat *formats;
{
    response_instance instance = malloc(sizeof(*instance));
    FMFormat *cur_format;

    cod_code code;
    cod_parse_context parse_context = new_cod_parse_context();
    /*    sm_ref conn_info_data_type, conn_info_param;*/

    memset(instance, 0, sizeof(*instance));

    for (cur_format = formats; *cur_format; ++cur_format) {
        add_type(parse_context, *cur_format);
    }

    add_standard_routines(stone, parse_context);
    add_queued_routines(parse_context, formats);
    add_queued_constants(parse_context, formats);


    assert(mrd->response_type == Response_Multityped);
    cod_add_param("ec", "cod_exec_context", 0, parse_context);
/*    if (format) {
	add_param(parse_context, "input", 1, format);
    } else {
	cod_add_param("input", "int", 1, parse_context);
	}*/
    code = cod_code_gen(mrd->u.multityped.function, parse_context);
    instance->response_type = mrd->response_type;
    instance->u.queued.formats = formats;
    instance->u.queued.code = code;
    if (code)
	instance->u.queued.ec = cod_create_exec_context(code);
    
    cod_free_parse_context(parse_context);

    if (!instance->u.queued.ec) {
        free(instance);
        return NULL;
    }

    return instance;
}
