#include <stdio.h>

#include <SDL.h>

#include "game/camera.h"
#include "system/stacktrace.h"
#include "player_layer.h"
#include "system/nth_alloc.h"
#include "system/log.h"
#include "undo_history.h"

PlayerLayer create_player_layer(Vec position, Color color)
{
    return (PlayerLayer) {
        .position = position,
        .color_picker = create_color_picker_from_rgba(color),
        .prev_color = color
    };
}

PlayerLayer create_player_layer_from_line_stream(LineStream *line_stream)
{
    trace_assert(line_stream);

    const char *line = line_stream_next(line_stream);
    trace_assert(line);

    char colorstr[7] = "000000";
    Point position = vec(0.0f, 0.0f);

    const int bound =
        sscanf(line, "%f%f%6s", &position.x, &position.y, colorstr);

#define BOUND_EXPECTED 3
    if (bound != BOUND_EXPECTED) {
        log_fail("Could not read Player Layer properly. Parsed tokens: %d. Expected: %d\n",
                 bound, BOUND_EXPECTED);
    }
#undef BOUND_EXPECTED

    return create_player_layer(position, hexstr(colorstr));
}

LayerPtr player_layer_as_layer(PlayerLayer *player_layer)
{
    LayerPtr layer = {
        .type = LAYER_PLAYER,
        .ptr = player_layer
    };
    return layer;
}

int player_layer_render(const PlayerLayer *player_layer,
                        Camera *camera,
                        int active)
{
    trace_assert(player_layer);
    trace_assert(camera);

    if (camera_fill_rect(
            camera,
            rect_from_vecs(
                player_layer->position,
                vec(25.0f, 25.0f)),
            color_scale(
                color_picker_rgba(&player_layer->color_picker),
                rgba(1.0f, 1.0f, 1.0f, active ? 1.0f : 0.5f))) < 0) {
        return -1;
    }

    if (active && color_picker_render(
            &player_layer->color_picker,
            camera)) {
        return -1;
    }

    return 0;
}

static void player_layer_revert_position(void *layer, Context context)
{
    trace_assert(layer);

    PlayerLayer *player_layer = layer;
    player_layer->position = *((Point*)context.data);
}

static
void player_layer_revert_color(void *layer, Context context)
{
    trace_assert(layer);
    PlayerLayer *player_layer = layer;

    trace_assert(sizeof(Color) <= CONTEXT_SIZE);
    Color *color = (Color *)context.data;

    player_layer->color_picker = create_color_picker_from_rgba(*color);
    player_layer->prev_color = *color;
}

int player_layer_event(PlayerLayer *player_layer,
                       const SDL_Event *event,
                       const Camera *camera,
                       UndoHistory *undo_history)
{
    trace_assert(player_layer);
    trace_assert(event);
    trace_assert(camera);
    trace_assert(undo_history);

    int selected = 0;
    if (color_picker_event(
            &player_layer->color_picker,
            event,
            camera,
            &selected) < 0) {
        return -1;
    }

    if (selected && !color_picker_drag(&player_layer->color_picker)) {
        undo_history_push(
            undo_history,
            create_action(
                player_layer,
                player_layer_revert_color,
                &player_layer->prev_color,
                sizeof(player_layer->prev_color)));
        player_layer->prev_color = color_picker_rgba(&player_layer->color_picker);
    }

    if (!selected &&
        event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT) {

        undo_history_push(
            undo_history,
            create_action(
                player_layer,
                player_layer_revert_position,
                &player_layer->position,
                sizeof(player_layer->position)));

        player_layer->position =
            camera_map_screen(camera,
                              event->button.x,
                              event->button.y);
    }

    return 0;
}

int player_layer_dump_stream(const PlayerLayer *player_layer,
                             FILE *filedump)
{
    trace_assert(player_layer);
    trace_assert(filedump);

    fprintf(filedump, "%f %f ", player_layer->position.x, player_layer->position.y);
    color_hex_to_stream(color_picker_rgba(&player_layer->color_picker), filedump);
    fprintf(filedump, "\n");

    return 0;
}
