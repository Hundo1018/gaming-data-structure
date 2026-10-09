/* The k-nearest ordering, as heap and sort operations over Neighbour: the
 * pieces every k-nearest search here is assembled from. Instantiated once, so
 * every structure that includes it uses the same code. */
#ifndef GDS_SPATIAL_KNN_H
#define GDS_SPATIAL_KNN_H

#include "gds/spatial/types.h"

/* gds_neighbour_sort, _partial_sort, _make_heap, _push_heap, _pop_heap,
 * _sort_heap, ordered by gds_nearer: the heap's top is the farthest. */
#define GDS_SORT_NAME gds_neighbour
#define GDS_SORT_T Neighbour
#define GDS_SORT_LESS(a, b) gds_nearer((a), (b))
#include "gds/sort.inc.h"

#endif
