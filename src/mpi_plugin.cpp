#include <gcc-plugin.h>
#include <plugin-version.h>
#include <tree.h>
#include <basic-block.h>
#include <gimple.h>
#include <tree-pass.h>
#include <context.h>
#include <function.h>
#include <gimple-iterator.h>
#include <vector>
#include <diagnostic-core.h>
#include <c-family/c-pragma.h>

/* Global variable required for plugin to execute */
int plugin_is_GPL_compatible;

/* Enum to represent the collective operations */
enum mpi_collective_code {
#define DEFMPICOLLECTIVES( CODE, NAME ) CODE,
#include "include/MPI_collectives.def"
        LAST_AND_UNUSED_MPI_COLLECTIVE_CODE
#undef DEFMPICOLLECTIVES
} ;

/* Name of each MPI collective operations */
#define DEFMPICOLLECTIVES( CODE, NAME ) NAME,
const char *const mpi_collective_name[] = {
#include "include/MPI_collectives.def"
} ;
#undef DEFMPICOLLECTIVES

/* Check if the statement is one of the mpi collectives and returns its code */
int is_mpi_call(gimple *stmt) {
        if (is_gimple_call(stmt)) {
        tree function_decl = gimple_call_fndecl(stmt);
        const char* func_name = get_name(function_decl);
	if (strncmp(func_name, "MPI_", 4) == 0) {
        	for (int i = 0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; ++i) {
                	if (strcmp(func_name, mpi_collective_name[i]) == 0) {
         	                return i;
                	}
            	}
        }
        
    }
    return LAST_AND_UNUSED_MPI_COLLECTIVE_CODE;
}

/* returns the number of mpi collectives in the basic block  */
int get_nb_mpi_calls_in_bb( basic_block bb )
{
        gimple_stmt_iterator gsi;
        int nb_mpi_coll = 0 ;

        for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
        {
                gimple *stmt = gsi_stmt (gsi);

                int c = is_mpi_call(stmt) ;

                if ( c != LAST_AND_UNUSED_MPI_COLLECTIVE_CODE )
                {
                        nb_mpi_coll++ ;
                }
        }

        return nb_mpi_coll ;
}

/* splits the blocks containing multiple collectives */
void split_multiple_mpi_calls( function * fun )
{
        basic_block bb;

        FOR_EACH_BB_FN (bb, fun)
        {
                int n = get_nb_mpi_calls_in_bb( bb ) ;
                if ( n > 1 )
                {
                        gimple_stmt_iterator gsi;

                        for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
                        {
                                gimple *stmt = gsi_stmt (gsi);

                                int c = is_mpi_call(stmt) ;

                                if ( c != LAST_AND_UNUSED_MPI_COLLECTIVE_CODE )
                                {
                                        split_block( bb, stmt ) ;
                                }
                        }
                }
        }
}

/* struct used to store the collective code in the block */
/* and a vector to store the ranks of the collectives in that block */
typedef struct {
	int code;
	int* ranks;
} mpi_ranks;

void clean_aux_field( function * fun, long val )
{
        basic_block bb;

        /* Traverse all BBs to clean 'aux' field */
        FOR_ALL_BB_FN (bb, fun)
        {       
		mpi_ranks *aux_rank = (mpi_ranks *) bb -> aux;
		free(aux_rank -> ranks);
		free(aux_rank);
                bb->aux = (void *)val ;
        }
}


/* function to initialize the mpi_ranks and store them in the aux field of the blocks */
void mpi_ranks_in_aux(function *fun)
{
        basic_block bb;
        gimple_stmt_iterator gsi;
        gimple *stmt;

        FOR_ALL_BB_FN(bb,fun)
        {	
		mpi_ranks *aux_ranks = XNEWVEC(mpi_ranks, 1);
		int *ranks = XNEWVEC(int, LAST_AND_UNUSED_MPI_COLLECTIVE_CODE);
		
		for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) ranks[i]=0;
		
		aux_ranks -> ranks = ranks;
		aux_ranks -> code = LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; /* default value when there is no collective  */
		
                for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
                {
                        stmt = gsi_stmt(gsi);

                        int c = is_mpi_call(stmt);
			if (c != LAST_AND_UNUSED_MPI_COLLECTIVE_CODE) aux_ranks -> code = c;
                }
		bb -> aux = aux_ranks;
        }
}

void prepare_cfg(function * fun)
{
        split_multiple_mpi_calls(fun);

        mpi_ranks_in_aux(fun);
}

/* function to calculate the post dominance frontier of each basic_block and return it in bitmaps*/
bitmap_head *post_dominance_frontiers(function * fun)
{
        basic_block bb;
        
        bitmap_head *frontiers;
        
        frontiers = XNEWVEC (bitmap_head, last_basic_block_for_fn(cfun));
        FOR_ALL_BB_FN(bb, fun) {
                bitmap_initialize(&frontiers[bb -> index], &bitmap_default_obstack);
        }
        
        FOR_ALL_BB_FN(bb, fun) {
                if (EDGE_COUNT(bb -> succs) >= 2) {
                        edge_iterator it;
                        edge e;
                        FOR_EACH_EDGE(e, it, bb -> succs) {
                                basic_block doms;
                                doms = get_immediate_dominator(CDI_POST_DOMINATORS, bb);
                                
                                basic_block p = e -> dest;
                                
                                while (p -> index != doms -> index) {
                                        bitmap_set_bit(&frontiers[p -> index], bb -> index);
                                        p = get_immediate_dominator(CDI_POST_DOMINATORS, p);
                                }
                        }
                }
        }
	#ifdef DEBUG
        printf("----------------- post dominance frontier ------------------------\n");
        FOR_ALL_BB_FN(bb, fun) {
               	printf("Basic Block: %d : ", bb -> index);
               	bitmap_print(stdout, &frontiers[bb -> index], "", "");
               	printf("\n");
        }
        printf("------------------------------------------------------------------\n");
        #endif
	return frontiers;
}

/* function to get the edges of loops going back */
bitmap_head *cfg_prime(function *fun) {
        basic_block bb;

        bitmap_head *invalid_edges;
        bitmap_head *visited;

        invalid_edges = XNEWVEC(bitmap_head, last_basic_block_for_fn(cfun));
        visited = XNEWVEC(bitmap_head, last_basic_block_for_fn(cfun)); /* used to get the visited block to get to a certain block  */
        FOR_ALL_BB_FN(bb, fun) {
                bitmap_initialize(&invalid_edges[bb -> index], &bitmap_default_obstack);
                bitmap_initialize(&visited[bb -> index], &bitmap_default_obstack);
        }

        std::vector <basic_block> to_visit;
        to_visit.push_back(ENTRY_BLOCK_PTR_FOR_FN(fun));

        while (to_visit.size() != 0) {
                bb = to_visit.back();
                to_visit.pop_back();
                int index = bb -> index;
                bitmap_set_bit(&visited[index], index);

                edge_iterator it;
                edge e;

                int edge_index = 0;

                FOR_EACH_EDGE(e, it, bb -> succs) {
                        basic_block child = e -> dest;
                        if (bitmap_bit_p(&visited[index], child -> index)) {
                                bitmap_set_bit(&invalid_edges[index], edge_index);
                        }
                        else {
                                to_visit.push_back(child);
                                bitmap_ior_into(&visited[child -> index], &visited[index]); /*transmit the visited blocks to the next */
                        }
                        edge_index++;
                }
        }

	BITMAP_FREE(visited);

	#ifdef DEBUG
        printf("----------------- invalid edges ------------------------\n");
        FOR_ALL_BB_FN(bb, fun) {
               	printf("Basic Block: %d : ", bb -> index);
               	bitmap_print(stdout, &invalid_edges[bb -> index], "", "");
               	printf("\n");
        }
        printf("------------------------------------------------------\n");
	#endif
        return invalid_edges;
}

/* function to calculate the rank of each collective in each block
and store the max rank of each collective in the last block  */
void calculate_rank( function *fun, bitmap invalid_edges) {
        basic_block bb;
        
        std::vector <basic_block> to_visit;
        to_visit.push_back(ENTRY_BLOCK_PTR_FOR_FN(fun));
		
	basic_block last = EXIT_BLOCK_PTR_FOR_FN(fun);
        mpi_ranks *last_aux_ranks = (mpi_ranks *) last -> aux;
        int *last_ranks = last_aux_ranks -> ranks;
        while (to_visit.size() != 0) {
                bb = to_visit.front();
                to_visit.erase(to_visit.begin());


                int index = bb -> index;

                edge_iterator it;
                edge e;

                int edge_index = 0;

                mpi_ranks *father_aux_ranks = (mpi_ranks *) bb -> aux;

                FOR_EACH_EDGE(e, it, bb -> succs) {
			basic_block child = e -> dest;
                        mpi_ranks *child_aux_ranks = (mpi_ranks *) child -> aux;
                        int* father_ranks = father_aux_ranks -> ranks;
                        int* child_ranks = child_aux_ranks -> ranks;
                        if (!bitmap_bit_p(&invalid_edges[index], edge_index)) {
                                for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
                                        if (father_ranks[i] >= child_ranks[i]) {
                                                child_ranks[i] = father_ranks[i];
                                                if (i == child_aux_ranks -> code) child_ranks[i] += 1;
                                        }
                                }
                                to_visit.push_back(child);
                        }
			else {
				//since we do not transmit the ranks through looping edges
				//we check if the ranks in this block are superior to the ranks in the last
				//this way we ensure that the last block contains the max rank for each collective
				//this will be usefule to create the sets and iterate over them
				for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
                 		       if (father_ranks[i] > last_ranks[i]) last_ranks[i] = father_ranks[i];
                		}
			}
                        edge_index++;
                }
        }
	#ifdef DEBUG
        FOR_ALL_BB_FN(bb, fun) {
               	printf("index: %2d - ", bb -> index);
               	mpi_ranks *aux_ranks = (mpi_ranks *) bb -> aux;
               	printf("collective: %d - ", aux_ranks -> code);
               	printf("[");
               	for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) printf("%d, ", (aux_ranks -> ranks)[i]);
               	printf("]\n");
       	}
	#endif
}

/* returns a matrix of bitmaps representing the blocks containing a collective of a certain rank */
bitmap_head** collective_rank_set(function *fun) {
        basic_block last = EXIT_BLOCK_PTR_FOR_FN(fun);
        mpi_ranks *aux_ranks = (mpi_ranks *) last -> aux;

        int *ranks = aux_ranks -> ranks; /* ranks in the last block containing the max ranks */

        bitmap_head **sets = XNEWVEC(bitmap_head*, LAST_AND_UNUSED_MPI_COLLECTIVE_CODE);
	/* coordinates i, j are the collective code and the rank */
        for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
                int max_rank = ranks[i]; /* getting the max rank for this collective to create enough bitmaps */
                if (max_rank != 0) {
                        sets[i] = XNEWVEC(bitmap_head, max_rank);
                        for(int j=0; j < max_rank; j++) {
                                bitmap_initialize(&sets[i][j], &bitmap_default_obstack);
                        }
                }
        }

        basic_block bb;
        FOR_EACH_BB_FN(bb, fun) {
                mpi_ranks *aux_ranks = (mpi_ranks *) bb -> aux;

                int code = aux_ranks -> code;
                int *ranks = aux_ranks -> ranks;

                int index = bb -> index;
		/* if the block contains a collective we set the index in the right bitmap depending on its rank */
                if (code < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE) {
                        bitmap set = &sets[code][ranks[code]-1];
                        bitmap_set_bit(set, index);
                }
        }
	#ifdef DEBUG
	for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
               	int max_rank = ranks[i];
               	if (max_rank != 0) {
                       	for(int j=0; j < max_rank; j++) {
                               	printf("code: %d, rank: %d - ", i, j+1);
                               	bitmap_print(stdout, &sets[i][j], "", "");
                               	printf("\n");
                       	}
               	}
        }
	#endif
        return sets;
}
/* calculate and return the postdominance of each set */
bitmap_head** set_post_dominance(function *fun, bitmap_head **sets) {
        basic_block last = EXIT_BLOCK_PTR_FOR_FN(fun);

        mpi_ranks *aux_ranks = (mpi_ranks *) last -> aux;

        int *ranks = aux_ranks -> ranks;

        basic_block bb;

        std::vector <basic_block> to_visit;

        bitmap_head **post_dominated = XNEWVEC(bitmap_head*, LAST_AND_UNUSED_MPI_COLLECTIVE_CODE);
        for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
                int max_rank = ranks[i];
                if (max_rank != 0) {
                        post_dominated[i] = XNEWVEC(bitmap_head, max_rank);
                        for(int j=0; j < max_rank; j++) {
                                bitmap_initialize(&post_dominated[i][j], &bitmap_default_obstack);
				
				/* we start from the end of the graph and go up through the nodes that are not part of the set and not marked as not postdominated*/
				/* the nodes we can go through are marked as not postdominated */

                                to_visit.push_back(last);

                                while (to_visit.size() != 0) {
                                        bb = to_visit.back();
                                        to_visit.pop_back();

                                        bitmap_set_bit(&post_dominated[i][j], bb -> index);

                                        edge e;
                                        edge_iterator it;

                                        FOR_EACH_EDGE(e, it, bb -> preds) {
                                                basic_block parent = e -> src;
                                                int parent_index = parent -> index;
                                                if (!bitmap_bit_p(&sets[i][j], parent_index) && !bitmap_bit_p(&post_dominated[i][j], parent_index) && parent_index != 0) {
                                                        to_visit.push_back(parent);
                                                }
                                        }
                                }
				/* reversing the bits in the bitmap to get the nodes that are postdominated */
                                for (int k=1; k < last_basic_block_for_fn(fun); k++) {
                                        if (bitmap_bit_p(&post_dominated[i][j], k)) bitmap_clear_bit(&post_dominated[i][j], k);
                                        else bitmap_set_bit(&post_dominated[i][j], k);
                                }
                        }
                }
        }
	#ifdef DEBUG
 	printf("---- set postdominated ----\n");
       	for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
               	int max_rank = ranks[i];
               	if (max_rank != 0) {
                       	for(int j=0; j < max_rank; j++) {
                               	printf("code: %d, rank: %d - ", i, j+1);
                               	bitmap_print(stdout, &post_dominated[i][j], "", "");
                               	printf("\n");
                       	}
               	}
       	}
	#endif
        return post_dominated;
}

/* calculate and returns the post dominance frontiers of the sets */
bitmap_head** set_post_dominance_frontiers(function * fun, bitmap_head **post_dominated, bitmap_head *frontiers)
{
        basic_block last = EXIT_BLOCK_PTR_FOR_FN(fun);

        mpi_ranks *aux_ranks = (mpi_ranks *) last -> aux;

        int *ranks = aux_ranks -> ranks;

        bitmap_head **set_frontiers = XNEWVEC(bitmap_head*, LAST_AND_UNUSED_MPI_COLLECTIVE_CODE);
        bitmap_head valid_frontiers;
        bitmap_initialize(&valid_frontiers, &bitmap_default_obstack);
        for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
                int max_rank = ranks[i];
                if (max_rank != 0) {
                        set_frontiers[i] = XNEWVEC(bitmap_head, max_rank);
                        for(int j=0; j < max_rank; j++) {
                                bitmap_initialize(&set_frontiers[i][j], &bitmap_default_obstack);
                                for (int k=0; k < last_basic_block_for_fn(fun); k++) {
					/* the frontier of the set is the union of the frontiers of blocks postdominated by set, frontiers that are not postdominated by the set */
                                        if (bitmap_bit_p(&post_dominated[i][j], k)) {
                                                bitmap_and_compl(&valid_frontiers, &frontiers[k], &post_dominated[i][j]);
                                                bitmap_ior_into(&set_frontiers[i][j], &valid_frontiers);
                                        }
                                }
                        }
                }
        }
	#ifdef DEBUG
        printf("---- set frontiers ----\n");
        for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
               	int max_rank = ranks[i];
               	if (max_rank != 0) {
                       	for(int j=0; j < max_rank; j++) {
                               	printf("code: %d, rank: %d - ", i, j+1);
                               	bitmap_print(stdout, &set_frontiers[i][j], "", "");
                               	printf("\n");
                               	if (!bitmap_empty_p(&set_frontiers[i][j])) printf("Potential MPI Deadlock\n");
                       	}
               	}
        }
	#endif
        return set_frontiers;
}
/* calculate the iterated postdominance frontiers of the sets */
bitmap_head **iterated_post_dominance_frontiers(function *fun, bitmap_head **set_frontiers, bitmap_head *frontiers) {
        basic_block last = EXIT_BLOCK_PTR_FOR_FN(fun);

        mpi_ranks *aux_ranks = (mpi_ranks *) last -> aux;

        int *ranks = aux_ranks -> ranks;

        bitmap_head **set_iterated_frontiers = XNEWVEC(bitmap_head*, LAST_AND_UNUSED_MPI_COLLECTIVE_CODE);
        for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
                int max_rank = ranks[i];
                if (max_rank != 0) {
                        set_iterated_frontiers[i] = XNEWVEC(bitmap_head, max_rank);
                        for(int j=0; j < max_rank; j++) {
                                bitmap_initialize(&set_iterated_frontiers[i][j], &bitmap_default_obstack);
                                bitmap_ior_into(&set_iterated_frontiers[i][j], &set_frontiers[i][j]);
				
				/* we add the frontiers of the blocks in the frontier and we keep adding the frontiers of those frontiers until no new blocks are added */
                                unsigned long old_count = bitmap_count_bits(&set_iterated_frontiers[i][j]);
                                unsigned long new_count = 0;
                                while (old_count != new_count) {
                                        old_count = bitmap_count_bits(&set_iterated_frontiers[i][j]);
                                        for (int k=0; k < last_basic_block_for_fn(fun); k++) {
                                                if (bitmap_bit_p(&set_iterated_frontiers[i][j], k)) {
                                                        bitmap_ior_into(&set_iterated_frontiers[i][j], &frontiers[k]);
                                                }
                                        }
                                        new_count = bitmap_count_bits(&set_iterated_frontiers[i][j]);
                                }
                        }
                }
        }
	#ifdef DEBUG
        printf("---- set iterated frontiers ----\n");
        for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
               	int max_rank = ranks[i];
               	if (max_rank != 0) {
                       	for(int j=0; j < max_rank; j++) {
                               	printf("code: %d, rank: %d - ", i, j+1);
                               	bitmap_print(stdout, &set_iterated_frontiers[i][j], "", "");
                               	printf("\n");
                               	if (!bitmap_empty_p(&set_iterated_frontiers[i][j])) printf("Potential MPI Deadlock\n");
                       	}
               	}
        }
	#endif
        return set_iterated_frontiers;
}


bool print_warnings(function *fun, bitmap_head **iterated_pdf, bitmap_head ** set) {
        basic_block last = EXIT_BLOCK_PTR_FOR_FN(fun);
        mpi_ranks *aux_ranks = (mpi_ranks *) last -> aux;
	bool warnings = false;
        int *ranks = aux_ranks -> ranks;
        for (int i=0; i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
                int max_rank = ranks[i];
                if (max_rank != 0) {
                        for(int j=0; j < max_rank; j++) {
                                if (!bitmap_empty_p(&iterated_pdf[i][j])) {
					warnings = true;
                                        for (int k=0; k < last_basic_block_for_fn(fun); k++) {
                                                if (bitmap_bit_p(&set[i][j], k)) {
                                                        basic_block bb = BASIC_BLOCK_FOR_FN(fun, k);
                                                        gimple_stmt_iterator gsi;
                                                        gimple *stmt;
                                                        for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
                                                                stmt = gsi_stmt(gsi);
                                                                if (is_gimple_call(stmt)) {
                                                                        tree function_decl = gimple_call_fndecl(stmt);
                                                                        const char* func_name = get_name(function_decl);
                                                                        if (strcmp(func_name, mpi_collective_name[i]) == 0) {
                                                                                warning_at(gimple_location(stmt), 0, "Potential issue: MPI collective %s in block %d", mpi_collective_name[i], k);
                                                                        }
                                                                }
                                                        }
                                                }
                                        }
                                        for (int k=0; k < last_basic_block_for_fn(fun); k++) {
                                                if (bitmap_bit_p(&iterated_pdf[i][j], k)) {
                                                        basic_block bb = BASIC_BLOCK_FOR_FN(fun, k);
                                                        gimple_stmt_iterator gsi = gsi_last_bb(bb);
                                                        gimple *stmt = gsi_stmt(gsi);
                                                        if (!gsi_end_p(gsi)) {
                                                                stmt = gsi_stmt(gsi);
                                                        }
                                                        warning_at(gimple_location(stmt), 0, "Potential issue caused by the following fork in block %d", k);
                                                }
                                        }
                                }
                        }
                }
        }
	return warnings;
}

/* Pragma Handling  */

static std::vector<tree> fname_vec;

bool is_function_in_pragma_list(const char* fname) {
        for (auto elem : fname_vec) {
                if (strcmp(fname, IDENTIFIER_POINTER(elem)) == 0) {
                return true;
                }
        }
        return false;
}

bool remove_function_from_pragma_list(const char* fname) {
        for (auto it = fname_vec.begin(); it != fname_vec.end(); ++it) {
                if (strcmp(fname, IDENTIFIER_POINTER(*it)) == 0) {
                fname_vec.erase(it); // Remove the found element
                return true; // Indicate that the element was found and removed
                }
        }
        return false; // Indicate that the element was not found
}

static void handle_arg(tree pragma_arg) {
        const char *func_name = IDENTIFIER_POINTER(pragma_arg);
        if (is_function_in_pragma_list(func_name)) {
                warning(0, "%<#pragma ProjetCA mpicoll_check%> function '%s' appears multiple times", func_name);
        } else {
                fname_vec.push_back(pragma_arg);
        }
}

static void
handle_pragma_fx(cpp_reader *dummy ATTRIBUTE_UNUSED)
{
        enum cpp_ttype token;
        bool close_paren_needed = false;
        tree pragma_arg;
                
        if (cfun) {
                error("%<#pragma ProjetCA mpicoll_check%> pragma not allowed inside a function definition");
                return;
        }

        token = pragma_lex(&pragma_arg);
        if (CPP_OPEN_PAREN == token) {
                close_paren_needed = true;
                token = pragma_lex(&pragma_arg);
                if (CPP_NAME != token) {
                error("%<#pragma ProjetCA mpicoll_check%> argument is not a name");
                return;
                }
                
                handle_arg(pragma_arg);
                
                token = pragma_lex(&pragma_arg);
                if (token == CPP_NAME) {
                        error("%<#pragma ProjetCA mpicoll_check%> list must be separated by commas");
                        return;
                }
                while (CPP_COMMA == token) {
                token = pragma_lex(&pragma_arg);
                if (CPP_NAME != token) {
                        error("%<#pragma ProjetCA mpicoll_check%> argument is not a name");
                        return;
                }
                handle_arg(pragma_arg);
                token = pragma_lex(&pragma_arg);
                }
        } else if (CPP_NAME == token) {
                handle_arg(pragma_arg);
                token = pragma_lex(&pragma_arg);
        }else {
                error("%<#pragma ProjetCA mpicoll_check%> argument is not a name");
                return;
        }

        if (CPP_CLOSE_PAREN == token) {
                if (!close_paren_needed) {
                error("%<#pragma ProjetCA mpicoll_check%> unexpected closing perenthesis");
                return;
                }
                close_paren_needed = false;
                token = pragma_lex(&pragma_arg);
        }

        if (CPP_EOF == token) {
                if (close_paren_needed) {
                error("%<#pragma ProjetCA mpicoll_check%> missing closing perenthesis");
                }
        }
        else {
                error("%<#pragma ProjetCA mpicoll_check%> expected parenthesis for list");
                return;
        }

}

void not_declared_functions(void *event_data, void *data) {
	for (auto elem : fname_vec) {
		const char* func_name = IDENTIFIER_POINTER(elem);
		location_t loc = UNKNOWN_LOCATION;
		warning_at(loc, 0, "%<#pragma ProjetCA mpicoll_check%> function '%s' is not declared but referenced in pragma", func_name);
   	}	
}

/* Graphviz */

/* Build a filename (as a string) based on function name */
static char * cfgviz_generate_filename( function * fun, const char * suffix )
{
	char * target_filename ;

	target_filename = (char *)xmalloc( 1024 * sizeof( char ) ) ;

	snprintf( target_filename, 1024, "graph/%s_%s_%d_%s.dot",
			current_function_name(),
			LOCATION_FILE( fun->function_start_locus )+6,
			LOCATION_LINE( fun->function_start_locus ),
			suffix ) ;

	return target_filename ;
}

/* Dump the graphviz representation of function 'fun' in file 'out' */
static void cfgviz_internal_dump( function * fun, FILE * out, bitmap valid_edges)
{
	basic_block bb;
	// Print the header line and open the main graph
	fprintf(out, "Digraph G{\n");

	FOR_ALL_BB_FN(bb, fun)
	{
		fprintf( out,"%d [label=\"BB %d", bb->index,	bb->index);

		gimple_stmt_iterator gsi;
		gimple * stmt;
		gsi = gsi_start_bb(bb);
		stmt = gsi_stmt(gsi);
	
		for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
		{

			stmt = gsi_stmt(gsi);

			enum mpi_collective_code returned_code ;

			returned_code = LAST_AND_UNUSED_MPI_COLLECTIVE_CODE ;

			if (is_gimple_call (stmt))
			{
				tree t ;
				const char * callee_name ;
				int i ;
				bool found = false ;

				t = gimple_call_fndecl( stmt ) ;
				callee_name = IDENTIFIER_POINTER(DECL_NAME(t)) ;

				i = 0 ;
				while ( !found && i < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE )
				{
					if ( strncmp( callee_name, mpi_collective_name[i], strlen(
									mpi_collective_name[i] ) ) == 0 )
					{
						found = true ;
						returned_code = (enum mpi_collective_code) i ;
					}
					i++ ;
				} 

			}


			if ( returned_code != LAST_AND_UNUSED_MPI_COLLECTIVE_CODE )
			{
				fprintf( out, " \\n %s", mpi_collective_name[returned_code] ) ;
			}
		}

		fprintf(out, "\" shape=ellipse]\n");

		edge_iterator eit;
		edge e;
		int i =0;
		FOR_EACH_EDGE( e, eit, bb->succs )
		{
			int exist=1;
			if (valid_edges != NULL && !bitmap_bit_p(&valid_edges[bb->index], i))
			{
				exist=0;
			}
			const char *label = "";
			if( e->flags == EDGE_TRUE_VALUE )
				label = "true";
			else if( e->flags == EDGE_FALSE_VALUE )
				label = "false";
			if (valid_edges != NULL)
			{
				fprintf( out, "%d -> %d [color=%s label=\"%s\"]\n",
						bb->index, e->dest->index, exist ? "red" : "blue", label ) ;
			}
			else
			{
				fprintf( out, "%d -> %d [color=%s label=\"%s\"]\n",
						bb->index, e->dest->index, exist ? "blue" : "red", label ) ;
			}
		}
	}
	
	fprintf(out, "}\n");
}

void 
cfgviz_dump( function * fun, const char * suffix, bitmap valid_edges=NULL)
{
	char * target_filename ; 
	FILE * out ;

	target_filename = cfgviz_generate_filename( fun, suffix ) ;
	
	#ifdef DEBUG
	printf( "[GRAPHVIZ] Generating CFG of function %s in file <%s>\n",
			current_function_name(), target_filename ) ;
	#endif
	
	out = fopen( target_filename, "w" ) ;

	cfgviz_internal_dump( fun, out, valid_edges ) ;

	fclose( out ) ;
	free( target_filename ) ;
}


static std::vector<tree> decl_funs;

/* Global object (const) to represent my pass */
const pass_data mpicoll_pass_data =
{
        GIMPLE_PASS, /* type */
        "mpicoll", /* name */
        OPTGROUP_NONE, /* optinfo_flags */
        TV_OPTIMIZE, /* tv_id */
        0, /* properties_required */
        0, /* properties_provided */
        0, /* properties_destroyed */
        0, /* todo_flags_start */
        0, /* todo_flags_finish */
};

class mpicoll_pass : public gimple_opt_pass
{       
        public: 
                mpicoll_pass (gcc::context *ctxt)
                        : gimple_opt_pass (mpicoll_pass_data, ctxt)
                {}
                
                
                mpicoll_pass *clone ()
                {       
                        return new mpicoll_pass(g);
                }
		               
                bool gate (function *fun)
                {       
			const char* fname = fndecl_name(cfun->decl);
    			if (remove_function_from_pragma_list(fname)) {
        			printf("Now starting to examine function %s\n", fname);
        			return true;
    			}
    			return false;
                }
                
                unsigned int execute (function *fun)
                {       
			cfgviz_dump(fun, "initial");
                        prepare_cfg(fun);
                        cfgviz_dump(fun, "split");
                        calculate_dominance_info(CDI_POST_DOMINATORS);
                        bitmap_head *frontiers = post_dominance_frontiers(fun);
                        bitmap_head *invalid_edges = cfg_prime(fun);
			cfgviz_dump(fun, "invalid_edges", invalid_edges);
                        calculate_rank(fun, invalid_edges);
                        bitmap_head **sets = collective_rank_set(fun);
                        bitmap_head **set_postdominated = set_post_dominance(fun, sets);
                        bitmap_head **set_frontiers = set_post_dominance_frontiers(fun, set_postdominated, frontiers);
                        bitmap_head **it_frontier = iterated_post_dominance_frontiers(fun, set_frontiers, frontiers);
                        bool warnings = print_warnings(fun, it_frontier, sets);
			if (!warnings) printf("No potential deadlock found.\n");
                        clean_aux_field(fun, 0);

                        free_dominance_info(CDI_POST_DOMINATORS);
                        return 0;
                }
};

        int
plugin_init(struct plugin_name_args * plugin_info,
                struct plugin_gcc_version * version)
{
        struct register_pass_info mpicoll_pass_info;

        printf( "plugin_init: Entering...\n" ) ;

        /* First check that the current version of GCC is the right one */

        if(!plugin_default_version_check(version, &gcc_version))
                return 1;

        printf( "plugin_init: Check ok...\n" ) ;

        /* Declare and build my new pass */
        mpicoll_pass p(g);

        /* Fill info on my pass 
 *          (insertion after the pass building the CFG) */
        mpicoll_pass_info.pass = &p;
        mpicoll_pass_info.reference_pass_name = "cfg";
        mpicoll_pass_info.ref_pass_instance_number = 0;
        mpicoll_pass_info.pos_op = PASS_POS_INSERT_AFTER;
	
        /* Add my pass to the pass manager */
        register_callback(plugin_info->base_name,
                        PLUGIN_PASS_MANAGER_SETUP,
                        NULL,
                        &mpicoll_pass_info);
	
	c_register_pragma("Projet_CA", "mpicoll_check", handle_pragma_fx);
	register_callback(plugin_info->base_name, PLUGIN_FINISH, not_declared_functions, NULL);

        printf( "plugin_init: Pass added...\n" ) ;

        return 0;
}

