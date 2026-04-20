# PES-VCS Lab Report

**Name:** LOKESH ADITHYA
**SRN:** PES1UG24CSS392

---

## Phase 1: Object Storage

### Screenshots

#### 1A: test_objects output

![Phase 1A](screenshots/1a.jpg)

#### 1B: object storage structure

![Phase 1B](screenshots/1b.jpg)

### Implementation

Implemented `object_write` and `object_read` in `object.c`.

* `object_write`: builds header ("blob N\0"), computes SHA-256, deduplicates, writes atomically via `mkstemp + rename`
* `object_read`: reads file, verifies integrity by recomputing hash, parses header, returns data portion

---

## Phase 2: Tree Objects

### Screenshots

#### 2A: test_tree output

![Phase 2A](screenshots/2a.jpg)

#### 2B: raw tree object (xxd)

![Phase 2B](screenshots/2b.jpg)

### Implementation

Implemented `tree_parse`, `tree_serialize`, and `tree_from_index` in `tree.c`.

* `tree_serialize`: sorts entries by name before serializing (deterministic hashing)
* `tree_parse`: safely parses binary format using `memchr` to find null bytes
* `tree_from_index`: recursively builds tree hierarchy from staged files

---

## Phase 3: Index (Staging Area)

### Screenshots

#### 3A: pes workflow (init → add → status)

![Phase 3A](screenshots/3a.jpg)

#### 3B: index file contents

![Phase 3B](screenshots/3b.jpg)

### Implementation

Implemented `index_load`, `index_save`, `index_add`, `index_status` in `index.c`.

* Uses text format: `<mode> <hex> <mtime> <size> <path>` per line
* `index_save` uses heap allocation to avoid stack overflow (Index struct is ~5MB)
* Atomic writes via `mkstemp + rename` pattern

---

## Phase 4: Commits and History

### Screenshots

#### 4A: commit log

![Phase 4A](screenshots/4a.jpg)

#### 4B: object store growth

![Phase 4B](screenshots/4b.jpg)

#### 4C: HEAD and refs

![Phase 4C](screenshots/4c.jpg)

### Implementation

Implemented `commit_create`, `head_read`, `head_update` in `commit.c`.

* `commit_create`: builds tree from index, reads parent, serializes and stores commit, updates HEAD
* `head_read`: follows symbolic refs (HEAD → branch → commit hash)
* `head_update`: atomically updates branch pointer using temp file + rename

---

## Phase 5: Branching Analysis

**Q5.1:**
Update `.pes/HEAD` to point to the new branch, then traverse the target commit’s tree and rewrite every file in the working directory. Files not present in the target tree must be deleted. The complexity lies in safely handling file overwrites, deletions, and additions, while preventing data loss if local changes exist.

**Q5.2:**
The index stores `mtime` and `size` for each file. By comparing these values with the working directory, modified files can be detected. Before checkout, differences between current and target branch trees are computed. If a file is both modified locally and different in the target branch, checkout must be aborted.

**Q5.3:**
In detached HEAD state, HEAD stores a direct commit hash instead of a branch reference. New commits become unreachable because no branch points to them. To recover, the commit hash (from `pes log`) must be used to create a new branch pointing to that commit.

---

## Phase 6: Garbage Collection Analysis

**Q6.1:**
Garbage collection starts from all branch references, traverses commit histories, and marks all reachable objects (commits, trees, blobs). Unmarked objects in `.pes/objects/` are deleted. A hash set ensures efficient O(1) lookups. For large repositories (~100k commits), traversal may involve ~500k objects.

**Q6.2:**
If GC runs during a commit, newly created blobs may be incorrectly deleted before the commit is finalized, leading to corruption. Git avoids this by retaining recently created objects (typically for 2 weeks), ensuring in-progress operations are not affected.

---

## Integration Test

All integration tests passed successfully:

* Multiple commits created
* Commit history traversed correctly
* Reference chain verified

---
