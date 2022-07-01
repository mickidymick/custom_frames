#include <yed/plugin.h>

static yed_frame *save_frame;
static array_t    custom_frame_buffers;

typedef union {
    struct {
/*         floating */
        float f_height;
        float f_width;
        float f_x;
        float f_y;
    };
    struct {
/*         split */
        char  split_type;
        char  loc;
        char  use_root;
        float size;
        int   heirarchy;
    };
} custom_buffer_frame;

typedef struct {
    int close_after;
    int speed;
} custom_buffer_animation;

typedef struct {
    array_t                  buff_name;
    char                    *frame_name;
    char                     is_split;
    int                      use_animation;
    custom_buffer_frame      max_frame_size;
    custom_buffer_frame      min_frame_size;
    custom_buffer_animation  animation;
} custom_buffer_data;

/* command replacement functions*/
static void _special_buffer_prepare_focus(int n_args, char **args);
static void _special_buffer_prepare_unfocus(int n_args, char **args);
static void _special_buffer_prepare_unfocus_active(int n_args, char **args);

/* internal functions*/
static void _unfocus(char *buffer);
static void _start_tray_animate(int rows);
static void _tray_animate(yed_event *event);
static void _custom_frames_unload(yed_plugin *self);
static void _create_new_frame(char loc, char use_root, float size, int heirarchy);

/* new command functions*/
static void set_custom_buffer_frame(int n_args, char **args);

int yed_plugin_boot(yed_plugin *self) {

    YED_PLUG_VERSION_CHECK();

    custom_frame_buffers = array_make(struct custom_buffer_data *);

    yed_plugin_set_command(self, "special-buffer-prepare-focus", _special_buffer_prepare_focus);
    yed_plugin_set_command(self, "special-buffer-prepare-unfocus", _special_buffer_prepare_unfocus);
    yed_plugin_set_command(self, "special-buffer-prepare-unfocus-active", _special_buffer_prepare_unfocus_active);
    yed_plugin_set_command(self, "set-custom-buffer-frame", set_custom_buffer_frame);

    yed_plugin_set_unload_fn(self, _custom_frames_unload);

    return 0;
}

static void _special_buffer_prepare_focus(int n_args, char **args) {
    custom_buffer_data **data_it;
    char               **data_it_inner;
    yed_frame           *frame;

    if (n_args != 1) {
        yed_cerr("expected 1 argument, but got %d", n_args);
        return;
    }

    if (ys->active_frame == NULL) {
        YEXE("frame-new"); if (ys->active_frame == NULL) { return; }
    }

    int one = 0;
    array_traverse(custom_frame_buffers, data_it) {
        array_traverse((*data_it)->buff_name, data_it_inner) {
            if (strcmp((*data_it_inner), args[0]) == 0) {
                if ((*data_it)->use_animation == 0) {
                    yed_log("%s", (*data_it)->frame_name);
                    frame = yed_find_frame_by_name((*data_it)->frame_name); //broken here
                    return;
                    if (frame == NULL) {
                        frame = yed_add_new_frame((*data_it)->max_frame_size.f_y,
                                                (*data_it)->max_frame_size.f_x,
                                                (*data_it)->max_frame_size.f_height,
                                                (*data_it)->max_frame_size.f_width);
                        yed_frame_set_name(frame, (*data_it)->frame_name);
                    }
                    save_frame = ys->active_frame;
                    yed_activate_frame(frame);
                } else {
                    frame = yed_find_frame_by_name((*data_it)->frame_name);
                    if (frame == NULL) {
                        frame = yed_add_new_frame((*data_it)->min_frame_size.f_y,
                                                (*data_it)->min_frame_size.f_x,
                                                (*data_it)->min_frame_size.f_height,
                                                (*data_it)->min_frame_size.f_width);
                        yed_frame_set_name(frame, (*data_it)->frame_name);
                    }
                    save_frame = ys->active_frame;
                    yed_activate_frame(frame);
                }
                return;
            }
        }
    }
}

static void _unfocus(char *buffer) {
    custom_buffer_data **data_it;
    char               **data_it_inner;

    array_traverse(custom_frame_buffers, data_it) {
        array_traverse((*data_it)->buff_name, data_it_inner) {
            if (strcmp((*data_it_inner), buffer) == 0 &&
                    (*data_it)->frame_name == (ys->active_frame->name)) {
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
                yed_delete_frame((yed_frame *)(*data_it)->frame_name);
                break;
            }
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

static void set_custom_buffer_frame(int n_args, char **args) {
    custom_buffer_data *data = malloc(sizeof(custom_buffer_data));
    char               *buff_arr;

/*     check for either split or float */
    if (n_args < 2) {
        yed_cerr("Expected at least 2 arguments, but got %d", n_args);
        free(data);
        return;
    }

    data->frame_name = strdup(args[0]);
    data->is_split   = args[1][0];

    if (data->is_split != 's' && data->is_split != 'f') {
        yed_cerr("Expected 2nd argument must be either s or f");
        free(data);
        return;
    }

    if (data->is_split == 'f') {
        if (n_args < 6) {
            yed_cerr("Expected at least 6 arguments, but got %d", n_args);
            free(data);
            return;
        }
        data->max_frame_size.f_x      = atof(args[2]);
        data->max_frame_size.f_y      = atof(args[3]);
        data->max_frame_size.f_width  = atof(args[4]);
        data->max_frame_size.f_height = atof(args[5]);

        if (data->max_frame_size.f_x < 0      || data->max_frame_size.f_x > 1     ||
            data->max_frame_size.f_y < 0      || data->max_frame_size.f_y > 1     ||
            data->max_frame_size.f_width < 0  || data->max_frame_size.f_width > 1 ||
            data->max_frame_size.f_height < 0 || data->max_frame_size.f_height > 1) {
            yed_cerr("Expected arguments 3 - 6 to be between 0 - 1");
            free(data);
            return;
        }

        if (n_args < 7) {
            yed_cerr("Expected at least 7 arguments, but got %d", n_args);
            free(data);
            return;
        }
        data->use_animation = atoi(args[6]);

        if (data->use_animation < 0 || data->use_animation > 1) {
            yed_cerr("Expected argument 7 to be either 0 or 1");
            free(data);
            return;
        }

        if (data->use_animation) {
            if (n_args < 14) {
                yed_cerr("Expected at least 14 arguments, but got %d", n_args);
                free(data);
                return;
            }
            data->min_frame_size.f_y      = atof(args[7]);
            data->min_frame_size.f_x      = atof(args[8]);
            data->min_frame_size.f_height = atof(args[9]);
            data->min_frame_size.f_width  = atof(args[10]);

            if (data->min_frame_size.f_x < 0      || data->min_frame_size.f_x > 1     ||
                data->min_frame_size.f_y < 0      || data->min_frame_size.f_y > 1     ||
                data->min_frame_size.f_width < 0  || data->min_frame_size.f_width > 1 ||
                data->min_frame_size.f_height < 0 || data->min_frame_size.f_height > 1) {
                yed_cerr("Expected arguments 8 - 11 to be between 0 - 1");
                free(data);
                return;
            }

            data->animation.close_after = atoi(args[11]);
            data->animation.speed       = atoi(args[12]);

            if (data->animation.close_after < 0 || data->animation.close_after > 1) {
                yed_cerr("Expected argument 12 to be either 0 or 1");
                free(data);
                return;
            }

            buff_arr = strdup(args[13]);
        } else {
            if (n_args < 8) {
                yed_cerr("Expected at least 8 arguments, but got %d", n_args);
                free(data);
                return;
            }

            buff_arr = strdup(args[7]);
        }

    } else {
        if (n_args < 7) {
            yed_cerr("Expected at least 7 arguments, but got %d", n_args);
            free(data);
            return;
        }
        data->max_frame_size.split_type = args[2][0];
        data->max_frame_size.loc        = args[3][0];

        if ((data->max_frame_size.split_type == 'h' && data->max_frame_size.loc != 't') ||
            (data->max_frame_size.split_type == 'h' && data->max_frame_size.loc != 'b') ||
            (data->max_frame_size.split_type == 'v' && data->max_frame_size.loc != 'l') ||
            (data->max_frame_size.split_type == 'v' && data->max_frame_size.loc != 'r')) {
            yed_cerr("Expected 3rd argument to be h and 4th to be either t or b or \n 3rd argument to be v and 4th to be l or r");
            free(data);
            return;
        }

        data->max_frame_size.use_root = args[4][0];
        if (data->max_frame_size.use_root < 0 || data->max_frame_size.use_root > 1) {
            yed_cerr("Expected 5th argument to be either 0 - 1");
            free(data);
            return;
        }

        data->max_frame_size.size = atof(args[5]);
        if (data->max_frame_size.size < 0 || data->max_frame_size.size > 1) {
            yed_cerr("Expected 6th argument to be between 0 - 1");
            free(data);
            return;
        }

        data->max_frame_size.heirarchy = atoi(args[6]);

        if (n_args < 8) {
            yed_cerr("Expected at least 8 arguments, but got %d", n_args);
            free(data);
            return;
        }
        data->use_animation = atoi(args[7]);

        if (data->use_animation < 0 || data->use_animation > 1) {
            yed_cerr("Expected 8th argument to be either 0 or 1");
            free(data);
            return;
        }

        if (data->use_animation) {
            if (n_args < 15) {
                yed_cerr("Expected at least 15 arguments, but got %d", n_args);
                free(data);
                return;
            }

            data->min_frame_size.split_type = args[8][0];
            data->min_frame_size.loc        = args[9][0];

            if ((data->min_frame_size.split_type == 'h' && data->min_frame_size.loc != 't') ||
                (data->min_frame_size.split_type == 'h' && data->min_frame_size.loc != 'b') ||
                (data->min_frame_size.split_type == 'v' && data->min_frame_size.loc != 'l') ||
                (data->min_frame_size.split_type == 'v' && data->min_frame_size.loc != 'r')) {
                yed_cerr("Expected 9th argument to be h and 10th to be either t or b or \n 9th argument to be v and 10th to be l or r");
                free(data);
                return;
            }

            data->min_frame_size.use_root   = args[10][0];
            if (data->min_frame_size.use_root == 'r' && data->min_frame_size.use_root != 'a') {
                yed_cerr("Expected 11th argument to be either r or a");
                free(data);
                return;
            }

            data->min_frame_size.size      = atof(args[11]);
            data->min_frame_size.heirarchy = -1;

            data->animation.close_after = atoi(args[12]);
            if (data->animation.close_after < 0 || data->animation.close_after > 1) {
                yed_cerr("Expected 13th argument to be either 0 or 1");
                free(data);
                return;
            }

            data->animation.speed = atoi(args[13]);
            buff_arr = strdup(args[14]);
        } else {
            if (n_args < 9) {
                yed_cerr("Expected at least 9 arguments, but got %d", n_args);
                free(data);
                return;
            }
            buff_arr = strdup(args[8]);
        }
    }

    data->buff_name = array_make(char *);
    char *token;
    char *tmp;
    const char s[2] = " ";
    token = strtok(buff_arr, s);

    while (token != NULL) {
        tmp = strdup(token); array_push(data->buff_name, tmp);
        token = strtok(NULL, s);
    }

    array_push(custom_frame_buffers, data);
    free(buff_arr);

    for (int i=0; i<n_args; i++) {
        yed_log("%s ", args[i]);
    }
}

static void _create_new_frame(char loc, char use_root, float size, int heirarchy) {
    if (ys->active_frame == NULL) {
        YEXE("frame-new"); if (ys->active_frame == NULL) { return; }
    }

}

static void _custom_frames_unload(yed_plugin *self) {
    while (array_len(custom_frame_buffers) > 0) {
        array_pop(custom_frame_buffers);
    }

    array_free(custom_frame_buffers);
}
