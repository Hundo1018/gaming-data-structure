/* uniform_grid with its history strategy changed and nothing else.
 *
 * The index is byte-for-byte the same code as its parent. The only difference
 * is that rewinding replays a log of what changed instead of restoring a
 * snapshot and rebuilding, which is what makes the pair a measurement of the
 * history strategy rather than of two different indexes. */
#include "spatial/uniform_grid/structure.h"

#define GDS_UL_INNER uniform_grid
#define GDS_UL_SELF grid_undo_log
#include "gds/spatial/undo_log_rewind.inc.h"

#define GDS_CANDIDATE grid_undo_log
#include "gds/spatial/entry.h"
