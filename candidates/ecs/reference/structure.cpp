// The oracle, entered through the same harness as every candidate so that its
// cost is measured on the same terms. Verifying it compares it against itself
// and is therefore vacuous; it is present as a performance floor, not as
// evidence of correctness.
#include "gds/entry.hpp"
#include "gds/reference.hpp"

GDS_CANDIDATE_MAIN(gds::ReferenceStructure)
