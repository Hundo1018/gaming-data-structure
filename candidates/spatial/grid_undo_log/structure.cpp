// uniform_grid with its history strategy changed and nothing else.
//
// The index is byte-for-byte the same code as its parent. The only difference
// is that rewinding replays a log of what changed instead of restoring a
// snapshot and rebuilding, which is what makes the pair a measurement of the
// history strategy rather than of two different indexes.
#include "gds/spatial/entry.hpp"
#include "gds/spatial/undo_log_rewind.hpp"
#include "spatial/uniform_grid/structure.hpp"

namespace gds::candidates {

class GridUndoLog : public gds::spatial::UndoLogRewind<UniformGrid> {
 public:
  using Base = gds::spatial::UndoLogRewind<UniformGrid>;
  using Base::Base;
  static const char* name() { return "grid_undo_log"; }
};

}  // namespace gds::candidates

GDS_SPATIAL_CANDIDATE_MAIN(gds::candidates::GridUndoLog)
