# PES-VCS Lab Report
**Name:** LOKESH ADITHYA
**SRN:** PES1UG24CSS392

## Phase 1: Object Storage

### Screenshots
- 1A: test_objects output showing all tests passing
- 1B: find .pes/objects -type f showing sharded directory structure

### Implementation
Implemented `object_write` and `object_read` in `object.c`.
- `object_write`: builds header ("blob N\0"), computes SHA-256, deduplicates, writes atomically via mkstemp+rename
- `object_read`: reads file, verifies integrity by recomputing hash, parses header, returns data portion

## Phase 2: Tree Objects

### Screenshots
- 2A: test_tree output showing all tests passing
- 2B: xxd of raw tree object showing binary format

### Implementation
Implemented `tree_parse`, `tree_serialize`, and `tree_from_index` in `tree.c`.
- `tree_serialize`: sorts entries by name before serializing (deterministic hashing)
- `tree_parse`: safely parses binary format using memchr to find null bytes
- `tree_from_index`: recursively builds tree hierarchy from staged files

## Phase 3: Index (Staging Area)

### Screenshots
- 3A: pes init → pes add → pes status sequence
- 3B: cat .pes/index showing text-format index

### Implementation
Implemented `index_load`, `index_save`, `index_add`, `index_status` in `index.c`.
- Uses text format: `<mode> <hex> <mtime> <size> <path>` per line
- `index_save` uses heap allocation to avoid stack overflow (Index struct is ~5MB)
- Atomic writes via mkstemp+rename pattern

## Phase 4: Commits and History

### Screenshots
- 4A: pes log showing three commits
- 4B: find .pes -type f showing object store growth
- 4C: cat .pes/refs/heads/main and cat .pes/HEAD

### Implementation
Implemented `commit_create`, `head_read`, `head_update` in `commit.c`.
- `commit_create`: builds tree from index, reads parent, serializes and stores commit, updates HEAD
- `head_read`: follows symbolic refs (HEAD → branch → commit hash)
- `head_update`: atomically swings branch pointer via temp file + rename

## Phase 5: Branching Analysis

**Q5.1:** Update `.pes/HEAD` to point to the new branch, then walk the target commit's tree and rewrite every file in the working directory. Delete files not in the target tree. Complexity comes from handling deletions, overwrites, and new files, plus refusing if local modifications would be lost.

**Q5.2:** The index stores `mtime` and `size` per staged file. Compare these against disk to detect modifications. Before checkout, find files differing between branch trees. If any are also locally modified, refuse checkout.

**Q5.3:** HEAD holds a raw commit hash instead of a branch name. New commits are saved but nothing points to them, so switching away makes them unreachable. To recover, note the hash from `pes log` before switching, then create a branch pointing to it.

## Phase 6: Garbage Collection Analysis

**Q6.1:** Walk all branch refs, follow every commit's parent chain, mark each commit/tree/blob reachable. Delete everything in `.pes/objects/` not marked. Use a hash set for O(1) lookups. For 100k commits × ~5 objects = ~500k objects to visit.

**Q6.2:** GC marks a new blob as unreachable (commit not written yet) and deletes it. Then the commit is written referencing the deleted blob — silent corruption. Git fixes this by never deleting objects newer than 2 weeks, giving in-progress operations time to complete.

## Integration Test
All integration tests passed — 3 commits created, commit history walked correctly, reference chain verified.
