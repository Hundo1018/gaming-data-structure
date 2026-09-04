"""The candidate manifest schema, and who reads each field.

`PROJECT.md` lists the fields a manifest must carry. It does not say which of
them any code actually reads, and that gap let `complexity:` sit in every
manifest for two tracks without a single line of the runner ever looking at it.
A hand-written claim that nothing consumes is indistinguishable from a comment.

So the schema is declared here with a consumer for every field. `documentation`
means the field is written for a human reader and no code path depends on it,
which is a legitimate answer — but it has to be said out loud rather than being
what happens by default. Validation rejects a manifest with an unknown key, so
adding a field forces a decision about who reads it.
"""

FIELDS = {
    # field: (required, consumer)
    "id":                     (True,  "archive.candidates.id, genealogy"),
    "name":                   (True,  "orchestrate: identity in every result and table"),
    "track":                  (True,  "orchestrate: decides which workloads it meets"),
    "binary":                 (True,  "orchestrate: which executable to run"),
    "island":                 (False, "archive.candidates.island; population bookkeeping"),
    "parents":                (True,  "archive.candidates.parents; genealogy"),
    "origin":                 (True,  "orchestrate: 'oracle' anchors the bench checksum, "
                                      "'negative_control' is excluded from measurement "
                                      "and from sweeps"),
    "novelty_status":         (True,  "archive.candidates.novelty_status; the Historian "
                                      "stage's field, not yet written by any code"),
    "expect_verify":          (True,  "orchestrate: 'pass' gates measurement, 'fail' "
                                      "asserts the correctness gate rejects it"),
    "complexity":             (True,  "sweep: the claim each measured growth exponent is "
                                      "compared against"),
    "hypothesis":             (True,  "archive.candidates.hypothesis; report"),
    "representation":         (True,  "archive.candidates.representation"),
    "operations":             (True,  "documentation"),
    "assumptions":            (True,  "documentation"),
    "expected_advantages":    (True,  "documentation"),
    "expected_disadvantages": (True,  "documentation"),
    "mutation_operator":      (False, "documentation: which operator from PROJECT.md "
                                      "produced this candidate from its parent"),
    "notes":                  (False, "documentation"),
}


def validate(manifest, path):
    """Returns a list of problems; empty means the manifest is well formed."""
    problems = []
    for field, (required, _) in FIELDS.items():
        if required and field not in manifest:
            problems.append(f"{path}: missing required field '{field}'")
    for field in manifest:
        if field.startswith("_"):
            continue
        if field not in FIELDS:
            problems.append(
                f"{path}: unknown field '{field}'. Add it to runner/manifest.py "
                f"with the code that reads it, or 'documentation' if nothing does."
            )
    return problems


def consumers_table():
    """The schema as rows, for ARCHITECTURE.md."""
    rows = []
    for field, (required, consumer) in FIELDS.items():
        rows.append((field, "required" if required else "optional", consumer))
    return rows


if __name__ == "__main__":
    # Prints the table that ARCHITECTURE.md carries, so the document can be
    # regenerated from the schema rather than kept in step by hand.
    print("| field | required | read by |")
    print("|---|---|---|")
    for field, req, consumer in consumers_table():
        print(f"| `{field}` | {req} | {consumer} |")
