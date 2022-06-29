#include <yed/plugin.h>

static yed_frame *save_frame;
static array_t    custom_frame_buffers;

struct custom_buffer_frame {
    float f_height;
    float f_width;
    float f_x;
    float f_y;
};

struct custom_buffer_animation {
    int close_after;
};

struct custom_buffer_data {
    u64                           *frame_name; //just the address at the moment
    char                          *buff_name;
    struct custom_buffer_frame     max_frame_size;
    struct custom_buffer_frame     min_frame_size;
    int                            use_animation;
    struct custom_buffer_animation animation;
};

/* internal functions */
static void _special_buffer_prepare_focus(int n_args, char **args);
static void _special_buffer_prepare_unfocus(int n_args, char **args);
static void _special_buffer_prepare_unfocus_active(int n_args, char **args);
static void _unfocus(char *buffer);
static void _set_custom_buffer_frame(int n_args, char **args);
static void _start_tray_animate(int rows);
static void _tray_animate(yed_event *event);
static void _custom_frames_unload(yed_plugin *self);

int yed_plugin_boot(yed_plugin *self) {

    YED_PLUG_VERSION_CHECK();

    custom_frame_buffers = array_make(struct custom_buffer_data *);

    yed_plugin_set_command(self, "special-buffer-prepare-focus", _special_buffer_prepare_focus);
    yed_plugin_set_command(self, "special-buffer-prepare-unfocus", _special_buffer_prepare_unfocus);
    yed_plugin_set_command(self, "special-buffer-prepare-unfocus-active", _special_buffer_prepare_unfocus_active);
    yed_plugin_set_command(self, "set-custom-buffer-frame", _set_custom_buffer_frame);

    yed_plugin_set_unload_fn(self, _custom_frames_unload);

    return 0;
}

static void _special_buffer_prepare_focus(int n_args, char **args) {
    struct custom_buffer_data **data_it;
    yed_frame *frame;

    if (n_args != 1) {
        yed_cerr("expected 1 argument, but got %d", n_args);
        return;
    }

    if (ys->active_frame == NULL) {
        YEXE("frame-new"); if (ys->active_frame == NULL) { return; }
    }

    array_traverse(custom_frame_buffers, data_it) {
        if (strcmp((*data_it)->buff_name, args[0]) == 0) {
            if ((*data_it)->use_animation == 0) {
                frame = yed_add_new_frame((*data_it)->max_frame_size.f_y,
                                          (*data_it)->max_frame_size.f_x,
                                          (*data_it)->max_frame_size.f_height,
                                          (*data_it)->max_frame_size.f_width);
                save_frame = ys->active_frame;
                yed_activate_frame(frame);
                (*data_it)->frame_name = (u64 *)frame;
            } else {
                frame = yed_add_new_frame((*data_it)->min_frame_size.f_y,
                                          (*data_it)->min_frame_size.f_x,
                                          (*data_it)->min_frame_size.f_height,
                                          (*data_it)->min_frame_size.f_width);
                save_frame = ys->active_frame;
                yed_activate_frame(frame);
                (*data_it)->frame_name = (u64 *)frame;
            }
            return;
        }
    }
}

static void _unfocus(char *buffer) {
    struct custom_buffer_data **data_it;
    yed_log("%s\n", buffer);

    array_traverse(custom_frame_buffers, data_it) {
        if (strcmp((*data_it)->buff_name, buffer) == 0 &&
                (*data_it)->frame_name == (u64 *)(ys->active_frame)) {
            if ((*data_it)->use_animation == 0) {
                yed_log("woo");
                if (save_frame != NULL) {
                    yed_activate_frame(save_frame);
                }
            } else {
                if (save_frame != NULL) {
                    yed_activate_frame(save_frame);
                }
            }
            yed_delete_frame((*data_it)->frame_name);
            break;
        }
    }

    save_frame = NULL;
}

static void _special_buffer_prepare_unfocus(int n_args, char **args) {

    if (n_args != 1) {
        yed_cerr("expected 1 argument, but got %d", n_args);
        return;
    }

    _unfocus(args[0]);
}

static void _special_buffer_prepare_unfocus_active(int n_args, char **args) {
    if (ys->active_frame == NULL || ys->active_frame->buffer == NULL) {
        return;
    }

    _unfocus(ys->active_frame->buffer->name);
}

static void _set_custom_buffer_frame(int n_args, char **args) {

    if (n_args != 6) {
        yed_cerr("expected 5 arguments, but got %d", n_args);
        return;
    }

    struct custom_buffer_data *data = malloc(sizeof(struct custom_buffer_data));

    data->frame_name              = NULL;
    data->buff_name               = strdup(args[0]);
    data->max_frame_size.f_y      = atof(args[1]);
    data->max_frame_size.f_x      = atof(args[2]);
    data->max_frame_size.f_height = atof(args[3]);
    data->max_frame_size.f_width  = atof(args[4]);
    data->use_animation           = atoi(args[5]);

    if (data->use_animation) {
        if (n_args != 10) {
            yed_cerr("expected 10 arguments, but got %d", n_args);
            return;
        }

        data->min_frame_size.f_y      = atof(args[6]);
        data->min_frame_size.f_x      = atof(args[7]);
        data->min_frame_size.f_height = atof(args[8]);
        data->min_frame_size.f_width  = atof(args[9]);

        data->animation.close_after   = 0;
    } else {
        data->min_frame_size.f_y      = 0.0;
        data->min_frame_size.f_x      = 0.0;
        data->min_frame_size.f_height = 0.0;
        data->min_frame_size.f_width  = 0.0;

        data->animation.close_after   = 0;
    }

    array_push(custom_frame_buffers, data);
}

static void _custom_frames_unload(yed_plugin *self) {
    while (array_len(custom_frame_buffers) > 0) {
        array_pop(custom_frame_buffers);
    }

    array_free(custom_frame_buffers);
}
