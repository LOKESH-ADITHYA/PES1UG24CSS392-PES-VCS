#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++)
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            memmove(&index->entries[i], &index->entries[i+1],
                    (index->count - i - 1) * sizeof(IndexEntry));
            index->count--;
            return 0;
        }
    }
    return -1;
}

int index_load(Index *index) {
    index->count = 0;
    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (index->count >= MAX_INDEX_ENTRIES) break;
        IndexEntry *e = &index->entries[index->count];
        char hex[HASH_HEX_SIZE + 1];
        unsigned long long mtime;
        if (sscanf(line, "%o %64s %llu %u %511s",
                   &e->mode, hex, &mtime, &e->size, e->path) != 5) continue;
        e->mtime_sec = (uint64_t)mtime;
        if (hex_to_hash(hex, &e->hash) < 0) continue;
        index->count++;
    }
    fclose(f);
    return 0;
}

static int compare_entries(const void *a, const void *b) {
    return strcmp(((const IndexEntry *)a)->path, ((const IndexEntry *)b)->path);
}

int index_save(const Index *index) {
    // Use malloc instead of stack to avoid overflow (Index is ~5MB on stack)
    Index *sorted = malloc(sizeof(Index));
    if (!sorted) return -1;
    *sorted = *index;
    qsort(sorted->entries, sorted->count, sizeof(IndexEntry), compare_entries);

    char tmp[256] = ".pes/index_tmp_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) { free(sorted); return -1; }

    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); free(sorted); return -1; }

    for (int i = 0; i < sorted->count; i++) {
        IndexEntry *e = &sorted->entries[i];
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&e->hash, hex);
        fprintf(f, "%o %s %llu %u %s\n",
                e->mode, hex,
                (unsigned long long)e->mtime_sec,
                e->size, e->path);
    }
    fflush(f);
    fsync(fd);
    fclose(f);
    free(sorted);

    if (rename(tmp, INDEX_FILE) < 0) return -1;
    return 0;
}

int index_add(Index *index, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = malloc(sz > 0 ? sz : 1);
    if (!buf) { fclose(f); return -1; }
    if (sz > 0 && (long)fread(buf, 1, sz, f) != sz) {
        fclose(f); free(buf); return -1;
    }
    fclose(f);

    ObjectID id;
    if (object_write(OBJ_BLOB, buf, sz, &id) < 0) { free(buf); return -1; }
    free(buf);

    struct stat st;
    if (stat(path, &st) < 0) return -1;

    IndexEntry *e = index_find(index, path);
    if (!e) {
        if (index->count >= MAX_INDEX_ENTRIES) return -1;
        e = &index->entries[index->count++];
    }
    e->mode = (st.st_mode & S_IXUSR) ? 0100755 : 0100644;
    e->hash = id;
    e->mtime_sec = (uint64_t)st.st_mtime;
    e->size = (uint32_t)sz;
    strncpy(e->path, path, sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = '\0';

    return index_save(index);
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    if (index->count == 0)
        printf("    (nothing to show)\n");
    else
        for (int i = 0; i < index->count; i++)
            printf("    new file:   %s\n", index->entries[i].path);

    printf("\nUnstaged changes:\n");
    int unstaged = 0;
    for (int i = 0; i < index->count; i++) {
        struct stat st;
        if (stat(index->entries[i].path, &st) < 0) {
            printf("    deleted:    %s\n", index->entries[i].path);
            unstaged++;
        } else if ((uint64_t)st.st_mtime != index->entries[i].mtime_sec ||
                   (uint32_t)st.st_size  != index->entries[i].size) {
            printf("    modified:   %s\n", index->entries[i].path);
            unstaged++;
        }
    }
    if (!unstaged) printf("    (nothing to show)\n");

    printf("\nUntracked files:\n");
    printf("    (nothing to show)\n");
    return 0;
}
