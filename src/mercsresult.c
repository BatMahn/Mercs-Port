#include "mercsresult.h"

#include <limits.h>
#include <string.h>

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int32_t read_signed32(const uint8_t *p)
{
    uint32_t value = read32(p);
    /* No implementation-defined unsigned-to-signed conversion. */
    return value <= INT32_MAX ? (int32_t)value :
           -1 - (int32_t)(UINT32_MAX - value);
}

static void write32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

int mercs_car_parse(const uint8_t *bytes, size_t length, mercs_car_record *out)
{
    if (!bytes || !out || length != MERCS_CAR_BYTES) return MERCS_RESULT_INVALID;
    memmove(out->bytes, bytes, MERCS_CAR_BYTES);
    return MERCS_RESULT_OK;
}

int mercs_car_encode(const mercs_car_record *record, uint8_t *out,
                     size_t capacity, size_t *written)
{
    if (written) *written = 0;
    if (!record || !out || !written) return MERCS_RESULT_INVALID;
    if (capacity < MERCS_CAR_BYTES) return MERCS_RESULT_NO_SPACE;
    memmove(out, record->bytes, MERCS_CAR_BYTES);
    *written = MERCS_CAR_BYTES;
    return MERCS_RESULT_OK;
}

int mercs_car_init_dos(mercs_car_record *out)
{
    if (!out) return MERCS_RESULT_INVALID;
    memset(out->bytes, 0, sizeof out->bytes);
    write32(out->bytes + MERCS_CAR_RESULT_INDEX_OFFSET, UINT32_MAX);
    return MERCS_RESULT_OK;
}

int mercs_car_set_player_status(mercs_car_record *record, uint8_t status)
{
    if (!record || (status != 0 && status != 1 && status != 2 && status != 4))
        return MERCS_RESULT_INVALID;
    record->bytes[MERCS_CAR_PLAYER_STATUS_OFFSET] = status;
    return MERCS_RESULT_OK;
}

int mercs_car_player_status(const mercs_car_record *record)
{
    return record ? record->bytes[MERCS_CAR_PLAYER_STATUS_OFFSET] : MERCS_RESULT_INVALID;
}

int mercs_car_note_loss(mercs_car_record *record, mercs_car_loss_context *loss)
{
    if (!record || !loss) return MERCS_RESULT_INVALID;
    if ((loss->is_player && loss->invulnerable && loss->controller_state != 7) ||
        (loss->actor_flags & 6)) return MERCS_RESULT_OK;
    if (loss->controller_state == 5) {
        if (loss->is_player)
            record->bytes[MERCS_CAR_PLAYER_STATUS_OFFSET] =
                loss->hazardous_ejection ? MERCS_CAR_STATUS_DEAD : MERCS_CAR_STATUS_EJECTED;
        if (loss->hazardous_ejection) loss->controller_state = 4;
    } else if (loss->is_player) {
        record->bytes[MERCS_CAR_PLAYER_STATUS_OFFSET] = MERCS_CAR_STATUS_DEAD;
    }
    return MERCS_RESULT_OK;
}

static void car_counter_inc(mercs_car_record *record, size_t offset)
{
    uint16_t value = (uint16_t)((uint16_t)record->bytes[offset] |
                              (uint16_t)record->bytes[offset + 1] << 8);
    value = (uint16_t)(value + 1u);
    record->bytes[offset] = (uint8_t)value;
    record->bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void car_counter_alliance(mercs_car_record *record, size_t friendly,
                                size_t neutral, size_t enemy, uint8_t alliance)
{
    if (alliance == 0) car_counter_inc(record, friendly);
    else if (alliance == 1) car_counter_inc(record, enemy);
    else if (alliance == 2) car_counter_inc(record, neutral);
}

int mercs_car_count_initial(mercs_car_record *record, int is_thing, uint8_t alliance)
{
    size_t off = is_thing ? 0x3e : 0x38;
    if (!record) return MERCS_RESULT_INVALID;
    car_counter_alliance(record, off, off + 2, off + 4, alliance);
    return MERCS_RESULT_OK;
}

int mercs_car_count_unit_loss(mercs_car_record *record, mercs_car_unit_loss *event)
{
    mercs_car_loss_context *loss;
    if (!record || !event) return MERCS_RESULT_INVALID;
    loss = &event->loss;
    if ((loss->is_player && loss->invulnerable && loss->controller_state != 7) ||
        (loss->actor_flags & 6)) return MERCS_RESULT_OK;
    /* The ejection tally precedes the hazardous state5 -> state4 transition. */
    if (event->same_player_star)
        car_counter_inc(record, loss->controller_state == 5 ? 0x36 : 0x34);
    if (event->killer_is_player) {
        if (event->unit_class == 1)
            car_counter_alliance(record, 0x0b, 0x09, 0x07, event->alliance);
        else
            car_counter_alliance(record, 0x48, 0x46, 0x44, event->alliance);
    }
    if (loss->actor_flags & 0x1400) {
        if (event->unit_class == 1)
            car_counter_alliance(record, 0x22, 0x20, 0x1e, event->alliance);
        else
            car_counter_alliance(record, 0x4e, 0x4c, 0x4a, event->alliance);
    }
    return mercs_car_note_loss(record, loss);
}

int mercs_car_count_thing_loss(mercs_car_record *record, uint16_t thing_flags,
                             uint8_t alliance, int killer_is_player,
                             int killer_in_player_star)
{
    if (!record) return MERCS_RESULT_INVALID;
    if (thing_flags & 4) return MERCS_RESULT_OK;
    if (killer_is_player)
        car_counter_alliance(record, 0x11, 0x0f, 0x0d, alliance);
    if (killer_in_player_star)
        car_counter_alliance(record, 0x28, 0x26, 0x24, alliance);
    return MERCS_RESULT_OK;
}

int mercs_car_count_unit_hit(mercs_car_record *record, uint8_t victim_alliance,
                           int source_is_player, int source_in_player_star,
                           int victim_is_player, int victim_in_player_star)
{
    if (!record) return MERCS_RESULT_INVALID;
    if (source_is_player)
        car_counter_alliance(record, 0x19, 0x17, 0x15, victim_alliance);
    if (source_in_player_star)
        car_counter_alliance(record, 0x2c, 0x2e, 0x30, victim_alliance);
    if (victim_is_player) car_counter_inc(record, 0x1b);
    if (victim_in_player_star) car_counter_inc(record, 0x32);
    return MERCS_RESULT_OK;
}

int mercs_car_count_shot(mercs_car_record *record, int source_is_player,
                       int source_in_player_star)
{
    if (!record) return MERCS_RESULT_INVALID;
    if (source_is_player) car_counter_inc(record, 0x13);
    if (source_in_player_star) car_counter_inc(record, 0x2a);
    return MERCS_RESULT_OK;
}

int mercs_msn_parse(const uint8_t *bytes, size_t length, mercs_msn_record *out)
{
    mercs_msn_record record;
    size_t i;
    if (!bytes || !out || length != MERCS_MSN_BYTES ||
        memcmp(bytes, "MW2M", 4) || read32(bytes + 4) > MERCS_MSN_ROWS)
        return MERCS_RESULT_INVALID;
    memset(&record, 0, sizeof record);
    record.row_count = read32(bytes + 4);
    record.started_seconds = read_signed32(bytes + 8);
    record.resolved_seconds = read_signed32(bytes + 12);
    record.outcome = read32(bytes + 16);
    for (i = 0; i < MERCS_MSN_ROWS; i++) {
        const uint8_t *p = bytes + 20 + i * 52;
        mercs_msn_row *row = &record.rows[i];
        row->succeeded = read32(p);
        row->priority = read32(p + 4);
        row->started_seconds = read_signed32(p + 8);
        row->resolved_seconds = read_signed32(p + 12);
        row->objective_value = read32(p + 16);
        memcpy(row->text, p + 20, MERCS_MSN_TEXT_BYTES);
    }
    *out = record;
    return MERCS_RESULT_OK;
}

int mercs_msn_encode(const mercs_msn_record *record, uint8_t *out,
                     size_t capacity, size_t *written)
{
    uint8_t bytes[MERCS_MSN_BYTES];
    size_t i;
    if (written) *written = 0;
    if (!record || !out || !written || record->row_count > MERCS_MSN_ROWS)
        return MERCS_RESULT_INVALID;
    if (capacity < MERCS_MSN_BYTES) return MERCS_RESULT_NO_SPACE;
    memcpy(bytes, "MW2M", 4);
    write32(bytes + 4, record->row_count);
    write32(bytes + 8, (uint32_t)record->started_seconds);
    write32(bytes + 12, (uint32_t)record->resolved_seconds);
    write32(bytes + 16, record->outcome);
    for (i = 0; i < MERCS_MSN_ROWS; i++) {
        uint8_t *p = bytes + 20 + i * 52;
        const mercs_msn_row *row = &record->rows[i];
        write32(p, row->succeeded);
        write32(p + 4, row->priority);
        write32(p + 8, (uint32_t)row->started_seconds);
        write32(p + 12, (uint32_t)row->resolved_seconds);
        write32(p + 16, row->objective_value);
        memcpy(p + 20, row->text, MERCS_MSN_TEXT_BYTES);
    }
    memcpy(out, bytes, sizeof bytes);
    *written = sizeof bytes;
    return MERCS_RESULT_OK;
}

int mercs_msn_encode_selected(const mercs_msn_star *stars, size_t star_count,
                              size_t selected_star, uint8_t *out,
                              size_t capacity, size_t *written)
{
    uint8_t bytes[MERCS_MSN_BYTES];
    const mercs_msn_star *star;
    size_t i, rows = 0;
    if (written) *written = 0;
    if (!stars || !out || !written || selected_star >= star_count)
        return MERCS_RESULT_INVALID;
    star = &stars[selected_star];
    if (star->node_count > MERCS_MSN_ROWS) return MERCS_RESULT_INVALID;
    if (capacity < MERCS_MSN_BYTES) return MERCS_RESULT_NO_SPACE;
    memset(bytes, 0, sizeof bytes);
    memcpy(bytes, "MW2M", 4);
    write32(bytes + 8, (uint32_t)star->started_seconds);
    write32(bytes + 12, (uint32_t)star->resolved_seconds);
    write32(bytes + 16, star->outcome);
    for (i = 0; i < star->node_count; i++) {
        const mercs_msn_node *node = &star->nodes[i];
        uint8_t *row;
        size_t text_offset, length;
        const char *end;
        if (!node->visible) continue;
        end = memchr(node->text, 0, sizeof node->text);
        if (!end) return MERCS_RESULT_INVALID;
        row = bytes + 20 + rows * 52;
        text_offset = (size_t)(row - bytes) + 20;
        length = (size_t)(end - node->text) + 1;
        if (length > sizeof bytes - text_offset) return MERCS_RESULT_INVALID;
        write32(row, node->runtime_state == 5);
        write32(row + 4, node->priority);
        write32(row + 8, (uint32_t)node->started_seconds);
        write32(row + 12, (uint32_t)node->resolved_seconds);
        write32(row + 16, node->objective_value);
        /* DOS 0x1e1df / Windows 0x1004fcb0: the original copies through
         * NUL, including into the following physical row. Keep its write
         * order and bytes, but contain the copy in the staged wire buffer. */
        memcpy(bytes + text_offset, node->text, length);
        rows++;
    }
    write32(bytes + 4, (uint32_t)rows);
    memcpy(out, bytes, sizeof bytes);
    *written = sizeof bytes;
    return MERCS_RESULT_OK;
}
