// The oracle, entered through the same harness as every candidate so that the
// cost of scanning everything is on the record. Verifying it compares it
// against itself and proves nothing; it is here as a performance floor and as
// the answer to "at what size does an index stop paying for itself".
#include "gds/spatial/entry.hpp"
#include "gds/spatial/oracle.hpp"

GDS_SPATIAL_CANDIDATE_MAIN(gds::spatial::BruteForceOracle)
