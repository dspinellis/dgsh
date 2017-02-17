#include <check.h>  /* Check unit test framework API. */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <unistd.h> /* pipe() */
#include <fcntl.h>  /* fcntl() */
#include <err.h>    /* err(), errx() */
#include <time.h>   /* nanosleep() */
#include <stdio.h> /* snprintf */
#include <unistd.h> /* pipe */
#include <sys/types.h>
#include <sys/socket.h> /* socket */
#include <sys/un.h> /* sockaddr_un */
#include "../src/negotiate.h"
#include "../src/negotiate.c"	/* struct definitions, static structures */
#include "../src/dgsh-conc.c"			/* pi */
//#include "../src/dgsh-internal-api.h"		/* chosen_mb */


struct dgsh_negotiation *fresh_mb;
struct dgsh_edge *compact_edges;
struct dgsh_edge **pointers_to_edges;
int n_ptedges;
int *args;
/* Depending on whether a test triggers a failure or not, a different sequence
 * of actions may be needed to exit normally.
 * exit_state is the control variable for following the correct 
 * sequence of actions.
 */
int exit_state = 0;

void
setup_concs(struct dgsh_negotiation *mb)
{
	mb->n_concs = 2;
	mb->conc_array = (struct dgsh_conc *)malloc(sizeof(struct dgsh_conc) * mb->n_concs);
	mb->conc_array[0].pid = 2000;
	mb->conc_array[0].input_fds = 2;
	mb->conc_array[0].output_fds = 2;
	mb->conc_array[0].multiple_inputs = false;
	mb->conc_array[0].endpoint_pid = 102;
	mb->conc_array[0].n_proc_pids = 2;
	mb->conc_array[0].proc_pids = (int *)malloc(sizeof(int) * 2);
	mb->conc_array[0].proc_pids[0] = 100;
	mb->conc_array[0].proc_pids[1] = 101;

	mb->conc_array[1].pid = 2001;
	mb->conc_array[1].input_fds = 3;
	mb->conc_array[1].output_fds = 3;
	mb->conc_array[1].multiple_inputs = true;
	mb->conc_array[1].endpoint_pid = 103;
	mb->conc_array[1].n_proc_pids = 2;
	mb->conc_array[1].proc_pids = (int *)malloc(sizeof(int) * 2);
	mb->conc_array[1].proc_pids[0] = 100;
	mb->conc_array[1].proc_pids[1] = 101;
}


void
setup_graph_solution(void)
{
	chosen_mb->graph_solution = (struct dgsh_node_connections *)malloc(
			sizeof(struct dgsh_node_connections) * 
			chosen_mb->n_nodes);
	struct dgsh_node_connections *graph_solution =
		chosen_mb->graph_solution;
	graph_solution[0].node_index = 0;
	graph_solution[0].n_edges_incoming = 2;
	graph_solution[0].edges_incoming = (struct dgsh_edge *)malloc(
		sizeof(struct dgsh_edge) * graph_solution[0].n_edges_incoming);
	memcpy(&graph_solution[0].edges_incoming[0], &chosen_mb->edge_array[0],
					sizeof(struct dgsh_edge));
	memcpy(&graph_solution[0].edges_incoming[1], &chosen_mb->edge_array[2],
					sizeof(struct dgsh_edge));
	graph_solution[0].n_edges_outgoing = 1;
	graph_solution[0].edges_outgoing = (struct dgsh_edge *)malloc(
		sizeof(struct dgsh_edge) * graph_solution[0].n_edges_outgoing);
	memcpy(&graph_solution[0].edges_outgoing[0], &chosen_mb->edge_array[4],
					sizeof(struct dgsh_edge));

	graph_solution[1].node_index = 1;
	graph_solution[1].n_edges_incoming = 1;
	graph_solution[1].edges_incoming = (struct dgsh_edge *)malloc(
		sizeof(struct dgsh_edge) * graph_solution[1].n_edges_incoming);
	memcpy(&graph_solution[1].edges_incoming[0], &chosen_mb->edge_array[1],
					sizeof(struct dgsh_edge));
	graph_solution[1].n_edges_outgoing = 2;
	graph_solution[1].edges_outgoing = (struct dgsh_edge *)malloc(
		sizeof(struct dgsh_edge) * graph_solution[1].n_edges_outgoing);
	memcpy(&graph_solution[1].edges_outgoing[0], &chosen_mb->edge_array[2],
					sizeof(struct dgsh_edge));
	memcpy(&graph_solution[1].edges_outgoing[1], &chosen_mb->edge_array[3],
					sizeof(struct dgsh_edge));

	graph_solution[2].node_index = 2;
	graph_solution[2].n_edges_incoming = 0;
	graph_solution[2].edges_incoming = NULL;
	graph_solution[2].n_edges_outgoing = 2;
	graph_solution[2].edges_outgoing = (struct dgsh_edge *)malloc(
		sizeof(struct dgsh_edge) * graph_solution[2].n_edges_outgoing);
	memcpy(&graph_solution[2].edges_outgoing[0], &chosen_mb->edge_array[0],
					sizeof(struct dgsh_edge));
	memcpy(&graph_solution[2].edges_outgoing[1], &chosen_mb->edge_array[1],
					sizeof(struct dgsh_edge));

	graph_solution[3].node_index = 3;
	graph_solution[3].n_edges_incoming = 2;
	graph_solution[3].edges_incoming = (struct dgsh_edge *)malloc(
		sizeof(struct dgsh_edge) * graph_solution[3].n_edges_incoming);
	memcpy(&graph_solution[0].edges_incoming[0], &chosen_mb->edge_array[3],
					sizeof(struct dgsh_edge));
	memcpy(&graph_solution[0].edges_incoming[1], &chosen_mb->edge_array[4],
					sizeof(struct dgsh_edge));
	graph_solution[3].n_edges_outgoing = 0;
	graph_solution[3].edges_outgoing = NULL;

}

void
setup_chosen_mb(void)
{
	struct dgsh_node *nodes;
	struct dgsh_edge *edges;
	int n_nodes;
	int n_edges;
	n_nodes = 4;
        nodes = (struct dgsh_node *)malloc(sizeof(struct dgsh_node) * n_nodes);
        nodes[0].pid = 100;
	nodes[0].index = 0;
        strcpy(nodes[0].name, "proc0");
        nodes[0].requires_channels = 2;
	nodes[0].provides_channels = 1;
	nodes[0].dgsh_in = 1;
        nodes[0].dgsh_out = 1;

        nodes[1].pid = 101;
	nodes[1].index = 1;
        strcpy(nodes[1].name, "proc1");
        nodes[1].requires_channels = 1;
	nodes[1].provides_channels = 2;
	nodes[1].dgsh_in = 1;
        nodes[1].dgsh_out = 1;

	/* dgsh OUT and not IN = initiator node.
         * This node could start the negotiation.
         * Fix.
	 */
        nodes[2].pid = 102;
	nodes[2].index = 2;
        strcpy(nodes[2].name, "proc2");
        nodes[2].requires_channels = 0;
	nodes[2].provides_channels = 2;
	nodes[2].dgsh_in = 0;
        nodes[2].dgsh_out = 1;

	/* dgsh IN and not OUT = termination node.
         * This node couldn't start the negotiation.
         * Fix.
	 */
        nodes[3].pid = 103;
	nodes[3].index = 3;
        strcpy(nodes[3].name, "proc3");
        nodes[3].requires_channels = 2;
	nodes[3].provides_channels = 0;
	nodes[3].dgsh_in = 1;
        nodes[3].dgsh_out = 0;

        n_edges = 5;
        edges = (struct dgsh_edge *)malloc(sizeof(struct dgsh_edge) *n_edges);
        edges[0].from = 2;
        edges[0].to = 0;
        edges[0].instances = 0;
        edges[0].from_instances = 0;
        edges[0].to_instances = 0;

        edges[1].from = 2;
        edges[1].to = 1;
        edges[1].instances = 0;
        edges[1].from_instances = 0;
        edges[1].to_instances = 0;

        edges[2].from = 1;
        edges[2].to = 0;
        edges[2].instances = 0;
        edges[2].from_instances = 0;
        edges[2].to_instances = 0;

        edges[3].from = 1;
        edges[3].to = 3;
        edges[3].instances = 0;
        edges[3].from_instances = 0;
        edges[3].to_instances = 0;

        edges[4].from = 0;
        edges[4].to = 3;
        edges[4].instances = 0;
        edges[4].from_instances = 0;
        edges[4].to_instances = 0;

        double dgsh_version = 0.1;
        chosen_mb = (struct dgsh_negotiation *)malloc(sizeof(struct dgsh_negotiation));
        chosen_mb->version = dgsh_version;
        chosen_mb->node_array = nodes;
        chosen_mb->n_nodes = n_nodes;
        chosen_mb->edge_array = edges;
        chosen_mb->n_edges = n_edges;
	chosen_mb->graph_solution = NULL;

	/* check_negotiation_round() */
	chosen_mb->state = PS_NEGOTIATION;
	chosen_mb->initiator_pid = 103; /* Node 3 */
	chosen_mb->origin_fd_direction = STDOUT_FILENO;
	chosen_mb->n_concs = 0;
	chosen_mb->conc_array = NULL;
}

/* Identical to chosen_mb except for the initiator field. */
void
setup_mb(struct dgsh_negotiation **mb)
{
	struct dgsh_node *nodes;
	struct dgsh_edge *edges;
	int n_nodes;
	int n_edges;
	n_nodes = 4;
        nodes = (struct dgsh_node *)malloc(sizeof(struct dgsh_node) * n_nodes);
        nodes[0].pid = 100;
	nodes[0].index = 0;
        strcpy(nodes[0].name, "proc0");
        nodes[0].requires_channels = 2;
	nodes[0].provides_channels = 1;
	nodes[0].dgsh_in = 1;
        nodes[0].dgsh_out = 1;

        nodes[1].pid = 101;
	nodes[1].index = 1;
        strcpy(nodes[1].name, "proc1");
        nodes[1].requires_channels = 1;
	nodes[1].provides_channels = 2;
	nodes[1].dgsh_in = 1;
        nodes[1].dgsh_out = 1;

        nodes[2].pid = 102;
	nodes[2].index = 2;
        strcpy(nodes[2].name, "proc2");
        nodes[2].requires_channels = 0;
	nodes[2].provides_channels = 2;
	nodes[2].dgsh_in = 0;
        nodes[2].dgsh_out = 1;

        nodes[3].pid = 103;
	nodes[3].index = 3;
        strcpy(nodes[3].name, "proc3");
        nodes[3].requires_channels = 2;
	nodes[3].provides_channels = 0;
	nodes[3].dgsh_in = 1;
        nodes[3].dgsh_out = 0;

        n_edges = 5;
        edges = (struct dgsh_edge *)malloc(sizeof(struct dgsh_edge) *n_edges);
        edges[0].from = 2;
        edges[0].to = 0;
        edges[0].instances = 0;
        edges[0].from_instances = 0;
        edges[0].to_instances = 0;

        edges[1].from = 2;
        edges[1].to = 1;
        edges[1].instances = 0;
        edges[1].from_instances = 0;
        edges[1].to_instances = 0;

        edges[2].from = 1;
        edges[2].to = 0;
        edges[2].instances = 0;
        edges[2].from_instances = 0;
        edges[2].to_instances = 0;

        edges[3].from = 1;
        edges[3].to = 3;
        edges[3].instances = 0;
        edges[3].from_instances = 0;
        edges[3].to_instances = 0;

        edges[4].from = 0;
        edges[4].to = 3;
        edges[4].instances = 0;
        edges[4].from_instances = 0;
        edges[4].to_instances = 0;

        double dgsh_version = 0.1;
        struct dgsh_negotiation *temp_mb = (struct dgsh_negotiation *)malloc(sizeof(struct dgsh_negotiation));
        temp_mb->version = dgsh_version;
        temp_mb->node_array = nodes;
        temp_mb->n_nodes = n_nodes;
        temp_mb->edge_array = edges;
        temp_mb->n_edges = n_edges;
	temp_mb->graph_solution = NULL;

	/* check_negotiation_round() */
	temp_mb->state = PS_NEGOTIATION;
	temp_mb->initiator_pid = 102; /* Node 2 */
	temp_mb->origin_index = 2;
	temp_mb->origin_fd_direction = STDOUT_FILENO;
	temp_mb->n_concs = 0;
	temp_mb->conc_array = NULL;

	*mb = temp_mb;
}

void
setup_pointers_to_edges(void)
{
        n_ptedges = 2;
	pointers_to_edges = (struct dgsh_edge **)malloc(sizeof(struct dgsh_edge *) *n_ptedges);
	int i;
	for (i = 0; i < n_ptedges; i++) {
		pointers_to_edges[i] = (struct dgsh_edge *)malloc(sizeof(struct dgsh_edge));	
		pointers_to_edges[i]->from = i;
		pointers_to_edges[i]->to = 3; // the node.
		pointers_to_edges[i]->instances = 0;
		pointers_to_edges[i]->from_instances = 0;
		pointers_to_edges[i]->to_instances = 0;
        }
}

void
setup_self_node(void)
{
	/* fill in self_node */
	memcpy(&self_node, &chosen_mb->node_array[3], sizeof(struct dgsh_node));
}

void
setup_self_node_io_side(void)
{
	self_node_io_side.index = 3;
	self_node_io_side.fd_direction = 0;
}

/* establish_io_connections() */
void
setup_pipe_fds(void)
{
	/* fill in self_pipe_fds */
	self_pipe_fds.n_input_fds = 2;
	self_pipe_fds.input_fds = (int *)malloc(sizeof(int) *
						self_pipe_fds.n_input_fds);
	self_pipe_fds.input_fds[0] = 3;
	self_pipe_fds.input_fds[1] = 4;
	self_pipe_fds.n_output_fds = 0;
}


void setup_args(void)
{
        args = (int *)malloc(sizeof(int) * 3);
        args[0] = -1;
        args[1] = -1;
        args[2] = -1;
}

void
setup(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
	setup_pipe_fds();
	setup_graph_solution();
}

void
setup_test_set_fds(void)
{
	setup_chosen_mb();
	setup_self_node();
}

void
setup_test_add_node(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
}

void
setup_test_lookup_dgsh_edge(void)
{
	setup_chosen_mb();
}

void
setup_test_fill_dgsh_edge(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
}

void
setup_test_add_edge(void)
{
	setup_chosen_mb();
}

void
setup_test_try_add_dgsh_edge(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
}

void
setup_test_try_add_dgsh_node(void)
{
	setup_chosen_mb();
	setup_self_node();
}

void
setup_test_fill_node(void)
{
	/* setup_self_node() requires setup_chosen_mb() */
	setup_chosen_mb();
	setup_self_node();
}

void
setup_test_free_mb(void)
{
	setup_chosen_mb();
}

void
setup_test_analyse_read(void)
{
	setup_chosen_mb();
	setup_mb(&fresh_mb);
	setup_self_node();
	setup_self_node_io_side();
}

/*void
setup_test_point_io_direction(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
}*/

void
setup_test_alloc_copy_graph_solution(void)
{
	setup_mb(&fresh_mb);
}

void
setup_test_alloc_copy_concs(void)
{
	setup_mb(&fresh_mb);
}

void
setup_test_alloc_copy_edges(void)
{
	setup_mb(&fresh_mb);
}

void
setup_test_alloc_copy_nodes(void)
{
	setup_mb(&fresh_mb);
}

void
setup_test_read_chunk(void)
{
	setup_self_node_io_side();
}

void
setup_test_alloc_io_fds(void)
{
	setup_chosen_mb();
	setup_graph_solution();
	setup_self_node();
}

void
setup_test_get_provided_fds_n(void)
{
	setup_chosen_mb();
	setup_graph_solution();
}

void
setup_test_get_expected_fds_n(void)
{
	setup_chosen_mb();
	setup_graph_solution();
}

void
setup_test_get_origin_pid(void)
{
	setup_chosen_mb();
}

void
setup_test_read_input_fds(void)
{
	setup_chosen_mb();
	setup_graph_solution();
	setup_self_node();
}

void
setup_test_read_graph_solution(void)
{
	setup_mb(&fresh_mb);
	setup_chosen_mb();
	setup_self_node_io_side();
}

void
setup_test_read_concs(void)
{
	setup_mb(&fresh_mb);
	setup_chosen_mb();
	setup_concs(chosen_mb);
	setup_self_node_io_side();
}

void
setup_test_write_graph_solution(void)
{
	setup_chosen_mb();
	setup_graph_solution();
	setup_self_node_io_side();
}

void
setup_test_write_concs(void)
{
	setup_chosen_mb();
	setup_concs(chosen_mb);
	setup_self_node_io_side();
}

void
setup_test_read_message_block(void)
{
	setup_chosen_mb();
	setup_self_node_io_side();
}

void
setup_test_write_message_block(void)
{
	setup_chosen_mb();
	setup_self_node_io_side();
}

void
setup_test_make_compact_edge_array(void)
{
	setup_pointers_to_edges();
}

void
setup_test_reallocate_edge_pointer_array(void)
{
	setup_pointers_to_edges();
}

/*void
setup_test_assign_edge_instances(void)
{
	setup_chosen_mb();
	setup_pointers_to_edges();
}

void
setup_test_eval_constraints(void)
{
	setup_args();
}
*/

void
setup_test_move(void)
{
	setup_pointers_to_edges();
}

void
setup_test_satisfy_io_constraints(void)
{
	setup_chosen_mb();
	setup_pointers_to_edges();
	setup_args();
}

void
setup_test_dry_match_io_constraints(void)
{
	setup_chosen_mb();
	setup_graph_solution();
	setup_pointers_to_edges();
	setup_args();
}

void
setup_test_node_match_constraints(void)
{
	setup_chosen_mb();
}

void
setup_test_free_graph_solution(void)
{
	setup_chosen_mb();
	setup_graph_solution();
}

void
setup_test_solve_dgsh_graph(void)
{
	setup_chosen_mb();
	setup_graph_solution();
	setup_pointers_to_edges();
	setup_args();
}

void
setup_test_calculate_conc_fds(void)
{
	setup_chosen_mb();
	setup_graph_solution();
	setup_concs(chosen_mb);
}

void
setup_test_write_output_fds(void)
{
	setup_chosen_mb(); /* For setting up graph_solution. */
	setup_graph_solution();
	setup_self_node();
}

void
setup_test_set_dispatcher(void)
{
	setup_chosen_mb();
	setup_self_node_io_side();
}

void
setup_test_establish_io_connections(void)
{
	setup_pipe_fds();
	setup_chosen_mb();
	setup_self_node();
}

void setup_pi(void)
{
	pi = (struct portinfo *)calloc(5, sizeof(struct portinfo));
	pi[0].pid = 101;
	pi[0].seen = false;
	pi[0].written = true;
	pi[1].pid = 100;
	pi[1].seen = true;
	pi[1].written = false;
	pi[3].pid = 103;
	pi[3].seen = true;
	pi[3].written = true;
}

void
setup_test_is_ready(void)
{
	setup_pi();
	setup_chosen_mb();
}

void
setup_test_set_io_channels(void)
{
	setup_pi();
	setup_chosen_mb();
}

void
retire_pointers_to_edges(void)
{
	int i;
	for (i = 0; i < n_ptedges; i++)
		free(pointers_to_edges[i]);
	free(pointers_to_edges);
}

void
retire_graph_solution(struct dgsh_node_connections *graph_solution,
								int node_index)
{
	int i;
        for (i = 0; i <= node_index; i++) {
		if (graph_solution[i].n_edges_incoming)
                	free(graph_solution[i].edges_incoming);
		if (graph_solution[i].n_edges_outgoing)
                	free(graph_solution[i].edges_outgoing);
        }
        free(graph_solution);
}

void retire_concs(struct dgsh_negotiation *mb)
{
	int i;
	for (i = 0; i < mb->n_concs; i++)
		free(mb->conc_array[i].proc_pids);
	free(mb->conc_array);
}

void
retire_chosen_mb(void)
{
        free(chosen_mb->node_array);
        free(chosen_mb->edge_array);
        free(chosen_mb);
}

void
retire_mb(struct dgsh_negotiation *mb)
{
        free(mb->node_array);
        free(mb->edge_array);
        free(mb);
}

/* establish_io_connections() */
void
retire_pipe_fds(void)
{
	if (self_pipe_fds.n_input_fds > 0)
		free(self_pipe_fds.input_fds);
	if (self_pipe_fds.n_output_fds > 0)
		free(self_pipe_fds.output_fds);
	/* What about self_pipe_fds.input_fds? */
}

void
retire_args(void)
{
	free(args);
}

void
retire(void)
{
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	retire_chosen_mb();
	retire_pipe_fds();
}

void
retire_test_set_fds(void)
{
	retire_chosen_mb();
}

void
retire_test_construct_message_block(void)
{
	retire_chosen_mb();
}

void
retire_test_add_node(void)
{
	retire_chosen_mb();
}

void
retire_test_lookup_dgsh_edge(void)
{
	retire_chosen_mb();
}

void
retire_test_fill_dgsh_edge(void)
{
	retire_chosen_mb();
}

void
retire_test_add_edge(void)
{
	retire_chosen_mb();
}

void
retire_test_try_add_dgsh_edge(void)
{
	retire_chosen_mb();
}

void
retire_test_try_add_dgsh_node(void)
{
	retire_chosen_mb();
}

void
retire_test_analyse_read(void)
{
	if (exit_state == 1) {
		retire_mb(fresh_mb);
		exit_state = 0;
	} else retire_chosen_mb();
}

/*void
retire_test_point_io_direction(void)
{
	retire_chosen_mb();
}*/

void
retire_test_alloc_copy_graph_solution(void)
{
	retire_mb(fresh_mb);
}

void
retire_test_alloc_copy_concs(void)
{
	retire_concs(fresh_mb);
	retire_mb(fresh_mb);
}

void
retire_test_alloc_copy_edges(void)
{
	retire_mb(fresh_mb);
}

void
retire_test_alloc_copy_nodes(void)
{
	retire_mb(fresh_mb);
}

void
retire_test_alloc_io_fds(void)
{
	retire_pipe_fds();
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	retire_chosen_mb();
}

void
retire_test_get_provided_fds_n(void)
{
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	retire_chosen_mb();
}

void
retire_test_get_expected_fds_n(void)
{
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	retire_chosen_mb();
}

void
retire_test_read_input_fds(void)
{
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	retire_chosen_mb();
}

void
retire_test_get_origin_pid(void)
{
	retire_chosen_mb();
}

void
retire_test_read_message_block(void)
{
	retire_chosen_mb();
}

void
retire_test_write_message_block(void)
{
	retire_chosen_mb();
}

void
retire_test_read_graph_solution(void)
{
	retire_chosen_mb();
	retire_mb(fresh_mb);
}

void
retire_test_read_concs(void)
{
	retire_concs(chosen_mb);
	retire_chosen_mb();
	retire_mb(fresh_mb);
}

void
retire_test_write_graph_solution(void)
{
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	retire_chosen_mb();
}

void
retire_test_write_concs(void)
{
	retire_concs(chosen_mb);
	retire_chosen_mb();
}

void
retire_test_make_compact_edge_array(void)
{
	free(compact_edges);
	retire_pointers_to_edges();
}

void
retire_test_reallocate_edge_pointer_array(void)
{
	retire_pointers_to_edges();
}

/*void
retire_test_assign_edge_instances(void)
{
	retire_chosen_mb();
	retire_pointers_to_edges();
}

void
retire_test_eval_constraints(void)
{
        retire_args();
}
*/

void
retire_test_move(void)
{
	retire_pointers_to_edges();
}

void
retire_test_satisfy_io_constraints(void)
{
	retire_chosen_mb();
	retire_pointers_to_edges();
	retire_args();
}

void
retire_test_dry_match_io_constraints(void)
{
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	retire_chosen_mb();
	retire_pointers_to_edges();
	retire_args();
}

void
retire_test_node_match_constraints(void)
{
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	retire_chosen_mb();
}

void
retire_test_free_graph_solution(void)
{
	retire_chosen_mb();
}

void
retire_test_solve_dgsh_graph(void)
{
	/* Are the other data structures handled correctly?
	 * They could be deallocated above our feet.
	 */
	if (!exit_state) 
		retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	else exit_state = 0;
	retire_chosen_mb();
	retire_pointers_to_edges();
	retire_args();
}

void
retire_test_calculate_conc_fds(void)
{
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
	retire_concs(chosen_mb);
	retire_chosen_mb();
}

void
retire_test_write_output_fds(void)
{
	retire_graph_solution(chosen_mb->graph_solution,
			chosen_mb->n_nodes - 1);
}

void
retire_test_set_dispatcher(void)
{
	retire_chosen_mb();
}

void
retire_test_establish_io_connections(void)
{
	/* See setup_test_establish_io_connections() */
	retire_chosen_mb();
	retire_pipe_fds();
}

void
retire_pi(void)
{
	free(pi);
}

void
retire_test_is_ready(void)
{
	retire_pi();
	retire_chosen_mb();
}

void
retire_test_set_io_channels(void)
{
	retire_concs(chosen_mb);
	retire_chosen_mb();
	retire_pi();
}

START_TEST(test_solve_dgsh_graph)
{
	DPRINTF("%s", __func__);
        /* A normal case with fixed, tight constraints. */
	ck_assert_int_eq(solve_dgsh_graph(), OP_SUCCESS);
	struct dgsh_node_connections *graph_solution =
		chosen_mb->graph_solution;
	ck_assert_int_eq(graph_solution[3].n_edges_incoming, 2);
	ck_assert_int_eq(graph_solution[3].n_edges_outgoing, 0);
	ck_assert_int_eq(chosen_mb->edge_array[3].instances, 1);
	ck_assert_int_eq(chosen_mb->edge_array[4].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[0].instances, 1);
	ck_assert_int_eq(graph_solution[0].edges_outgoing[0].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[1].instances, 1);
	ck_assert_int_eq(graph_solution[1].edges_outgoing[1].instances, 1);
	ck_assert_int_eq((long int)graph_solution[3].edges_outgoing, 0);
	retire_test_solve_dgsh_graph();

	/* An impossible case. */
	setup_test_solve_dgsh_graph();
	chosen_mb->node_array[3].requires_channels = 1;
	ck_assert_int_eq(solve_dgsh_graph(), OP_ERROR);
	exit_state = 1;
	retire_test_solve_dgsh_graph();

	/* Relaxing our target node's constraint. */
	setup_test_solve_dgsh_graph();
	chosen_mb->node_array[3].requires_channels = -1;
	ck_assert_int_eq(solve_dgsh_graph(), OP_SUCCESS);
	graph_solution = chosen_mb->graph_solution;
	ck_assert_int_eq(graph_solution[3].n_edges_incoming, 2);
	ck_assert_int_eq(graph_solution[3].n_edges_outgoing, 0);
	/* Pair edges still have tight constraints. */
	ck_assert_int_eq(chosen_mb->edge_array[3].instances, 1);
	ck_assert_int_eq(chosen_mb->edge_array[4].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[0].instances, 1);
	ck_assert_int_eq(graph_solution[0].edges_outgoing[0].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[1].instances, 1);
	ck_assert_int_eq(graph_solution[1].edges_outgoing[1].instances, 1);
	ck_assert_int_eq((long int)graph_solution[3].edges_outgoing, 0);
	retire_test_solve_dgsh_graph();

	/* Relaxing also pair nodes' constraints. */
	setup_test_solve_dgsh_graph();
	chosen_mb->node_array[3].requires_channels = -1;
	chosen_mb->node_array[0].provides_channels = -1;
	chosen_mb->node_array[1].provides_channels = -1;
	ck_assert_int_eq(solve_dgsh_graph(), OP_SUCCESS);
	graph_solution = chosen_mb->graph_solution;
	ck_assert_int_eq(graph_solution[3].n_edges_incoming, 2);
	ck_assert_int_eq(graph_solution[3].n_edges_outgoing, 0);
	/* Flexible both sides: instances previously set to 5 */
	ck_assert_int_eq(chosen_mb->edge_array[3].instances, 1);
	ck_assert_int_eq(chosen_mb->edge_array[4].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[0].instances, 1);
	ck_assert_int_eq(graph_solution[0].edges_outgoing[0].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[1].instances, 1);
	ck_assert_int_eq(graph_solution[1].edges_outgoing[1].instances, 1);
	ck_assert_int_eq((long int)graph_solution[3].edges_outgoing, 0);
	/* Collateral impact. Node 1 (flex) -> Node 0 (tight) */
	ck_assert_int_eq(chosen_mb->edge_array[2].instances, 1);
	ck_assert_int_eq(graph_solution[1].edges_outgoing[0].instances, 1);
}
END_TEST

START_TEST(test_calculate_conc_fds)
{
	DPRINTF("%s()", __func__);
	chosen_mb->conc_array[0].input_fds = -1;
	chosen_mb->conc_array[0].output_fds = -1;
	chosen_mb->conc_array[1].input_fds = -1;
	chosen_mb->conc_array[1].output_fds = -1;
	struct dgsh_node_connections *graph_solution =
			chosen_mb->graph_solution;
	graph_solution[0].edges_incoming[0].instances = 1;
	graph_solution[0].edges_outgoing[0].instances = 1;
	graph_solution[1].edges_incoming[0].instances = 1;
	graph_solution[1].edges_outgoing[0].instances = 1;
	/* endpoint for conc with pid 2001*/
	graph_solution[3].edges_incoming[0].instances = 1;
	graph_solution[3].edges_incoming[1].instances = 1;
	/* endpoint for conc with pid 2000*/
	graph_solution[2].edges_outgoing[0].instances = 1;
	graph_solution[2].edges_outgoing[1].instances = 1;

	ck_assert_int_eq(calculate_conc_fds(), OP_SUCCESS);
}
END_TEST

START_TEST(test_free_graph_solution)
{
	ck_assert_int_eq(free_graph_solution(3), OP_SUCCESS);
	/* Invalid node indexes, the function's argument, are checked by assertion. */
}
END_TEST


START_TEST(test_establish_io_connections)
{
	/* Should be in the solution propagation test suite. */
	/* The test case contains an arrangement of 0 fds and another of >0 fds. */
	int *input_fds = NULL;
	int n_input_fds = 2; 
	int *output_fds = NULL;
	int n_output_fds = 0;
	int fd[2];

	if (pipe(fd) == -1) {		/* fd pair: 4 -- 5 */
		perror("pipe open failed");
		exit(1);
	}
	self_pipe_fds.input_fds[0] = fd[0];
	DPRINTF("%s: Opened pipe pair: input_fds[0]: %d, output: %d",
			__func__, fd[0], fd[1]);

	ck_assert_int_eq(establish_io_connections(NULL, NULL, NULL, NULL),
			OP_SUCCESS);
	/* Freed */
	ck_assert_int_eq(self_pipe_fds.n_input_fds, 0);
	ck_assert_int_eq(self_pipe_fds.n_output_fds, 0);
	close(fd[1]);
	retire_test_establish_io_connections();

	setup_test_establish_io_connections();
	if (pipe(fd) == -1) {		/* fd pair: 4 -- 5 */
		perror("pipe open failed");
		exit(1);
	}
	self_pipe_fds.input_fds[0] = fd[0];
	DPRINTF("%s: Opened pipe pair: input_fds[0]: %d, output: %d",
			__func__, fd[0], fd[1]);
	self_pipe_fds.input_fds[1] = 6;
	ck_assert_int_eq(establish_io_connections(&input_fds, &n_input_fds,
					&output_fds, &n_output_fds), OP_SUCCESS);
	ck_assert_int_eq(n_input_fds, 2);
	ck_assert_int_eq(input_fds[0], 0);
	ck_assert_int_eq(input_fds[1], 6);
	ck_assert_int_eq(n_output_fds, 0);
	close(fd[1]);
}
END_TEST

struct dgsh_edge **edges_in;
int n_edges_in;
struct dgsh_edge **edges_out;
int n_edges_out;

void
retire_dmic(void)
{
	retire();
	if (n_edges_in)
		free(edges_in);
	if (n_edges_out)
		free(edges_out);
}

START_TEST(test_node_match_constraints)
{
	DPRINTF("%s()\n", __func__);

	/* Default topology; take a look at setup_chosen_mb() */
	chosen_mb->node_array[3].requires_channels = 2;
	ck_assert_int_eq(node_match_constraints(), OP_SUCCESS);
	struct dgsh_node_connections *graph_solution =
		chosen_mb->graph_solution;
	ck_assert_int_eq(graph_solution[0].node_index, 0);
	ck_assert_int_eq(graph_solution[0].n_edges_incoming, 2);
	ck_assert_int_eq(graph_solution[0].n_instances_incoming_free, 0);
	ck_assert_int_eq(graph_solution[0].n_edges_outgoing, 1);
	ck_assert_int_eq(graph_solution[0].n_instances_outgoing_free, 0);

	ck_assert_int_eq(graph_solution[1].node_index, 1);
	ck_assert_int_eq(graph_solution[1].n_edges_incoming, 1);
	ck_assert_int_eq(graph_solution[1].n_instances_incoming_free, 0);
	ck_assert_int_eq(graph_solution[1].n_edges_outgoing, 2);
	ck_assert_int_eq(graph_solution[1].n_instances_outgoing_free, 0);

	ck_assert_int_eq(graph_solution[2].node_index, 2);
	ck_assert_int_eq(graph_solution[2].n_edges_incoming, 0);
	ck_assert_int_eq(graph_solution[2].n_instances_incoming_free, 0);
	ck_assert_int_eq(graph_solution[2].n_edges_outgoing, 2);
	ck_assert_int_eq(graph_solution[2].n_instances_outgoing_free, 0);

	ck_assert_int_eq(graph_solution[3].node_index, 3);
	ck_assert_int_eq(graph_solution[3].n_edges_incoming, 2);
	ck_assert_int_eq(graph_solution[3].n_instances_incoming_free, 0);
	ck_assert_int_eq(graph_solution[3].n_edges_outgoing, 0);
	ck_assert_int_eq(graph_solution[3].n_instances_outgoing_free, 0);
}
END_TEST
	
START_TEST(test_dry_match_io_constraints)
{
	DPRINTF("%s", __func__);

	struct dgsh_node_connections *graph_solution =
		chosen_mb->graph_solution;
        /* A normal case with fixed, tight constraints. */
	struct dgsh_node_connections *current_connections = &graph_solution[3];
	current_connections->n_edges_incoming = 0;
	current_connections->n_edges_outgoing = 0;
	ck_assert_int_eq(dry_match_io_constraints(&chosen_mb->node_array[3],
		current_connections, &edges_in, &edges_out), OP_SUCCESS);
        /* Hard coded. Observe the topology of the prototype solution in setup(). */
	ck_assert_int_eq(current_connections->n_edges_incoming, 2);
	ck_assert_int_eq(current_connections->n_edges_outgoing, 0);

	/* A case not matching at first sight; match result will
	 * be decided in cross_match_constraints() */
	current_connections->n_edges_incoming = 0;
	current_connections->n_edges_outgoing = 0;
	chosen_mb->node_array[3].requires_channels = 3;
	ck_assert_int_eq(dry_match_io_constraints(&chosen_mb->node_array[3],
				current_connections,
			&edges_in, &edges_out), OP_SUCCESS);
	ck_assert_int_eq(current_connections->n_edges_incoming, 2);
	ck_assert_int_eq(current_connections->n_edges_outgoing, 0);

	/* Relaxing our target node's constraint. */
	current_connections->n_edges_incoming = 0;
	current_connections->n_edges_outgoing = 0;
	chosen_mb->node_array[3].requires_channels = -1;
	ck_assert_int_eq(dry_match_io_constraints(&chosen_mb->node_array[3],
				current_connections,
			&edges_in, &edges_out), OP_SUCCESS);
	ck_assert_int_eq(current_connections->n_edges_incoming, 2);
	ck_assert_int_eq(current_connections->n_edges_outgoing, 0);
	ck_assert_int_eq(chosen_mb->edge_array[3].to_instances, -1);
	ck_assert_int_eq(chosen_mb->edge_array[4].to_instances, -1);

}
END_TEST

START_TEST(test_satisfy_io_constraints)
{
        /* To be concise, when changing the second argument that mirrors
         * the channel constraint of the node under evaluation, we should
         * also change it in the node array, but it does not matter since it
         * is the pair nodes that we are interested in.
         */
	int free_instances = 0;
        /* Fixed constraint both sides, just satisfy. */
	ck_assert_int_eq(satisfy_io_constraints(&free_instances, 
				2, pointers_to_edges, 2, true), OP_SUCCESS);
	ck_assert_int_eq(free_instances, 0);
        /* Fixed constraint both sides, not matching at
	 * first sight, but will leave it to cross_match_constraints()
	 * to decide */
	ck_assert_int_eq(satisfy_io_constraints(&free_instances,
				1, pointers_to_edges, 2, true), OP_SUCCESS);
	ck_assert_int_eq(free_instances, 0);
        /* Fixed constraint bith sides, plenty. */
	ck_assert_int_eq(satisfy_io_constraints(&free_instances,
				5, pointers_to_edges, 2, true), OP_SUCCESS);
	ck_assert_int_eq(free_instances, 0);
        /* Fixed constraint node, flexible pair, just one. */
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(satisfy_io_constraints(&free_instances,
				2, pointers_to_edges, 2, true), OP_SUCCESS);
	ck_assert_int_eq(free_instances, 0);
        /* Fixed constraint node, flexible pair,
	 * cross_match_constraints() will decide */
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(satisfy_io_constraints(&free_instances,
				1, pointers_to_edges, 2, true), OP_SUCCESS);
	ck_assert_int_eq(free_instances, 0);
	retire_test_satisfy_io_constraints();

        /* Expand the semantics of remaining_free_channels to fixed constraints
           as in this case. */  
        /* Fixed constraint node, flexible pair, plenty. */
	setup_test_satisfy_io_constraints();
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(satisfy_io_constraints(&free_instances,
				5, pointers_to_edges, 2, true), OP_SUCCESS);
	ck_assert_int_eq(free_instances, 0);
	free_instances = 0;
	retire_test_satisfy_io_constraints();

        /* Flexible constraint both sides */
	setup_test_satisfy_io_constraints();
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(satisfy_io_constraints(&free_instances,
				-1, pointers_to_edges, 2, 1), OP_SUCCESS);
	ck_assert_int_eq(free_instances, -1);
	free_instances = 0;
}
END_TEST

START_TEST(test_move)
{
	int diff = 1;
	bool is_edge_incoming = true;
	DPRINTF("%s()", __func__);
	pointers_to_edges[0]->from_instances = 1;
	pointers_to_edges[0]->to_instances = 2;
	pointers_to_edges[1]->from_instances = 2;
	pointers_to_edges[1]->to_instances = 1;
	ck_assert_int_eq(move(pointers_to_edges, n_ptedges, diff, is_edge_incoming),
			OP_SUCCESS);
	ck_assert_int_eq(pointers_to_edges[0]->from_instances, 1);
	ck_assert_int_eq(pointers_to_edges[0]->to_instances, 2);
	ck_assert_int_eq(pointers_to_edges[1]->from_instances, 2);
	ck_assert_int_eq(pointers_to_edges[1]->to_instances, 2);
}
END_TEST

START_TEST(test_record_move_flexible)
{
	/* Successful increase move */
	int diff = 1;
	int index = -1;
	int to_move_index = 2;
	int instances = 0;
	int to_move = 2;
	record_move_flexible(&diff, &index, to_move_index,
			&instances, to_move);
	ck_assert_int_eq(diff, 0);
	ck_assert_int_eq(index, to_move_index);
	ck_assert_int_eq(instances, 1);

	/* Can't subtract instances from size 1 */
	diff = -1;
	index = -1;
	to_move_index = 2;
	instances = 0;
	to_move = 1;
	record_move_flexible(&diff, &index, to_move_index,
			&instances, to_move);
	ck_assert_int_eq(diff, -1);
	ck_assert_int_eq(index, -1);
	ck_assert_int_eq(instances, 0);

	/* Successful decrease. diff greater than to_move */
	diff = -3;
	index = -1;
	to_move_index = 2;
	instances = 0;
	to_move = 2;
	record_move_flexible(&diff, &index, to_move_index,
			&instances, to_move);
	ck_assert_int_eq(diff, -2);
	ck_assert_int_eq(index, to_move_index);
	ck_assert_int_eq(instances, -1);

	/* Successful decrease. diff smaller than to_move */
	diff = -2;
	index = -1;
	to_move_index = 2;
	instances = 0;
	to_move = 4;
	record_move_flexible(&diff, &index, to_move_index,
			&instances, to_move);
	ck_assert_int_eq(diff, 0);
	ck_assert_int_eq(index, to_move_index);
	ck_assert_int_eq(instances, -2);

}
END_TEST

START_TEST(test_record_move_unbalanced)
{
	/* Successful increase move */
	int diff = 1;
	int index = -1;
	int to_move_index = 2;
	int instances = 0;
	int to_move = 2;
	int pair = 3;
	record_move_unbalanced(&diff, &index, to_move_index,
			&instances, to_move, pair);
	ck_assert_int_eq(diff, 0);
	ck_assert_int_eq(index, to_move_index);
	ck_assert_int_eq(instances, 1);

	/* Successful decrease. diff greater than to_move - pair */
	diff = -3;
	index = -1;
	to_move_index = 2;
	instances = 0;
	to_move = 2;
	pair = 1;
	record_move_unbalanced(&diff, &index, to_move_index,
			&instances, to_move, pair);
	ck_assert_int_eq(diff, -2);
	ck_assert_int_eq(index, to_move_index);
	ck_assert_int_eq(instances, -1);

	/* Successful decrease. diff smaller than to_move - pair */
	diff = -2;
	index = -1;
	to_move_index = 2;
	instances = 0;
	to_move = 4;
	pair = 1;
	record_move_unbalanced(&diff, &index, to_move_index,
			&instances, to_move, pair);
	ck_assert_int_eq(diff, 0);
	ck_assert_int_eq(index, to_move_index);
	ck_assert_int_eq(instances, -2);

}
END_TEST

START_TEST(test_reallocate_edge_pointer_array)
{
	ck_assert_int_eq(reallocate_edge_pointer_array(NULL, 1), OP_ERROR);
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, -2), OP_ERROR);
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, 0), OP_ERROR);
	/* Not incresing the value of n_ptedges to not perplex freeing 
         * pointers_to_edges because reallocation only accounts for
         * struct dgsh_edge *.
         */
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, n_ptedges + 1), OP_SUCCESS);
}
END_TEST

START_TEST(test_make_compact_edge_array)
{
	ck_assert_int_eq(make_compact_edge_array(NULL, 2, pointers_to_edges), OP_ERROR);
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, -2, pointers_to_edges), OP_ERROR);
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, 0, pointers_to_edges), OP_ERROR);
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, n_ptedges, NULL), OP_ERROR);

	struct dgsh_edge *p = pointers_to_edges[0];
	pointers_to_edges[0] = NULL;
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, n_ptedges, pointers_to_edges), OP_ERROR);

	pointers_to_edges[0] = p;
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, n_ptedges, pointers_to_edges), OP_SUCCESS);
}
END_TEST

START_TEST(test_write_output_fds)
{
	int write_fd = 1;
	int *output_fds;
	/* 0 outgoing edges for node 3, so no action really. */
	ck_assert_int_eq(write_output_fds(write_fd, output_fds), OP_SUCCESS);

	/* Switch to node 2 that has 2 outgoing edges. */
	memcpy(&self_node, &chosen_mb->node_array[2], sizeof(struct dgsh_node));
	output_fds = (int *)malloc(sizeof(int) * 2);
	ck_assert_int_eq(write_output_fds(write_fd, output_fds), OP_SUCCESS);
	free(output_fds);

	/* Incomplete testing since socket descriptors have not yet been setup.
	 * This will hapeen through the shell.
	 */
}
END_TEST

START_TEST(test_set_dispatcher)
{
	set_dispatcher();
	ck_assert_int_eq(chosen_mb->origin_index, 3);
	ck_assert_int_eq(chosen_mb->origin_fd_direction, 0); /* The input side */
}
END_TEST

START_TEST(test_alloc_node_connections)
{
	struct dgsh_edge *test;
	/* It is assumed that negative number of edges have already
         * been checked. See e.g. read_graph_solution().
         */
	ck_assert_int_eq(alloc_node_connections(NULL, 2, 1, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, 2, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, -1, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, 1, -2), OP_ERROR);

	ck_assert_int_eq(alloc_node_connections(&test, 1, 1, 2), OP_SUCCESS);
	free(test);
}
END_TEST

START_TEST(test_write_concs)
{
	int fd[2];
	int buf_size = getpagesize();
	int pid;
	int i;
        int n_concs = chosen_mb->n_concs;
        int concs_size = sizeof(struct dgsh_conc) * n_concs;
	struct dgsh_conc *conc_array = 
		(struct dgsh_conc *)malloc(concs_size);

	if (pipe(fd) == -1) {
		perror("pipe open failed");
		exit(1);
	}
	DPRINTF("%s()...", __func__);
	DPRINTF("Opened pipe pair %d - %d.", fd[0], fd[1]);

	pid = fork();
	if (pid <= 0) {
		int rsize = -1;
		DPRINTF("Child speaking with pid %d.", (int)getpid());

		close(fd[1]);
		DPRINTF("Child reads concs of size %d.",
					concs_size);
		rsize = read(fd[0], conc_array, concs_size);
		if (rsize == -1) {
			DPRINTF("Write concs failed.");
			exit(1);
		}

		DPRINTF("Child: closes fd %d.", fd[0]);
		close(fd[0]);
		DPRINTF("Child with pid %d exits.", (int)getpid());
	} else {
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(write_concs(fd[1]), OP_SUCCESS);
	}
}
END_TEST

/* Incomplete? */
START_TEST(test_write_graph_solution)
{
	int fd[2];
	int buf_size = getpagesize();
	int pid;
	int i;
        int n_nodes = chosen_mb->n_nodes;
        int graph_solution_size = sizeof(struct dgsh_node_connections) *
                                                                n_nodes;
	struct dgsh_node_connections *graph_solution = 
		(struct dgsh_node_connections *)malloc(graph_solution_size);

	if (pipe(fd) == -1) {
		perror("pipe open failed");
		exit(1);
	}
	DPRINTF("%s()...", __func__);
	DPRINTF("Opened pipe pair %d - %d.", fd[0], fd[1]);

	pid = fork();
	if (pid <= 0) {
		int rsize = -1;
		DPRINTF("Child speaking with pid %d.", (int)getpid());

		close(fd[1]);
		DPRINTF("Child reads graph solution of size %d.",
					graph_solution_size);
        	rsize = read(fd[0], graph_solution, graph_solution_size);
		if (rsize == -1) {
			DPRINTF("Write graph solution failed.");
			exit(1);
		}

		for (i = 0; i < chosen_mb->n_nodes; i++) {
                	struct dgsh_node_connections *nc = &graph_solution[i];
                	int in_edges_size = sizeof(struct dgsh_edge) *
							nc->n_edges_incoming;
                	int out_edges_size = sizeof(struct dgsh_edge) *
							nc->n_edges_outgoing;
                	if ((in_edges_size > buf_size) || 
						(out_edges_size > buf_size)) {
                        	DPRINTF("Dgsh negotiation graph solution for node at index %d: incoming connections of size %d or outgoing connections of size %d do not fit to buffer of size %d.\n", nc->node_index, in_edges_size, out_edges_size, buf_size);
                        	exit(1);
                	}

			DPRINTF("Child reads incoming edges of node %d in fd %d. Total size: %d", i, fd[1], in_edges_size);
                	/* Transmit a node's incoming connections. */
                	rsize = read(fd[0], nc->edges_incoming, in_edges_size);
			if (rsize == -1) {
				DPRINTF("Read edges incoming failed.");
				exit(1);
			}

			DPRINTF("Child reads outgoing edges of node %d in fd %d. Total size: %d", i, fd[1], out_edges_size);
                	/* Transmit a node's outgoing connections. */
                	rsize = read(fd[0], nc->edges_outgoing, out_edges_size);
			if (rsize == -1) {
				DPRINTF("Read edges outgoing failed.");
				exit(1);
			}
        	}
		DPRINTF("Child: closes fd %d.", fd[0]);
		close(fd[0]);
		DPRINTF("Child with pid %d exits.", (int)getpid());
	} else {
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(write_graph_solution(fd[1]), OP_SUCCESS);
	}
}
END_TEST

/* Incomplete? */
START_TEST(test_write_message_block)
{
	int fd[2];
	int pid;
	DPRINTF("%s()", __func__);

	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}
	DPRINTF("Opened pipe pair %d - %d.", fd[0], fd[1]);	

	pid = fork();
	if (pid <= 0) {
		DPRINTF("Child speaking with pid %d.", (int)getpid());
		struct dgsh_negotiation *test_mb = (struct dgsh_negotiation *)
				malloc(sizeof(struct dgsh_negotiation));
        	int mb_struct_size = sizeof(struct dgsh_negotiation);
                int i = 0;
		int rsize = -1;

		close(fd[1]);
		DPRINTF("Child reads message block structure of size %d.",
					mb_struct_size);
        	rsize = read(fd[0], test_mb, mb_struct_size);
		if (rsize == -1) {
			DPRINTF("Read message block failed.");
			exit(1);
		}
        	int n_nodes = test_mb->n_nodes;
        	int n_edges = test_mb->n_edges;
        	int mb_nodes_size = sizeof(struct dgsh_node) * n_nodes;
		test_mb->node_array = (struct dgsh_node *)malloc(mb_nodes_size);
        	int mb_edges_size = sizeof(struct dgsh_edge) * n_edges;
		test_mb->edge_array = (struct dgsh_edge *)malloc(mb_edges_size);

		DPRINTF("Child reads message block node array of size %d.",
					mb_nodes_size);
        	rsize = read(fd[0], test_mb->node_array, mb_nodes_size);
		if (rsize == -1) {
			DPRINTF("Read node array failed.");
			exit(1);
		}

		DPRINTF("Child reads message block edge array of size %d.",
					mb_edges_size);
		rsize = read(fd[0], test_mb->edge_array, mb_edges_size);
		if (rsize == -1) {
			DPRINTF("Read edge array failed.");
			exit(1);
		}
                for (i = 0; i < test_mb->n_edges; i++) {
                        struct dgsh_edge *e = &test_mb->edge_array[i];
                        DPRINTF("Edge from: %d, to: %d", e->from, e->to);
                }

		DPRINTF("Child: closes fd %d.", fd[0]);
		close(fd[0]);
		DPRINTF("Child with pid %d exits.", (int)getpid());
		retire_mb(test_mb);
	} else {
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(write_message_block(fd[1]), OP_SUCCESS);
	}
}
END_TEST

/* Incomplete? */
START_TEST(test_read_message_block)
{
	int fd[2];
	int pid;
	DPRINTF("%s()", __func__);

	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}
	DPRINTF("Opened pipe pair %d - %d.", fd[0], fd[1]);

	pid = fork();
	if (pid <= 0) {
		DPRINTF("Child speaking with pid %d.", (int)getpid());
		struct dgsh_negotiation *test_mb;
		setup_mb(&test_mb);
        	int n_nodes = test_mb->n_nodes;
        	int n_edges = test_mb->n_edges;
        	int mb_struct_size = sizeof(struct dgsh_negotiation);
        	int mb_nodes_size = sizeof(struct dgsh_node) * n_nodes;
        	int mb_edges_size = sizeof(struct dgsh_edge) * n_edges;
                int i = 0;
		int wsize = -1;
		struct timespec tm;
		tm.tv_sec = 0;
		tm.tv_nsec = 1000000;

		close(fd[0]);
		DPRINTF("Child writes message block structure of size %d.",
					mb_struct_size);
        	wsize = write(fd[1], test_mb, mb_struct_size);
		if (wsize == -1) {
			DPRINTF("Write message block structure failed.");
			exit(1);
		}
		/* Sleep for 1 millisecond to write-read orderly.
		 * Why do we need this?
		 * Shouldn't the write block by deafult?
		 */
		nanosleep(&tm, NULL);

		DPRINTF("Child writes message block node array of size %d.",
					mb_nodes_size);
        	wsize = write(fd[1], test_mb->node_array, mb_nodes_size);
		if (wsize == -1) {
			DPRINTF("Write message block node array failed.");
			exit(1);
		}
		/* Sleep for 1 millisecond before the next operation. */
		nanosleep(&tm, NULL);

		DPRINTF("Child writes message block edge array of size %d.",
					mb_edges_size);
                for (i = 0; i < test_mb->n_edges; i++) {
                        struct dgsh_edge *e = &test_mb->edge_array[i];
                        DPRINTF("Edge from: %d, to: %d", e->from, e->to);
                }
		wsize = write(fd[1], test_mb->edge_array, mb_edges_size);
		if (wsize == -1) {
			DPRINTF("Write message block edge array failed.");
			exit(1);
		}

		DPRINTF("Child: closes fd %d.", fd[1]);
		close(fd[1]);
		DPRINTF("Child with pid %d exits.", (int)getpid());
		retire_mb(test_mb);
	} else {
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(read_message_block(fd[0], &fresh_mb),
				OP_SUCCESS);
	}
}
END_TEST

/* Incomplete? */
START_TEST(test_read_graph_solution)
{
	int fd[2];
	int pid;
	int i;
        int n_nodes = fresh_mb->n_nodes;
	int buf_size = getpagesize();
        int graph_solution_size = sizeof(struct dgsh_node_connections) *
                                                                n_nodes;
	struct timespec tm;
	tm.tv_sec = 0;
	tm.tv_nsec = 1000000;
	DPRINTF("%s()", __func__);

	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}
	DPRINTF("Opened pipe pair %d - %d.", fd[0], fd[1]);	

	pid = fork();
	if (pid <= 0) {
		int wsize = -1;
		DPRINTF("Child speaking with pid %d.", (int)getpid());
		setup_graph_solution();
		struct dgsh_node_connections *graph_solution =
			chosen_mb->graph_solution;

		close(fd[0]);
		DPRINTF("Child writes graph solution of size %d.",
					graph_solution_size);
        	wsize = write(fd[1], graph_solution, graph_solution_size);
		if (wsize == -1) {
			DPRINTF("Write graph solution failed.");
			exit(1);
		}
		/* Sleep for 1 millisecond before the next operation. */
		nanosleep(&tm, NULL);

		for (i = 0; i < chosen_mb->n_nodes; i++) {
                	struct dgsh_node_connections *nc = &graph_solution[i];
                	int in_edges_size = sizeof(struct dgsh_edge) *
							nc->n_edges_incoming;
                	int out_edges_size = sizeof(struct dgsh_edge) *
							nc->n_edges_outgoing;
			int wsize = -1;
			if ((in_edges_size > buf_size) || 
						(out_edges_size > buf_size)) {
                        	DPRINTF("Dgsh negotiation graph solution for node at index %d: incoming connections of size %d or outgoing connections of size %d do not fit to buffer of size %d.\n", nc->node_index, in_edges_size, out_edges_size, buf_size);
                        	exit(1);
                	}

			DPRINTF("Child writes incoming edges of node %d in fd %d. Total size: %d", i, fd[1], in_edges_size);
                	/* Transmit a node's incoming connections. */
                	wsize = write(fd[1], nc->edges_incoming, in_edges_size);
			if (wsize == -1) {
				DPRINTF("Write edges incoming failed.");
				exit(1);
			}
			/* Sleep for 1 millisecond before the next operation. */
			nanosleep(&tm, NULL);

			DPRINTF("Child writes outgoing edges of node %d in fd %d. Total size: %d", i, fd[1], out_edges_size);
                	/* Transmit a node's outgoing connections. */
                	wsize = write(fd[1], nc->edges_outgoing, out_edges_size);
			if (wsize == -1) {
				DPRINTF("Write edges outgoing failed.");
				exit(1);
			}
			/* Sleep for 1 millisecond before the next operation. */
			nanosleep(&tm, NULL);
        	}
		DPRINTF("Child: closes fd %d.", fd[1]);
		close(fd[1]);
		DPRINTF("Child with pid %d exits.", (int)getpid());
		retire_graph_solution(graph_solution, chosen_mb->n_nodes - 1);
	} else {
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(read_graph_solution(fd[0],
					fresh_mb), OP_SUCCESS);
	}
}
END_TEST

START_TEST(test_read_concs)
{
	int fd[2];
	int pid;
	int i;
        int n_concs = fresh_mb->n_concs;
	int buf_size = getpagesize();
        int concs_size = sizeof(struct dgsh_conc) * n_concs;
	DPRINTF("%s()", __func__);

	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}
	DPRINTF("Opened pipe pair %d - %d.", fd[0], fd[1]);

	pid = fork();
	if (pid <= 0) {
		int wsize = -1;
		DPRINTF("Child speaking with pid %d.", (int)getpid());
		setup_graph_solution();
		struct dgsh_conc *concs =
			chosen_mb->conc_array;

		close(fd[0]);
		DPRINTF("Child writes concs of size %d.",
					concs_size);
		wsize = write(fd[1], concs, concs_size);
		if (wsize == -1) {
			DPRINTF("Write concs failed.");
			exit(1);
		}
	} else {
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(read_concs(fd[0],
					fresh_mb), OP_SUCCESS);
	}
}
END_TEST

START_TEST(test_alloc_fds)
{
	int *fds = NULL;
	int n_fds = 0;
	ck_assert_int_eq(alloc_fds(&fds, n_fds), OP_SUCCESS);
	ck_assert_int_eq((int)(long)fds, 0);

	n_fds = 2;
	ck_assert_int_eq(alloc_fds(&fds, n_fds), OP_SUCCESS);
	ck_assert_int_ne((int)(long)fds, 0);
	free(fds);
}
END_TEST

START_TEST(test_alloc_io_fds)
{
	/* By default initialised to 0. See setup_chosen_mb() */
	chosen_mb->graph_solution[3].edges_incoming[0].instances = 1;
	chosen_mb->graph_solution[3].edges_incoming[1].instances = 1;
	ck_assert_int_eq(alloc_io_fds(), OP_SUCCESS);
	ck_assert_int_eq(self_pipe_fds.n_input_fds, 2);
	ck_assert_int_eq(self_pipe_fds.n_output_fds, 0);
}
END_TEST

START_TEST(test_get_origin_pid)
{
	chosen_mb->origin_index = 3;
	ck_assert_int_eq(get_origin_pid(chosen_mb), 103);

	chosen_mb->origin_index = 1;
	ck_assert_int_eq(get_origin_pid(chosen_mb), 101);
}
END_TEST

START_TEST(test_get_expected_fds_n)
{
	struct dgsh_node_connections *graph_solution =
			chosen_mb->graph_solution;
	graph_solution[0].edges_incoming[0].instances = 1;
	graph_solution[1].edges_incoming[0].instances = 1;
	graph_solution[3].edges_incoming[0].instances = 1;
	graph_solution[3].edges_incoming[1].instances = 1;
	ck_assert_int_eq(get_expected_fds_n(chosen_mb, 103), 2);
	ck_assert_int_eq(get_expected_fds_n(chosen_mb, 100), 1);
	ck_assert_int_eq(get_expected_fds_n(chosen_mb, 101), 1);
	ck_assert_int_eq(get_expected_fds_n(chosen_mb, 102), 0);
}
END_TEST

START_TEST(test_get_provided_fds_n)
{
	struct dgsh_node_connections *graph_solution =
			chosen_mb->graph_solution;
	graph_solution[0].edges_outgoing[0].instances = 1;
	graph_solution[1].edges_outgoing[0].instances = 1;
	graph_solution[1].edges_outgoing[1].instances = 1;
	graph_solution[2].edges_outgoing[0].instances = 1;
	graph_solution[2].edges_outgoing[1].instances = 1;
	ck_assert_int_eq(get_provided_fds_n(chosen_mb, 103), 0);
	ck_assert_int_eq(get_provided_fds_n(chosen_mb, 100), 1);
	ck_assert_int_eq(get_provided_fds_n(chosen_mb, 101), 2);
	ck_assert_int_eq(get_provided_fds_n(chosen_mb, 102), 2);
}
END_TEST

START_TEST (test_read_write_fd)
{
	int pipefd[2];
	int sock, rsock;
	int readfd;
	char msg[] = "hello";
	char buff[20];
	int n;
	pid_t pid;
	socklen_t len;
	struct sockaddr_un local, remote;

	if (pipe(pipefd) == -1)
		err(1, "pipe");
	local.sun_family = AF_UNIX;
	snprintf(local.sun_path, sizeof(local.sun_path), "/tmp/conc-%d", getpid());
	len = strlen(local.sun_path) + 1 + sizeof(local.sun_family);

	switch ((pid = fork())) {
	case 0:
		/* Child: connect, pass fd and write test message */
		if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
			err(1, "socket");
		sleep(1);
		if (connect(sock, (struct sockaddr *)&local, len) == -1)
			err(1, "connect %s", local.sun_path);
		write_fd(sock, pipefd[STDIN_FILENO]);
		close(sock); // Should wait for data to be transmitted
		close(pipefd[STDIN_FILENO]);
		sleep(1);
		if (write(pipefd[STDOUT_FILENO], msg, sizeof(msg)) <= 0)
			err(1, "write");
		exit(0);
	default:
		/* Parent: accept connection, read fd and read test message */
		close(pipefd[STDIN_FILENO]);
		close(pipefd[STDOUT_FILENO]);
		if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
			err(1, "socket");
		if (bind(sock, (struct sockaddr *)&local, len) == -1)
			err(1, "bind %s", local.sun_path);
		if (listen(sock, 5) == -1)
			err(1, "listen");
		rsock = accept(sock, (struct sockaddr *)&remote, &len);
		readfd = read_fd(rsock);
		if ((n = read(readfd, buff, sizeof(buff))) == -1)
			err(1, "read");
		(void)unlink(local.sun_path);
		ck_assert_int_eq(n, sizeof(msg));
		ck_assert_str_eq(msg, buff);
		break;
	case -1:        /* Error */
		err(1, "fork");
	}

}
END_TEST

		
/* Incomplete? */
START_TEST(test_read_input_fds)
{
	int sockets[2];
	int fd, ping;
	struct msghdr msg;
	struct iovec vec[1];
	union fdmsg cmsg;
	struct cmsghdr *h;
	int wsize = -1;

	memset(&msg, 0, sizeof(struct msghdr));
	DPRINTF("%s()...pid %d", __func__, (int)getpid());

	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) < 0) {
		perror("Error opening stream socket pair. Exiting now.");
		exit(1);
	}
	DPRINTF("Opened socket pair %d - %d.", sockets[0], sockets[1]);

	fd = open("unit-test-dgsh", O_CREAT | O_RDWR, 0660);
	wsize = write(fd, "Unit testing dgsh...", 21);
	if (wsize == -1) {
		DPRINTF("Write to 'unit-test-dgsh' failed.");
		exit(1);
	}
        close(fd);
	fd = open("unit-test-dgsh", O_RDONLY);
	if (fd < 0) {
		perror("Failed to open file test-dgsh for reading.");
		exit(1);
	}

        int pid = fork();
	if (pid <= 0) {
		DPRINTF("Child speaking with pid %d.", (int)getpid());

		DPRINTF("Child closes socket %d.", sockets[1]);
		close(sockets[1]);

		vec[0].iov_base = &ping;
		vec[0].iov_len = 1;

		msg.msg_iov = vec;
		msg.msg_iovlen = 1;
		msg.msg_name = 0;
		msg.msg_namelen = 0;
		msg.msg_control = cmsg.buf;
		msg.msg_controllen = sizeof(union fdmsg);
		msg.msg_flags = 0;

		h = CMSG_FIRSTHDR(&msg);
		h->cmsg_level = SOL_SOCKET;
		h->cmsg_type = SCM_RIGHTS;
		h->cmsg_len = CMSG_LEN(sizeof(int));
		*((int*)CMSG_DATA(h)) = fd;

		DPRINTF("Child goes sendmsg()");
		if((sendmsg(sockets[0], &msg, 0)) < 0){
			perror("sendmsg()");
			exit(EXIT_FAILURE);
		}
		DPRINTF("Child: closes fd %d.", fd);
		close(fd);
		DPRINTF("Child with pid %d exits.", (int)getpid());
	} else {
		struct dgsh_node_connections *graph_solution =
			chosen_mb->graph_solution;
		memcpy(&self_node, &chosen_mb->node_array[1],
						sizeof(struct dgsh_node));
		/* Edges have been setup with 0 instances.
		 * See setup_chosen_mb().
	 	 * Edges then copied to graph_solution.
		 * See setup_graph_solution().
	 	 */
		graph_solution[1].edges_incoming[0].instances = 1;
		int *input_fds = (int *)malloc(sizeof(int));
		input_fds[0] = -1;
	
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(read_input_fds(sockets[1], input_fds),
				OP_SUCCESS);
		ck_assert_int_ge(input_fds[0], 3);
		/* Hard-coded ceiling to check whether some weird
		 * error has caused some random number to slip in.
		 */
		ck_assert_int_le(input_fds[0], 20);
		free(input_fds);
		DPRINTF("Parent with pid %d exits.", (int)getpid()); 
	}
	close(sockets[0]);
	close(sockets[1]);
}
END_TEST

/* Incomplete? */
START_TEST(test_read_chunk)
{
	/* Requires setting up of I/O multiplexing (a bash shell extension)
	 * in order to be able to support bidirectional negotiation.
	 * Without it we cannot test this function because it tries to read
	 * repeatedly from stdin and then stdout until it manages.
         * We cannot feed it; writing to stdin or stdout
	 * ends up in the unit test file output).
	 * The good news is that the core of this function is the call to
	 * call read, which has been successfully tested.
	 * Then there is variable checking to determine the exit code.
	 */
	int fd[2];
	int wsize = -1;
	DPRINTF("%s()...", __func__);
	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}

	/* Broken pipe error if we close the other side. */
	//close(fd[0]);
	wsize = write(fd[1], "test-in", 9);
	if (wsize == -1) {
		DPRINTF("Write to 'test-in' failed.");
		exit(1);
	}
	char buf[32];
	int read_fd = -1;
	int bytes_read = -1;
	ck_assert_int_eq(read_chunk(fd[0], buf, 32, &bytes_read, 5), OP_SUCCESS);
	ck_assert_int_eq(bytes_read, 9);
	
	close(fd[0]);
	close(fd[1]);
}
END_TEST

START_TEST(test_call_read)
{
	int fd[2];
	int wsize = -1;
	DPRINTF("%s()...", __func__);
	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}

	if ((wsize = write(fd[1], "test", 5)) == -1) {
		DPRINTF("Write to 'test' failed.\n");
		exit(1);
	}

	char buf[32];
	int bytes_read = -1;
	int error_code = -1;
	ck_assert_int_eq(call_read(fd[0], buf, 32, &bytes_read,
				&error_code), OP_SUCCESS);
	ck_assert_int_eq(bytes_read, 5);
	ck_assert_int_eq(error_code, 0);

	close(fd[0]);
	close(fd[1]);
}
END_TEST

START_TEST(test_alloc_copy_mb)
{
	const int size = sizeof(struct dgsh_negotiation);
	char buf[512];
	struct dgsh_negotiation *mb;
	ck_assert_int_eq(alloc_copy_mb(&mb, buf, 86, 512), OP_ERROR);

	char buf2[32];
	ck_assert_int_eq(alloc_copy_mb(&mb, buf2, size, 32), OP_ERROR);

	ck_assert_int_eq(alloc_copy_mb(&mb, buf, size, 512), OP_SUCCESS);
	free(mb);
}
END_TEST

START_TEST(test_alloc_copy_proc_pids)
{
	struct dgsh_conc c;
	c.n_proc_pids = 2;
	const int size = sizeof(int) * c.n_proc_pids;
	int pids[2] = {101, 103};
	char buf[512];
	memcpy(buf, pids, size);

	ck_assert_int_eq(alloc_copy_proc_pids(&c, buf, 86, 512),
			OP_ERROR);

	char buf2[8];
	ck_assert_int_eq(alloc_copy_proc_pids(&c, buf2, size, 4),
			OP_ERROR);

	ck_assert_int_eq(alloc_copy_proc_pids(&c, buf, size, 512),
			OP_SUCCESS);
}
END_TEST

START_TEST(test_alloc_copy_concs)
{
	const int n_concs = fresh_mb->n_concs = 1;
	const int size = sizeof(struct dgsh_conc) * n_concs;
	struct dgsh_conc c;
	char buf[512];
	memcpy(buf, &c, size);
	ck_assert_int_eq(alloc_copy_concs(fresh_mb, buf, 86, 512),
			OP_ERROR);

	char buf2[8];
	ck_assert_int_eq(alloc_copy_concs(fresh_mb, buf2, size, 8),
			OP_ERROR);

	ck_assert_int_eq(alloc_copy_concs(fresh_mb, buf, size, 512),
			OP_SUCCESS);
}
END_TEST

START_TEST(test_alloc_copy_nodes)
{
	const int size = sizeof(struct dgsh_node) * fresh_mb->n_nodes;
	char buf[512];
	ck_assert_int_eq(alloc_copy_nodes(fresh_mb, buf, 86, 512), OP_ERROR);

	char buf2[32];
	ck_assert_int_eq(alloc_copy_nodes(fresh_mb, buf2, size, 32), OP_ERROR);

	free(fresh_mb->node_array);  /* to avoid memory leak */
	ck_assert_int_eq(alloc_copy_nodes(fresh_mb, buf, size, 512), OP_SUCCESS);
}
END_TEST

START_TEST(test_alloc_copy_edges)
{
	const int size = sizeof(struct dgsh_edge) * fresh_mb->n_edges;
	char buf[256];
	ck_assert_int_eq(alloc_copy_edges(fresh_mb, buf, 86, 256), OP_ERROR);

	char buf2[32];
	ck_assert_int_eq(alloc_copy_edges(fresh_mb, buf2, size, 32), OP_ERROR);

	free(fresh_mb->edge_array);  /* to avoid memory leak */
	ck_assert_int_eq(alloc_copy_edges(fresh_mb, buf, size, 256), OP_SUCCESS);
}
END_TEST

START_TEST(test_alloc_copy_graph_solution)
{
	const int size = sizeof(struct dgsh_node_connections) *
		fresh_mb->n_nodes;
	char buf[256];
	ck_assert_int_eq(alloc_copy_graph_solution(fresh_mb, buf, 86, 256),
			OP_ERROR);

	char buf2[32];
	ck_assert_int_eq(alloc_copy_edges(fresh_mb, buf2, size, 32), OP_ERROR);

	/* Free to avoid memory leak.
	 * About the free() see how graph_solution is malloc'ed
	 * at alloc_copy_graph_solution.
	 */
	free(fresh_mb->graph_solution);
	ck_assert_int_eq(alloc_copy_graph_solution(fresh_mb, buf, size, 256),
			OP_SUCCESS);
	free(fresh_mb->graph_solution); /* Easier to handle here. */
}
END_TEST

START_TEST(test_check_read)
{
	ck_assert_int_eq(check_read(512, 1024, 256), OP_ERROR);
	ck_assert_int_eq(check_read(512, 256, 512), OP_ERROR);
	ck_assert_int_eq(check_read(512, 1024, 512), OP_SUCCESS);
}
END_TEST

/*
*START_TEST(test_point_io_direction)
{
	ck_assert_int_eq(point_io_direction(STDOUT_FILENO), STDIN_FILENO);

	memcpy(&self_node, &chosen_mb->node_array[2], sizeof(struct dgsh_node));
	ck_assert_int_eq(point_io_direction(STDIN_FILENO), STDOUT_FILENO);
	
}
END_TEST
*/

START_TEST(test_analyse_read)
{
	DPRINTF("%s()", __func__);
	/* error state flag seen; terminal process such as node 3
	 * which is the current node leave.
	 */
	bool should_transmit_mb = false;
	int serialno_ntimes_same = 0;
	int run_ntimes_same = 0;
	int error_ntimes_same = 0;
	fresh_mb->state = PS_ERROR;
	fresh_mb->is_error_confirmed = true;
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->state, PS_ERROR);
	ck_assert_int_eq(error_ntimes_same, 1);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	retire_test_analyse_read();

	setup_test_analyse_read();
	/* error state flag seen; non-terminal process such as node 1
	 * which is the current node leave when they see it the second time.
	 * This is the first.
	 */
	memcpy(&self_node, &chosen_mb->node_array[1], sizeof(struct dgsh_node));
	error_ntimes_same = 0;
	fresh_mb->state = PS_ERROR;
	fresh_mb->is_error_confirmed = true;
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->state, PS_ERROR);
	ck_assert_int_eq(error_ntimes_same, 1);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	retire_test_analyse_read();

	setup_test_analyse_read();
	/* error state flag seen; non-terminal process such as node 1
	 * which is the current node have to leave when they see it 
	 * the second time. This is the second.
	 * Before leaving they have to pass the block.
	 */
	memcpy(&self_node, &chosen_mb->node_array[1], sizeof(struct dgsh_node));
	error_ntimes_same = 1;
	fresh_mb->state = PS_ERROR;
	fresh_mb->is_error_confirmed = true;
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->state, PS_ERROR);
	ck_assert_int_eq(error_ntimes_same, 2);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	retire_test_analyse_read();

	setup_test_analyse_read();
	/* All processes have to pass the block first except
	 * for the ones who passed the block the last time
	 * before finding a solution.
	 */
	error_ntimes_same = 1;
	memcpy(&self_node, &chosen_mb->node_array[1], sizeof(struct dgsh_node));
	fresh_mb->state = PS_ERROR;
	fresh_mb->is_error_confirmed = true;
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->state, PS_ERROR);
	ck_assert_int_eq(error_ntimes_same, 2);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	retire_test_analyse_read();

	setup_test_analyse_read();
	/* run state flag seen; terminal process such as node 3
	 * which is the current node leave.
	 */
	run_ntimes_same = 0;
	error_ntimes_same = 0;
	fresh_mb->state = PS_RUN;
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->state, PS_RUN);
	ck_assert_int_eq(run_ntimes_same, 1);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	retire_test_analyse_read();

	setup_test_analyse_read();
	/* run state flag seen; non-terminal process such as node 1
	 * which is the current node leave when they see it the second time.
	 * This is the first.
	 */
	memcpy(&self_node, &chosen_mb->node_array[1], sizeof(struct dgsh_node));
	run_ntimes_same = 0;
	fresh_mb->state = PS_RUN;
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->state, PS_RUN);
	ck_assert_int_eq(run_ntimes_same, 1);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	retire_test_analyse_read();

	setup_test_analyse_read();
	/* run state flag seen; non-terminal process such as node 1
	 * which is the current node have to leave when they see it 
	 * the second time. This is the second.
	 * Before leaving they have to pass the block.
	 */
	memcpy(&self_node, &chosen_mb->node_array[1], sizeof(struct dgsh_node));
	run_ntimes_same = 1;
	fresh_mb->state = PS_RUN;
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->state, PS_RUN);
	ck_assert_int_eq(run_ntimes_same, 2);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	retire_test_analyse_read();

	setup_test_analyse_read();
	/* When they are to leave they pass the block first except
	 * if they are the ones who passed the block the last time
	 * before finding a solution.
	 */
	memcpy(&self_node, &chosen_mb->node_array[1], sizeof(struct dgsh_node));
	should_transmit_mb = false;
	fresh_mb->state = PS_RUN;
	run_ntimes_same = 1;
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->state, PS_RUN);
	ck_assert_int_eq(run_ntimes_same, 2);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	retire_test_analyse_read();

	/* Negotiation state */
	setup_test_analyse_read();
	run_ntimes_same = 0;
	error_ntimes_same = 0;
	/* set_dispatcher() */
	self_node_io_side.index = 3;
	self_node_io_side.fd_direction = STDIN_FILENO;
	fresh_mb->initiator_pid = 110; /* Younger than chosen_mb. */
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	retire_test_analyse_read();

	setup_test_analyse_read();
	should_transmit_mb = false;
	memcpy(&self_node, &chosen_mb->node_array[3], sizeof(struct dgsh_node));
	/* set_dispatcher() */
	self_node_io_side.index = 3;
	self_node_io_side.fd_direction = STDIN_FILENO;
	fresh_mb->initiator_pid = 103; /* Same initiator */
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	retire_test_analyse_read();

	setup_test_analyse_read();
	memcpy(&self_node, &chosen_mb->node_array[0], sizeof(struct dgsh_node));
	/* set_dispatcher() */
	self_node_io_side.index = 0;
	self_node_io_side.fd_direction = STDOUT_FILENO;
	chosen_mb->origin_index = 3;
	chosen_mb->origin_fd_direction = STDIN_FILENO;
	fresh_mb->initiator_pid = 103; /* Same initiator */
	ck_assert_int_eq(analyse_read(fresh_mb,
				&run_ntimes_same,
				&error_ntimes_same, self_node.name,
				self_node.pid, &self_node.requires_channels,
				&self_node.provides_channels), OP_SUCCESS);
}
END_TEST

START_TEST(test_free_mb)
{
	free_mb(chosen_mb);
}
END_TEST

START_TEST(test_fill_node)
{
	/* self node is node at index 3 of chosen_mb */
	fill_node("test", 1003, NULL, NULL);
	ck_assert_int_eq(strcmp(self_node.name, "test"), 0);
	ck_assert_int_eq(self_node.pid, 1003);
	ck_assert_int_eq(self_node.requires_channels, 1);
	ck_assert_int_eq(self_node.provides_channels, 0);

	int n_input_fds = 1;
	int n_output_fds = 1;
	fill_node("test", 1003, &n_input_fds, &n_output_fds);
	ck_assert_int_eq(self_node.requires_channels, 1);
	ck_assert_int_eq(self_node.provides_channels, 1);
}
END_TEST

START_TEST(test_try_add_dgsh_node)
{
	int n_input_fds = 1;
	int n_output_fds = 1;
	ck_assert_int_eq(try_add_dgsh_node("proc3", 103, &n_input_fds,
				&n_output_fds), OP_EXISTS);
	ck_assert_int_eq(chosen_mb->n_nodes, 4);
	ck_assert_int_eq(self_node_io_side.index, 0);
	ck_assert_int_eq(self_node.index, 3);

	ck_assert_int_eq(try_add_dgsh_node("proc4", 104, &n_input_fds,
				&n_output_fds), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->n_nodes, 5);
	ck_assert_int_eq(self_node_io_side.index, 4);
	ck_assert_int_eq(self_node.index, 4);
}
END_TEST

START_TEST(test_try_add_dgsh_edge)
{
	/* Better in a setup function. */ 
	chosen_mb->origin_fd_direction = STDOUT_FILENO;   
	chosen_mb->origin_index = 0;
	/* self_node_io_side should also be set; it is set in setup */
	ck_assert_int_eq(try_add_dgsh_edge(), OP_EXISTS);

	/* New edge: from new node to existing */
	struct dgsh_node new;
	new.index = 4;
	new.pid = 104;
	memcpy(new.name, "proc4", 6);
	new.requires_channels = 1;
	new.provides_channels = 1;
	new.dgsh_in = 1;
        new.dgsh_out = 1;
	/* Better in a setup function. */ 
	chosen_mb->origin_fd_direction = STDOUT_FILENO;   
	chosen_mb->origin_index = new.index;
	/* self_node_io_side should also be set; it is set in setup */
	memcpy(&self_node, &new, sizeof(struct dgsh_node));
	chosen_mb->n_nodes++;
	chosen_mb->node_array = realloc(chosen_mb->node_array,
				sizeof(struct dgsh_node) * chosen_mb->n_nodes);
	memcpy(&chosen_mb->node_array[chosen_mb->n_nodes - 1], &new,
		sizeof(struct dgsh_node));
	ck_assert_int_eq(try_add_dgsh_edge(), OP_SUCCESS);

	/* New edge: from existing to new node */
	/* Better in a setup function. */ 
	chosen_mb->origin_fd_direction = STDOUT_FILENO;   
	chosen_mb->origin_index = 0;
	/* self_node_io_side should also be set; it is set in setup */
	self_node_io_side.index = new.index;
	self_node_io_side.fd_direction = STDIN_FILENO;
	ck_assert_int_eq(try_add_dgsh_edge(), OP_SUCCESS);

	/* NOOP: message block created just now */
	chosen_mb->origin_index = -1;
	ck_assert_int_eq(try_add_dgsh_edge(), OP_NOOP);
}
END_TEST

START_TEST(test_add_edge)
{
	struct dgsh_edge new;
	new.from = 2;
	new.to = 3;
	new.instances = 0;
	ck_assert_int_eq(add_edge(&new), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->n_edges, 6);
}
END_TEST

START_TEST(test_fill_dgsh_edge)
{
	struct dgsh_edge new;
	/* STDIN -> STDOUT */
	/* Better in a setup function. */ 
	chosen_mb->origin_fd_direction = STDOUT_FILENO;   
	chosen_mb->origin_index = 0;
	/* self_node_io_side should also be set; it is set in setup */
	ck_assert_int_eq(fill_dgsh_edge(&new), OP_SUCCESS);
	
	/* Impossible case. No such origin. */
	chosen_mb->origin_index = 7;
	ck_assert_int_eq(fill_dgsh_edge(&new), OP_ERROR);
	
	/* STDOUT -> STDIN */
	chosen_mb->origin_fd_direction = STDIN_FILENO;   
	chosen_mb->origin_index = 3;
	memcpy(&self_node, &chosen_mb->node_array[0], sizeof(struct dgsh_node));
	self_node_io_side.fd_direction = STDOUT_FILENO;   
	self_node_io_side.index = 0;
	/* self_node_io_side should also be set; it is set in setup */
	ck_assert_int_eq(fill_dgsh_edge(&new), OP_SUCCESS);
	
}
END_TEST

START_TEST(test_lookup_dgsh_edge)
{
	struct dgsh_edge new;
	new.from = 2;
	new.to = 3;
	ck_assert_int_eq(lookup_dgsh_edge(&new), OP_CREATE);
	ck_assert_int_eq(lookup_dgsh_edge(&chosen_mb->edge_array[4]), OP_EXISTS);
}
END_TEST

START_TEST(test_add_node)
{
	struct dgsh_node new;
	new.pid = 104;
	memcpy(new.name, "proc4", 6);
	new.requires_channels = 1;
	new.provides_channels = 1;
	memcpy(&self_node, &new, sizeof(struct dgsh_node));
	ck_assert_int_eq(add_node(), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->n_nodes, 5);
	ck_assert_int_eq(self_node_io_side.index, 4);
	ck_assert_int_eq(self_node.index, 4);
}
END_TEST

START_TEST(test_construct_message_block)
{
	DPRINTF("%s()", __func__);
	int pid = 7;
	const char tool_name[10] = "test";
	ck_assert_int_eq(construct_message_block(tool_name, pid), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->version, 1);
        ck_assert_int_eq((long)chosen_mb->node_array, 0);
        ck_assert_int_eq(chosen_mb->n_nodes, 0);
        ck_assert_int_eq(chosen_mb->initiator_pid, pid);
        ck_assert_int_eq(chosen_mb->state, PS_NEGOTIATION);
        ck_assert_int_eq(chosen_mb->origin_index, -1);
        ck_assert_int_eq(chosen_mb->origin_fd_direction, -1);
	free(chosen_mb);
}
END_TEST

START_TEST(test_get_env_var)
{
	DPRINTF("%s()...", __func__);
	int value = -1;
	putenv("DGSH_IN=0");
	get_env_var("DGSH_IN", &value);
	ck_assert_int_eq(value, 0);

	value = -1;
	putenv("DGSH_OUT=1");
	get_env_var("DGSH_OUT", &value);
	ck_assert_int_eq(value, 1);
}
END_TEST


START_TEST(test_get_environment_vars)
{
	DPRINTF("%s()...", __func__);
	putenv("DGSH_IN=0");
	putenv("DGSH_OUT=1");

	get_environment_vars();
	ck_assert_int_eq(self_node.dgsh_in, 0);
	ck_assert_int_eq(self_node.dgsh_out, 1);

}
END_TEST

START_TEST(test_validate_input)
{
	int i = 0;
	int o = 0;
	ck_assert_int_eq(validate_input(&i, &o, NULL), OP_ERROR); 
	ck_assert_int_eq(validate_input(NULL, &o, "test"), OP_SUCCESS); 
	ck_assert_int_eq(validate_input(&i, NULL, "test"), OP_SUCCESS); 
	ck_assert_int_eq(validate_input(NULL, NULL, "test"), OP_SUCCESS); 
	ck_assert_int_eq(validate_input(&i, &o, "test"), OP_SUCCESS);
	i = 0;
	o = 1;
	ck_assert_int_eq(validate_input(&i, &o, "test"), OP_SUCCESS);
	i = 1;
	o = 0;
	ck_assert_int_eq(validate_input(&i, &o, "test"), OP_SUCCESS);
	i = -1;
	o = -1;
	ck_assert_int_eq(validate_input(&i, &o, "test"), OP_SUCCESS);
	i = -2;
	o = -1;
	ck_assert_int_eq(validate_input(&i, &o, "test"), OP_ERROR);
	i = -1;
	o = -2;
	ck_assert_int_eq(validate_input(&i, &o, "test"), OP_ERROR);
	i = 1000;
	o = 1000;
	ck_assert_int_eq(validate_input(&i, &o, "test"), OP_SUCCESS);
}
END_TEST

START_TEST(test_set_fds)
{
	/* For node 3 which is a terminal node */
	fd_set read_fds, write_fds;
	ck_assert_int_eq(set_fds(&read_fds, &write_fds, 0), 2);
	ck_assert_int_eq(self_node_io_side.fd_direction, STDIN_FILENO);
	ck_assert_int_eq(set_fds(&read_fds, &write_fds, 1), 2);
	ck_assert_int_eq(self_node_io_side.fd_direction, STDIN_FILENO);

	/* Make node 1 self node, which is a non terminal node */
	memcpy(&self_node, &chosen_mb->node_array[1], sizeof(struct dgsh_node));
	ck_assert_int_eq(set_fds(&read_fds, &write_fds, 0), 2);
	ck_assert_int_eq(self_node_io_side.fd_direction, STDOUT_FILENO);
	ck_assert_int_eq(set_fds(&read_fds, &write_fds, 1), 2);
}
END_TEST

START_TEST(test_dgsh_negotiate)
{
	int *input_fds;
	int n_input_fds = 0;
	int *output_fds;
	int n_output_fds = 0;
	ck_assert_int_eq(dgsh_negotiate(0, "test", &n_input_fds, &n_output_fds,
				&input_fds, &output_fds), 0);
}
END_TEST

/* Suite conc */
START_TEST(test_is_ready)
{
	chosen_mb->state = PS_RUN;
	ck_assert_int_eq(is_ready(3, chosen_mb), true);

	ck_assert_int_eq(is_ready(1, chosen_mb), false);

	ck_assert_int_eq(is_ready(0, chosen_mb), false);
	ck_assert_int_eq(pi[0].seen, false);
	ck_assert_int_eq(pi[1].written, false);
	ck_assert_int_eq(pi[1].run_ready, false);
}
END_TEST

START_TEST (test_next_fd)
{
	multiple_inputs = true;
	nfd = 5;
	bool ro = false;		/* restore origin */
	ck_assert_int_eq(next_fd(0, &ro), 1);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(1, &ro), 0);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(4, &ro), 4);
	ck_assert_int_eq(ro, true);
	ro = false;
	ck_assert_int_eq(next_fd(3, &ro), 3);
	ck_assert_int_eq(ro, true);

	multiple_inputs = false;
	noinput = false;
	ro = false;
	ck_assert_int_eq(next_fd(0, &ro), 1);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(1, &ro), 3);
	ck_assert_int_eq(ro, true);
	ro = false;
	ck_assert_int_eq(next_fd(3, &ro), 4);
	ck_assert_int_eq(ro, true);
	ro = false;
	ck_assert_int_eq(next_fd(4, &ro), 0);
	ck_assert_int_eq(ro, false);

	noinput = true;
	ro = false;
	ck_assert_int_eq(next_fd(1, &ro), 3);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(3, &ro), 4);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(4, &ro), 1);
	ck_assert_int_eq(ro, false);
}
END_TEST

START_TEST(test_set_io_channels)
{
	pid = 2000;	/* static in dgsh-conc.c */
	nfd = 4;	/* ditto */
	multiple_inputs = false;	/* ditto */
	ck_assert_int_eq(set_io_channels(chosen_mb), 0);
	ck_assert_int_eq(chosen_mb->n_concs, 1);
	ck_assert_int_eq(chosen_mb->conc_array[0].pid, 2000);
	ck_assert_int_eq(chosen_mb->conc_array[0].input_fds, -1);
	ck_assert_int_eq(chosen_mb->conc_array[0].output_fds, -1);
	ck_assert_int_eq(chosen_mb->conc_array[0].multiple_inputs, false);
	ck_assert_int_eq(chosen_mb->conc_array[0].n_proc_pids, 2);
	ck_assert_int_eq(chosen_mb->conc_array[0].proc_pids[0], 100);
	ck_assert_int_eq(chosen_mb->conc_array[0].proc_pids[1], 103);

	/* Exists with channels set: keep as it is */
	ck_assert_int_eq(set_io_channels(chosen_mb), 0);
	ck_assert_int_eq(chosen_mb->n_concs, 1);
	ck_assert_int_eq(chosen_mb->conc_array[0].pid, 2000);
	ck_assert_int_eq(chosen_mb->conc_array[0].input_fds, -1);
	ck_assert_int_eq(chosen_mb->conc_array[0].output_fds, -1);

	/* Not exists: set channels (same pi, same channels as before */
	pid = 2001;	/* static in dgsh-conc.c */
	multiple_inputs = true;
	ck_assert_int_eq(set_io_channels(chosen_mb), 0);
	ck_assert_int_eq(chosen_mb->n_concs, 2);
	DPRINTF("%d", chosen_mb->conc_array[0].pid);
	ck_assert_int_eq(chosen_mb->conc_array[1].pid, 2001);
	ck_assert_int_eq(chosen_mb->conc_array[1].input_fds, -1);
	ck_assert_int_eq(chosen_mb->conc_array[1].output_fds, -1);
	ck_assert_int_eq(chosen_mb->conc_array[1].multiple_inputs, true);
	ck_assert_int_eq(chosen_mb->conc_array[1].n_proc_pids, 2);
	ck_assert_int_eq(chosen_mb->conc_array[1].proc_pids[0], 101);
	ck_assert_int_eq(chosen_mb->conc_array[1].proc_pids[1], 103);
}
END_TEST

Suite *
suite_connect(void)
{
	Suite *s = suite_create("Connect");

	TCase *tc_aiof = tcase_create("alloc io fds");
	tcase_add_checked_fixture(tc_aiof, setup_test_alloc_io_fds,
					  retire_test_alloc_io_fds);
	tcase_add_test(tc_aiof, test_alloc_io_fds);
	suite_add_tcase(s, tc_aiof);

	TCase *tc_af = tcase_create("alloc fds");
	tcase_add_checked_fixture(tc_af, NULL, NULL);
	tcase_add_test(tc_af, test_alloc_fds);
	suite_add_tcase(s, tc_af);

	TCase *tc_trw = tcase_create("test read/write fd");
	tcase_add_checked_fixture(tc_trw, NULL, NULL);
	tcase_add_test(tc_trw, test_read_write_fd);
	suite_add_tcase(s, tc_trw);

	TCase *tc_rif = tcase_create("read input fds");
	tcase_add_checked_fixture(tc_rif, setup_test_read_input_fds,
					  retire_test_read_input_fds);
	tcase_add_test(tc_rif, test_read_input_fds);
	suite_add_tcase(s, tc_rif);

	TCase *tc_gop = tcase_create("get origin pid");
	tcase_add_checked_fixture(tc_gop, setup_test_get_origin_pid,
					  retire_test_get_origin_pid);
	tcase_add_test(tc_gop, test_get_origin_pid);
	suite_add_tcase(s, tc_gop);

	TCase *tc_gefn = tcase_create("get expected fds number");
	tcase_add_checked_fixture(tc_gefn, setup_test_get_expected_fds_n,
					  retire_test_get_expected_fds_n);
	tcase_add_test(tc_gefn, test_get_expected_fds_n);
	suite_add_tcase(s, tc_gefn);

	TCase *tc_gpfn = tcase_create("get provided fds number");
	tcase_add_checked_fixture(tc_gpfn, setup_test_get_provided_fds_n,
					  retire_test_get_provided_fds_n);
	tcase_add_test(tc_gpfn, test_get_provided_fds_n);
	suite_add_tcase(s, tc_gpfn);

	TCase *tc_eic = tcase_create("establish io connections");
	tcase_add_checked_fixture(tc_eic, setup_test_establish_io_connections,
					  retire_test_establish_io_connections);
	tcase_add_test(tc_eic, test_establish_io_connections);
	suite_add_tcase(s, tc_eic);

	TCase *tc_anc = tcase_create("alloc node connections");
	tcase_add_checked_fixture(tc_anc, NULL, NULL);
	tcase_add_test(tc_anc, test_alloc_node_connections);
	suite_add_tcase(s, tc_anc);

	TCase *tc_sd = tcase_create("set dispatcher");
	tcase_add_checked_fixture(tc_sd, setup_test_set_dispatcher,
					 retire_test_set_dispatcher);
	tcase_add_test(tc_sd, test_set_dispatcher);
	suite_add_tcase(s, tc_sd);

	/* Need to also simulate sendmsg; make sure it works. */
	TCase *tc_awof = tcase_create("write output fds");
	tcase_add_checked_fixture(tc_awof, setup_test_write_output_fds,
					   retire_test_write_output_fds);
	tcase_add_test(tc_awof, test_write_output_fds);
	suite_add_tcase(s, tc_awof);

	return s;
}

Suite *
suite_solve(void)
{
	Suite *s = suite_create("Solve");

	TCase *tc_wc = tcase_create("write concs");
	tcase_add_checked_fixture(tc_wc, setup_test_write_concs,
					  retire_test_write_concs);
	tcase_add_test(tc_wc, test_write_concs);
	suite_add_tcase(s, tc_wc);

	TCase *tc_rc = tcase_create("read concs");
	tcase_add_checked_fixture(tc_rc, setup_test_read_concs,
					  retire_test_read_concs);
	tcase_add_test(tc_rc, test_read_concs);
	suite_add_tcase(s, tc_rc);

	TCase *tc_rgs = tcase_create("read graph solution");
	tcase_add_checked_fixture(tc_rgs, setup_test_read_graph_solution,
					  retire_test_read_graph_solution);
	tcase_add_test(tc_rgs, test_read_graph_solution);
	suite_add_tcase(s, tc_rgs);

	TCase *tc_wgs = tcase_create("write graph solution");
	tcase_add_checked_fixture(tc_wgs, setup_test_write_graph_solution,
					  retire_test_write_graph_solution);
	tcase_add_test(tc_wgs, test_write_graph_solution);
	suite_add_tcase(s, tc_wgs);

	TCase *tc_ssg = tcase_create("solve dgsh graph");
	tcase_add_checked_fixture(tc_ssg, setup_test_solve_dgsh_graph,
					  retire_test_solve_dgsh_graph);
	tcase_add_test(tc_ssg, test_solve_dgsh_graph);
	suite_add_tcase(s, tc_ssg);

	TCase *tc_ccf = tcase_create("calculate conc fds");
	tcase_add_checked_fixture(tc_ccf, setup_test_calculate_conc_fds,
					  retire_test_calculate_conc_fds);
	tcase_add_test(tc_ccf, test_calculate_conc_fds);
	suite_add_tcase(s, tc_ccf);

	TCase *tc_fgs = tcase_create("free graph solution");
	tcase_add_checked_fixture(tc_fgs, setup_test_free_graph_solution,
					  retire_test_free_graph_solution);
	tcase_add_test(tc_fgs, test_free_graph_solution);
	suite_add_tcase(s, tc_fgs);

	TCase *tc_nmc = tcase_create("node match constraints");
	tcase_add_checked_fixture(tc_nmc, setup_test_node_match_constraints,
					  retire_test_node_match_constraints);
	tcase_add_test(tc_nmc, test_node_match_constraints);
	suite_add_tcase(s, tc_nmc);

	TCase *tc_dmic = tcase_create("dry match io constraints");
	tcase_add_checked_fixture(tc_dmic, setup_test_dry_match_io_constraints,
					  retire_test_dry_match_io_constraints);
	tcase_add_test(tc_dmic, test_dry_match_io_constraints);
	suite_add_tcase(s, tc_dmic);

	TCase *tc_sic = tcase_create("satisfy io constraints");
	tcase_add_checked_fixture(tc_sic, setup_test_satisfy_io_constraints,
					  retire_test_satisfy_io_constraints);
	tcase_add_test(tc_sic, test_satisfy_io_constraints);
	suite_add_tcase(s, tc_sic);

	TCase *tc_mov = tcase_create("move");
	tcase_add_checked_fixture(tc_mov, setup_test_move, retire_test_move);
	tcase_add_test(tc_mov, test_move);
	suite_add_tcase(s, tc_mov);

	TCase *tc_rmf = tcase_create("record move flexible");
	tcase_add_checked_fixture(tc_rmf, NULL, NULL);
	tcase_add_test(tc_rmf, test_record_move_flexible);
	suite_add_tcase(s, tc_rmf);

	TCase *tc_rmu = tcase_create("record move unbalanced");
	tcase_add_checked_fixture(tc_rmu, NULL, NULL);
	tcase_add_test(tc_rmu, test_record_move_unbalanced);
	suite_add_tcase(s, tc_rmu);

	/*TCase *tc_ec = tcase_create("evaluate constraints");
	tcase_add_checked_fixture(tc_ec, setup_test_eval_constraints,
					 retire_test_eval_constraints);
	tcase_add_test(tc_ec, test_eval_constraints);
	suite_add_tcase(s, tc_ec);

	*TCase *tc_aei = tcase_create("assign edge instances");
	tcase_add_checked_fixture(tc_aei, setup_test_assign_edge_instances,
					  retire_test_assign_edge_instances);
	tcase_add_test(tc_aei, test_assign_edge_instances);
	suite_add_tcase(s, tc_aei);
*/
	TCase *tc_repa = tcase_create("reallocate edge pointer array");
	tcase_add_checked_fixture(tc_repa,
				setup_test_reallocate_edge_pointer_array,
				retire_test_reallocate_edge_pointer_array);
	tcase_add_test(tc_repa, test_reallocate_edge_pointer_array);
	suite_add_tcase(s, tc_repa);

	TCase *tc_mcea = tcase_create("make compact edge array");
	tcase_add_checked_fixture(tc_mcea, setup_test_make_compact_edge_array,
					   retire_test_make_compact_edge_array);
	tcase_add_test(tc_mcea, test_make_compact_edge_array);
	suite_add_tcase(s, tc_mcea);

	return s;
}

Suite *
suite_broadcast(void)
{
	Suite *s = suite_create("Broadcast");

	TCase *tc_wmb = tcase_create("write message block");
	tcase_add_checked_fixture(tc_wmb, setup_test_write_message_block,
					  retire_test_write_message_block);
	tcase_add_test(tc_wmb, test_write_message_block);
	suite_add_tcase(s, tc_wmb);

	TCase *tc_trm = tcase_create("read message block");
	tcase_add_checked_fixture(tc_trm, setup_test_read_message_block,
					  retire_test_read_message_block);
	tcase_add_test(tc_trm, test_read_message_block);
	suite_add_tcase(s, tc_trm);

	TCase *tc_trc = tcase_create("read chunk");
	tcase_add_checked_fixture(tc_trc, setup_test_read_chunk, NULL);
	tcase_add_test(tc_trc, test_read_chunk);
	suite_add_tcase(s, tc_trc);

	TCase *tc_clr = tcase_create("call read");
	tcase_add_checked_fixture(tc_clr, NULL, NULL);
	tcase_add_test(tc_clr, test_call_read);
	suite_add_tcase(s, tc_clr);

	TCase *tc_acm = tcase_create("alloc copy message block");
	tcase_add_checked_fixture(tc_acm, NULL, NULL);
	tcase_add_test(tc_acm, test_alloc_copy_mb);
	suite_add_tcase(s, tc_acm);

	TCase *tc_acn = tcase_create("alloc copy nodes");
	tcase_add_checked_fixture(tc_acn, setup_test_alloc_copy_nodes,
					  retire_test_alloc_copy_nodes);
	tcase_add_test(tc_acn, test_alloc_copy_nodes);
	suite_add_tcase(s, tc_acn);

	TCase *tc_ace = tcase_create("alloc copy edges");
	tcase_add_checked_fixture(tc_ace, setup_test_alloc_copy_edges,
					  retire_test_alloc_copy_edges);
	tcase_add_test(tc_ace, test_alloc_copy_edges);
	suite_add_tcase(s, tc_ace);

	TCase *tc_acg = tcase_create("alloc copy graph solution");
	tcase_add_checked_fixture(tc_acg, setup_test_alloc_copy_graph_solution,
				retire_test_alloc_copy_graph_solution);
	tcase_add_test(tc_acg, test_alloc_copy_graph_solution);
	suite_add_tcase(s, tc_acg);

	TCase *tc_acc = tcase_create("alloc copy concs");
	tcase_add_checked_fixture(tc_acc, setup_test_alloc_copy_concs,
				retire_test_alloc_copy_concs);
	tcase_add_test(tc_acc, test_alloc_copy_concs);
	suite_add_tcase(s, tc_acc);

	TCase *tc_acp = tcase_create("alloc copy proc pids");
	tcase_add_test(tc_acp, test_alloc_copy_proc_pids);
	suite_add_tcase(s, tc_acp);

	TCase *tc_cr = tcase_create("check read");
	tcase_add_checked_fixture(tc_cr, NULL, NULL);
	tcase_add_test(tc_cr, test_check_read);
	suite_add_tcase(s, tc_cr);
/*
	*TCase *tc_pid = tcase_create("point io direction");
	tcase_add_checked_fixture(tc_pid, setup_test_point_io_direction,
					  retire_test_point_io_direction);
	tcase_add_test(tc_pid, test_point_io_direction);
	suite_add_tcase(s, tc_pid);
*/

	TCase *tc_ar = tcase_create("analyse read");
	tcase_add_checked_fixture(tc_ar, setup_test_analyse_read,
					  retire_test_analyse_read);
	tcase_add_test(tc_ar, test_analyse_read);
	suite_add_tcase(s, tc_ar);

	TCase *tc_fm = tcase_create("free message block");
	tcase_add_checked_fixture(tc_fm, setup_test_free_mb, NULL);
	tcase_add_test(tc_fm, test_free_mb);
	suite_add_tcase(s, tc_fm);

	TCase *tc_fsn = tcase_create("fill dgsh node");
	tcase_add_checked_fixture(tc_fsn, setup_test_fill_node, NULL);
	tcase_add_test(tc_fsn, test_fill_node);
	suite_add_tcase(s, tc_fsn);

	TCase *tc_tasn = tcase_create("try add dgsh node");
	tcase_add_checked_fixture(tc_tasn, setup_test_try_add_dgsh_node,
					   retire_test_try_add_dgsh_node);
	tcase_add_test(tc_tasn, test_try_add_dgsh_node);
	suite_add_tcase(s, tc_tasn);

	TCase *tc_tase = tcase_create("try add dgsh edge");
	tcase_add_checked_fixture(tc_tase, setup_test_try_add_dgsh_edge,
					   retire_test_try_add_dgsh_edge);
	tcase_add_test(tc_tase, test_try_add_dgsh_edge);
	suite_add_tcase(s, tc_tase);

	TCase *tc_ae = tcase_create("add edge");
	tcase_add_checked_fixture(tc_ae, setup_test_add_edge,
					 retire_test_add_edge);
	tcase_add_test(tc_ae, test_add_edge);
	suite_add_tcase(s, tc_ae);

	TCase *tc_fse = tcase_create("fill dgsh edge");
	tcase_add_checked_fixture(tc_fse, setup_test_fill_dgsh_edge,
					 retire_test_fill_dgsh_edge);
	tcase_add_test(tc_fse, test_fill_dgsh_edge);
	suite_add_tcase(s, tc_fse);

	TCase *tc_lse = tcase_create("lookup dgsh edge");
	tcase_add_checked_fixture(tc_lse, setup_test_lookup_dgsh_edge,
					 retire_test_lookup_dgsh_edge);
	tcase_add_test(tc_lse, test_lookup_dgsh_edge);
	suite_add_tcase(s, tc_lse);

	TCase *tc_an = tcase_create("add node");
	tcase_add_checked_fixture(tc_an, setup_test_add_node,
					 retire_test_add_node);
	tcase_add_test(tc_an, test_add_node);
	suite_add_tcase(s, tc_an);

	TCase *tc_cnmb = tcase_create("construct message block");
	tcase_add_checked_fixture(tc_cnmb, NULL, NULL);
	tcase_add_test(tc_cnmb, test_construct_message_block);
	suite_add_tcase(s, tc_cnmb);

	TCase *tc_gev = tcase_create("get environment variable");
	tcase_add_checked_fixture(tc_gev, NULL, NULL);
	tcase_add_test(tc_gev, test_get_env_var);
	suite_add_tcase(s, tc_gev);

	TCase *tc_gevs = tcase_create("get environment variables");
	tcase_add_checked_fixture(tc_gevs, NULL, NULL);
	tcase_add_test(tc_gevs, test_get_environment_vars);
	suite_add_tcase(s, tc_gevs);

	TCase *tc_vi = tcase_create("validate input");
	tcase_add_checked_fixture(tc_vi, NULL, NULL);
	tcase_add_test(tc_vi, test_validate_input);
	suite_add_tcase(s, tc_vi);

	TCase *tc_sf = tcase_create("set fds");
	tcase_add_checked_fixture(tc_sf, setup_test_set_fds, retire_test_set_fds);
	tcase_add_test(tc_sf, test_set_fds);
	suite_add_tcase(s, tc_sf);

	TCase *tc_sn = tcase_create("dgsh negotiate");
	tcase_add_checked_fixture(tc_sn, setup, retire);
	tcase_add_test(tc_sn, test_dgsh_negotiate);
	suite_add_tcase(s, tc_sn);

	return s;
}

Suite *
suite_conc(void)
{
	Suite *s = suite_create("Concentrator");
	TCase *tc_tn = tcase_create("test next_fd");
	TCase *tc_ir = tcase_create("test is_ready");
	TCase *tc_si = tcase_create("set io");
	TCase *tc_sich = tcase_create("set io channels");

	tcase_add_checked_fixture(tc_tn, NULL, NULL);
	tcase_add_test(tc_tn, test_next_fd);
	suite_add_tcase(s, tc_tn);
	tcase_add_checked_fixture(tc_ir, setup_test_is_ready,
			retire_test_is_ready);
	tcase_add_test(tc_ir, test_is_ready);
	suite_add_tcase(s, tc_ir);
	tcase_add_checked_fixture(tc_sich, setup_test_set_io_channels,
					  retire_test_set_io_channels);
	tcase_add_test(tc_sich, test_set_io_channels);
	suite_add_tcase(s, tc_sich);

	return s;
}

int run_suite(Suite *s)
{
	int number_failed;
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? 1 : 0;
}

int
run_suite_connect(void)
{
	Suite *s = suite_connect();
	return run_suite(s);
}

int
run_suite_solve(void)
{
	Suite *s = suite_solve();
	return run_suite(s);
}

int
run_suite_broadcast(void)
{
	Suite *s = suite_broadcast();
	return run_suite(s);
}

int
run_suite_conc(void)
{
	Suite *s = suite_conc();
	return run_suite(s);
}

/* Output is not appropriate; only pass fail. */
int main()
{
	int failed_neg, failed_sol, failed_conn, failed_conc;
	failed_neg = run_suite_broadcast();
	failed_sol = run_suite_solve();
	failed_conn = run_suite_connect();
	failed_conc = run_suite_conc();
	return (failed_neg && failed_sol && failed_conn && failed_conc) ? EXIT_SUCCESS : EXIT_FAILURE;
}
