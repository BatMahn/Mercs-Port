/* Mercs texture bindings in the expanded BWD stream. DOS 0x453d0/0x454dc,
 * retail Windows 0x1005f462/0x1005f5c6. The loader's record inventory is
 * breadth-first; bindings must instead follow original include/chunk order. */
#ifndef MERCS_TEXMAP_H
#define MERCS_TEXMAP_H
#include "bwdplace.h"
#include <string.h>

typedef struct {
    int cel[32];
    int frames, rate, mode, bound;
} mercs_texbinding;

typedef struct {
    prj_archive *archive;
    mercs_texbinding *slots;
    int group[32], frames, closed, group_index;
} mercs_texmap_reader;

static inline void mercs_texbinding_mode(mercs_texbinding *slot, int mode)
{
    if (slot->bound) slot->mode = (int16_t)mode;
    if (!slot->rate) slot->rate = 45;
}

static inline int mercs_texmap_chunk(const bwd_mission *mission, int record,
                                     int chunk, const bwd_transform *placement,
                                     void *user)
{
    mercs_texmap_reader *reader = user;
    const bwd_chunk *c;
    int id;
    (void)placement;
    if (chunk < 0) return 0;
    c = &mission->records[record].chunks[chunk];
    if (c->size < 2) return 0;
    id = (int16_t)((unsigned)c->data[0] | (unsigned)c->data[1] << 8);
    if (!strcmp(c->tag, "BMPJ")) {
        if (reader->closed) {
            reader->group_index++;
            reader->closed = reader->frames = 0;
            memset(reader->group, 0, sizeof reader->group);
        }
        if (id == -1 && reader->archive && c->size > 2) {
            char name[17];
            size_t n = c->size - 2;
            if (n > sizeof name - 1) n = sizeof name - 1;
            memcpy(name, c->data + 2, n); name[n] = 0;
            id = prj_find_id(reader->archive,
                             prj_find_type(reader->archive, "CEL"), name);
        }
        if (id > 0 && reader->group_index < 512 && reader->frames < 32)
            reader->group[reader->frames++] = id;
    } else if (!strcmp(c->tag, "BMID")) {
        reader->closed = 1;
        if (id >= 0 && id < 512 && reader->group_index < 512) {
            mercs_texbinding *slot = &reader->slots[id];
            memcpy(slot->cel, reader->group, sizeof slot->cel);
            slot->frames = reader->frames;
            slot->mode = slot->bound = 1;
            /* A single-frame rebind retains the previous rate. */
            if (reader->frames > 1) slot->rate = 45;
        }
    } else if (id >= 0 && id < 512 && c->size >= 4 &&
               (!strcmp(c->tag, "BSEC") || !strcmp(c->tag, "BMEN"))) {
        int value = (int16_t)((unsigned)c->data[2] | (unsigned)c->data[3] << 8);
        if (!strcmp(c->tag, "BSEC")) {
            reader->slots[id].rate = (uint16_t)value;
            mercs_texbinding_mode(&reader->slots[id], 1);
        } else mercs_texbinding_mode(&reader->slots[id], value);
    } else if (!strcmp(c->tag, "XPLO") && c->size >= 6) {
        int control = (int16_t)((unsigned)c->data[2] | (unsigned)c->data[3] << 8);
        int type = (int16_t)((unsigned)c->data[4] | (unsigned)c->data[5] << 8);
        if (type >= 0 && type < 36 && control > 0 && control < 512)
            mercs_texbinding_mode(&reader->slots[control], 0);
    }
    return 0;
}

static inline int mercs_texmap_read(prj_archive *archive,
                                    const bwd_mission *mission,
                                    mercs_texbinding slots[512])
{
    mercs_texmap_reader reader;
    memset(slots, 0, 512 * sizeof *slots);
    memset(&reader, 0, sizeof reader);
    reader.archive = archive; reader.slots = slots;
    return bwd_place_walk(mission, mercs_texmap_chunk, &reader);
}
#endif
