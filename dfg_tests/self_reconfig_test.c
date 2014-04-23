#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#include "config.h"
#include "evpath.h"
#include "ev_dfg.h"
#include "test_support.h"

static int status;
static EVdfg test_dfg;

static char *filter_func = "{\n\
	int hop_count;\n\
	static int event_count = 0;\n\
	int reconfig = attr_ivalue(stone_attrs, \"DoReconfig\");\n \
	event_count++;\n\
	set_int_attr(stone_attrs, \"EventCount\", event_count);\n\
	if (reconfig && event_count == 5) {\n\
	    EVdfg_trigger_reconfiguration();\n\
	}\n\
	hop_count = attr_ivalue(event_attrs, \"hop_count_atom\");\n\
	hop_count++;\n\
	set_int_attr(event_attrs, \"hop_count_atom\", hop_count);\n\
	return 1;\n\
}\0\0";

static char *new_filter_func = "{\n\
	int hop_count;\n\
	hop_count = attr_ivalue(event_attrs, \"hop_count_atom\");\n\
	hop_count+= 10 * hop_count;\n\
	hop_count++;\n\
	set_int_attr(event_attrs, \"hop_count_atom\", hop_count);\n\
	return 1;\n\
}\0\0";

#define REPEAT_COUNT 50

#include "ev_dfg_internal.h"
static int received_count = -1;
static void
on_failure()
{
    int i;
    printf("In failure\n");
    if (received_count != -1) {
	printf("I'm the sink, got only %d events\n", received_count);
    }
    for (i=0; i < test_dfg->node_count; i++) {
	printf("NODE %d status is :", i);
	switch (test_dfg->nodes[i].shutdown_status_contribution) {
	case STATUS_UNDETERMINED:
	    printf("NOT READY FOR SHUTDOWN\n");
	    break;
	case STATUS_NO_CONTRIBUTION:
	    printf("READY for shutdown, no status\n");
	    break;
	case STATUS_SUCCESS:
	    printf("READY for shutdown, SUCCESS\n");
	    break;
	default:
	    printf("READY for shutdown, FAILURE %d\n",
			test_dfg->nodes[i].shutdown_status_contribution);
	    break;
	}	    
    }
}

int node_count = 3;
char **nodes;
EVdfg_stone *stones;
EVdfg_stone reconfig_prev, reconfig_next;

static
int
simple_handler(CManager cm, void *vevent, void *client_data, attr_list attrs)
{
    simple_rec_ptr event = vevent;
    static int count = 0;
    (void)cm;
    (void)client_data;
    int hop_count;
    atom_t hop_count_atom = -1;
    if (hop_count_atom == -1) {
	hop_count_atom = attr_atom_from_string("hop_count_atom");
    }
    get_int_attr(attrs, hop_count_atom, &hop_count);
    checksum_simple_record(event, attrs, quiet);
    count++;
    received_count = count;
    if (!quiet)
	printf("Goal hops is %d\n", node_count + 10*((int)(node_count/2)));
    if (hop_count == node_count + 10*((int)(node_count/2))) {
	if (!quiet) printf("SINK complete\n");
        EVdfg_shutdown(test_dfg, 0);
    }
    if (!quiet) printf("\nreceived had %d hops\n", hop_count);
    return 0;
}


extern void
reconfig_handler(EVdfg dfg)
{
    char *filter;
    EVdfg_stone middle_stone;
    attr_list prev_stone_attrs;
    atom_t event_count_atom;
    int event_count;
    if (!quiet) 
	printf("Master has been requested to reconfigure\n");


    prev_stone_attrs = EVdfg_get_attr_list(reconfig_next);
    event_count_atom = attr_atom_from_string("EventCount");
    if (!get_int_attr(prev_stone_attrs, event_count_atom, &event_count)) {
	printf("Failed to get event count\n");
    } else {
	if (!quiet) 
	    printf("Event count is %d\n", event_count);
    }
    if (event_count != 5) {
	printf("Improper event count from reconfiguring node, test failure, don't reconfigure\n");
	return;
    }
    filter = create_filter_action_spec(NULL, new_filter_func);
    middle_stone = EVdfg_create_stone(dfg, filter);
    EVdfg_assign_node(middle_stone, nodes[node_count/2-1]);
    if (!quiet)
	printf("new stone deployed to node %s\n", nodes[node_count/2]);
		
    EVdfg_reconfig_insert_on_port(dfg, reconfig_prev, 0, middle_stone, NULL);
    EVdfg_realize(dfg);
    if (!quiet) 
	printf("realized\n");

}

extern int
be_test_master(int argc, char **argv)
{
    CManager cm;
    char *str_contact;
    EVdfg_stone src, last, tmp, sink;
    EVsource source_handle;
    int i;
    char *filter;

    alarm(240);  /* reset time limit to 4 minutes */
    if (argc == 1) {
	sscanf(argv[0], "%d", &node_count);
    }
    on_exit_handler = on_failure;
    nodes = malloc(sizeof(nodes[0]) * (node_count+1));
    stones = malloc(sizeof(stones[0]) * (node_count+1));
    for (i=0; i < node_count; i++) {
	nodes[i] = malloc(5);
	sprintf(nodes[i], "N%d", i);
    }
    nodes[node_count] = NULL;
    cm = CManager_create();
    CMlisten(cm);

/*
**  LOCAL DFG SUPPORT   Sources and sinks that might or might not be utilized.
*/

    source_handle = EVcreate_submit_handle(cm, -1, simple_format_list);
    EVdfg_register_source("master_source", source_handle);
    EVdfg_register_sink_handler(cm, "simple_handler", simple_format_list,
				(EVSimpleHandlerFunc) simple_handler);

/*
**  DFG CREATION
*/
    test_dfg = EVdfg_create(cm);
    str_contact = EVdfg_get_contact_list(test_dfg);
    EVdfg_node_reconfig_handler(test_dfg, reconfig_handler);
    EVdfg_register_node_list(test_dfg, &nodes[0]);
    src = EVdfg_create_source_stone(test_dfg, "master_source");
    EVdfg_assign_node(src, nodes[0]);
    if (!quiet)
	printf("stone %d deployed to node %s\n", 0, nodes[0]);

    stones[0] = last = src;
    filter = create_filter_action_spec(NULL, filter_func);

    for (i=1; i < node_count -1; i++) {
	stones[i] = tmp = EVdfg_create_stone(test_dfg, filter);
	EVdfg_link_port(last, 0, tmp);
	EVdfg_assign_node(tmp, nodes[i]);
	if (!quiet)
	    printf("stone %d deployed to node %s\n", i, nodes[i]);
	if (i == ( node_count / 2 )) {
	    attr_list attrs = create_attr_list();
	    set_int_attr(attrs, attr_atom_from_string("DoReconfig"), 1);
	    EVdfg_set_attr_list(tmp, attrs);  /* this one will cause reconfig */
	    free_attr_list(attrs);
	    reconfig_prev = last;
	    reconfig_next = tmp;
	}
	last = tmp;
    }
    sink = EVdfg_create_sink_stone(test_dfg, "simple_handler");
    stones[node_count-1] = sink;
    EVdfg_link_port(last, 0, sink);
    EVdfg_assign_node(sink, nodes[node_count-1]);
    if (!quiet)
	printf("stone %d deployed to node %s\n", node_count-1, nodes[node_count-1]);

    EVdfg_realize(test_dfg);

/* We're node 0 in the DFG */
    EVdfg_join_dfg(test_dfg, nodes[0], str_contact);

/* Fork the others */
    test_fork_children(&nodes[0], str_contact);

    free(str_contact);

    if (EVdfg_ready_wait(test_dfg) != 1) {
	/* dfg initialization failed! */
	exit(1);
    }

    
    if (EVdfg_active_sink_count(test_dfg) == 0) {
	EVdfg_ready_for_shutdown(test_dfg);
    }

    if (EVdfg_source_active(source_handle)) {
	for (i=0 ; i < REPEAT_COUNT; i++) {
	    simple_rec rec;
	    atom_t hop_count_atom;
	    attr_list attrs = create_attr_list();
	    hop_count_atom = attr_atom_from_string("hop_count_atom");
	    add_int_attr(attrs, hop_count_atom, 1);
	    generate_simple_record(&rec);
	    /* submit would be quietly ignored if source is not active */
	    EVsubmit(source_handle, &rec, attrs);
	    free_attr_list(attrs);
	    CMsleep(cm, 1);
	}
	if (!quiet) printf("Source complete\n");
    }
    EVfree_source(source_handle);

    status = EVdfg_wait_for_shutdown(test_dfg);

    wait_for_children(nodes);
    for (i=0; i < node_count; i++) {
	free(nodes[i]);
    }
    free(nodes);
    free(stones);

    CManager_close(cm);
    return status;
}


extern int
be_test_child(int argc, char **argv)
{
    CManager cm;
    EVsource src;

    alarm(240);   /* reset time limit to 4 minutes */
    cm = CManager_create();
    if (argc != 3) {
	printf("Child usage:  evtest  <nodename> <mastercontact>\n");
	exit(1);
    }
    test_dfg = EVdfg_create(cm);

    src = EVcreate_submit_handle(cm, -1, simple_format_list);
    EVdfg_register_source("master_source", src);
    EVdfg_register_sink_handler(cm, "simple_handler", simple_format_list,
				(EVSimpleHandlerFunc) simple_handler);
    EVdfg_join_dfg(test_dfg, argv[1], argv[2]);
    EVdfg_ready_wait(test_dfg);

    if (EVdfg_active_sink_count(test_dfg) == 0) {
	EVdfg_ready_for_shutdown(test_dfg);
    }

    if (EVdfg_source_active(src)) {
	simple_rec rec;
	generate_simple_record(&rec);
	/* submit would be quietly ignored if source is not active */
	EVsubmit(src, &rec, NULL);
    }
    EVfree_source(src);
    status = EVdfg_wait_for_shutdown(test_dfg);
    CManager_close(cm);
    return status;
}