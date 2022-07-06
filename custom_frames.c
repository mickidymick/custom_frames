#include <yed/plugin.h>

typedef struct {
    char *frame_name;
    int   r_c;
    int   want_size;
    int   s_l;
    int   speed;
    int   done;
} current_animation;

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
    int speed;

/*     internal */
    int _max_r_c;
    int _min_r_c;
    int r_c;
} custom_buffer_animation;

typedef struct {
    array_t                  buff_name;
    char                    *frame_name;
    char                     is_split;
    int                      use_animation;
    int                      close_after;
    custom_buffer_frame      max_frame_size;
    custom_buffer_frame      min_frame_size;
    custom_buffer_animation  animation;
} custom_buffer_data;

static yed_plugin       *Self;
static yed_frame        *save_frame;
static array_t           custom_frame_buffers;
static array_t           current_animations;
static int               animating;
static yed_event_handler epump_anim;
static yed_event_handler eframe_act;
static int               save_hz;

/* command replacement functions*/
static void            _special_buffer_prepare_focus(int n_args, char **args);
static void            _special_buffer_prepare_unfocus(int n_args, char **args);
static void            _special_buffer_prepare_unfocus_active(int n_args, char **args);

/* internal functions*/
static void            _unfocus(char *buffer);
static void            _start_tray_animate(int rows);
static void            _tray_animate(yed_event *event);
static void            _custom_frames_unload(yed_plugin *self);
static void            _create_new_frame(char loc, char use_root, float size, int heirarchy);
static void            _start_frame_animate(void);
static void            _frame_animate(yed_event *event);
static void            _frame_animate_on_change(yed_event *event);
static yed_frame_tree *_find_tree_from_heirarchy(yed_frame_tree *root, int heirarchy);
static void            _start(custom_buffer_data **data_it, int min);

/* new command functions*/
static void            set_custom_buffer_frame(int n_args, char **args);

int yed_plugin_boot(yed_plugin *self) {

    YED_PLUG_VERSION_CHECK();

    Self = self;

    custom_frame_buffers = array_make(custom_buffer_data *);
    current_animations   = array_make(current_animation);

    eframe_act.kind = EVENT_FRAME_PRE_ACTIVATE;
    eframe_act.fn   = _frame_animate_on_change;

    save_hz = 0;

    yed_plugin_add_event_handler(Self, eframe_act);

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
    yed_frame_tree      *frame_tree;
    yed_frame           *frame;
    int                  rows, cols;

    if (n_args != 1) {
        yed_cerr("expected 1 argument, but got %d", n_args);
        return;
    }

    if (ys->active_frame == NULL) {
        YEXE("frame-new"); if (ys->active_frame == NULL) { return; }
    }

    array_traverse(custom_frame_buffers, data_it) {
        array_traverse((*data_it)->buff_name, data_it_inner) {
            if (strcmp((*data_it_inner), args[0]) == 0) {

                if ((*data_it)->is_split == 'f') {
                    frame = yed_find_frame_by_name((*data_it)->frame_name); //broken here
                    if (frame == NULL) {
                        frame = yed_add_new_frame((*data_it)->max_frame_size.f_y,
                                                  (*data_it)->max_frame_size.f_x,
                                                  (*data_it)->max_frame_size.f_height,
                                                  (*data_it)->max_frame_size.f_width);
                        yed_frame_set_name(frame, (*data_it)->frame_name);
                    }
                    save_frame = ys->active_frame;
                } else {
                    frame = yed_find_frame_by_name((*data_it)->frame_name);

                    if (frame == NULL) {
                        if ((*data_it)->max_frame_size.split_type == 'v') {
                            if ((*data_it)->max_frame_size.loc == 'l') {
                                if ((*data_it)->max_frame_size.use_root == 'r') {
                                    frame_tree = _find_tree_from_heirarchy(yed_frame_tree_get_root(ys->active_frame->tree), (*data_it)->max_frame_size.heirarchy);
                                } else {
                                    frame_tree = ys->active_frame->tree;
                                }
                                frame_tree = yed_frame_tree_vsplit(frame_tree);
                                frame = frame_tree->child_trees[1]->frame;

                                yed_frame_tree_swap_children(frame_tree);
                            } else {
                                if ((*data_it)->max_frame_size.use_root == 'r') {
                                    frame_tree = _find_tree_from_heirarchy(yed_frame_tree_get_root(ys->active_frame->tree), (*data_it)->max_frame_size.heirarchy);
                                } else {
                                    frame_tree = ys->active_frame->tree;
                                }
                                frame_tree = yed_frame_tree_vsplit(frame_tree);
                                frame = frame_tree->child_trees[1]->frame;
                            }

                            cols = ys->term_cols * (*data_it)->max_frame_size.size;
                            yed_resize_frame(frame, 0, cols - frame->width);
                        } else {
                            if ((*data_it)->max_frame_size.loc == 't') {
                                if ((*data_it)->max_frame_size.use_root == 'r') {
                                    frame_tree = _find_tree_from_heirarchy(yed_frame_tree_get_root(ys->active_frame->tree), (*data_it)->max_frame_size.heirarchy);
                                } else {
                                    frame_tree = ys->active_frame->tree;
                                }
                                frame_tree = yed_frame_tree_hsplit(frame_tree);
                                frame = frame_tree->child_trees[1]->frame;

                                yed_frame_tree_swap_children(frame_tree);
                            } else {
                                if ((*data_it)->max_frame_size.use_root == 'r') {
                                    frame_tree = _find_tree_from_heirarchy(yed_frame_tree_get_root(ys->active_frame->tree), (*data_it)->max_frame_size.heirarchy);
                                } else {
                                    frame_tree = ys->active_frame->tree;
                                }
                                frame_tree = yed_frame_tree_hsplit(frame_tree);
                                frame = frame_tree->child_trees[1]->frame;
                            }

                            rows = ys->term_rows * (*data_it)->max_frame_size.size;
                            yed_resize_frame(frame, rows - frame->height, 0);
                        }

                        yed_frame_set_name(frame, (*data_it)->frame_name);
                    }
                    save_frame = ys->active_frame;
                }

                if ((*data_it)->use_animation) {
                    if ((*data_it)->max_frame_size.split_type == 'v') {
                        cols = ys->term_cols * (*data_it)->min_frame_size.size;
                        yed_resize_frame(frame, 0, cols - frame->width);
                    } else {
                        rows = ys->term_rows * (*data_it)->min_frame_size.size;
                        yed_resize_frame(frame, rows - frame->height, 0);
                    }
                }
                yed_activate_frame(frame);

                return;
            }
        }
    }
}

static void _search(yed_frame_tree *curr_frame_tree, yed_frame_tree *saved_frame_tree, int *largest_smaller_heirarchy, int heirarchy) {
    custom_buffer_data **data_it;

    if (curr_frame_tree->is_leaf) { return; }

    if (curr_frame_tree              &&
        curr_frame_tree->child_trees &&
        curr_frame_tree->child_trees[0]) {

        if (curr_frame_tree->child_trees[0]->is_leaf) {
            if (curr_frame_tree->child_trees[0]->frame   &&
                curr_frame_tree->child_trees[0]->frame->name){

                array_traverse(custom_frame_buffers, data_it) {
                    if (strcmp((*data_it)->frame_name, curr_frame_tree->child_trees[0]->frame->name) == 0) {
                        if ((*data_it)->max_frame_size.heirarchy < heirarchy && heirarchy > *largest_smaller_heirarchy) {
                            memcpy(largest_smaller_heirarchy, &heirarchy, sizeof(int));
                            memcpy(saved_frame_tree, curr_frame_tree->child_trees[0], sizeof(yed_frame_tree));
                        }
                        break;
                    }
                }
            }
        } else {
            _search(curr_frame_tree->child_trees[0], saved_frame_tree, largest_smaller_heirarchy, heirarchy);
        }
    }

    if (curr_frame_tree              &&
        curr_frame_tree->child_trees &&
        curr_frame_tree->child_trees[1]) {

        if (curr_frame_tree->child_trees[1]->is_leaf) {
            if (curr_frame_tree->child_trees[1]->frame   &&
                curr_frame_tree->child_trees[1]->frame->name){

                array_traverse(custom_frame_buffers, data_it) {
                    if (strcmp((*data_it)->frame_name, curr_frame_tree->child_trees[1]->frame->name) == 0) {
                        if ((*data_it)->max_frame_size.heirarchy < heirarchy && heirarchy > *largest_smaller_heirarchy) {
                            memcpy(largest_smaller_heirarchy, &heirarchy, sizeof(int));
                            memcpy(saved_frame_tree, curr_frame_tree->child_trees[1], sizeof(yed_frame_tree));
                        }
                        break;
                    }
                }
            }
        } else {
            _search(curr_frame_tree->child_trees[1], saved_frame_tree, largest_smaller_heirarchy, heirarchy);
        }
    }
}

static yed_frame_tree *_find_tree_from_heirarchy(yed_frame_tree *root, int heirarchy) {
    yed_frame_tree      *frame_tree;
    yed_frame_tree      *saved_frame_tree;
    yed_frame           *frame;
    int                  largest_smaller_heirarchy = -1;
    int                  done = 0;

    frame_tree = root;
    saved_frame_tree = malloc(sizeof(yed_frame_tree));

    _search(root, saved_frame_tree, &largest_smaller_heirarchy, heirarchy);

    if (saved_frame_tree && largest_smaller_heirarchy > -1) {
        if (saved_frame_tree->parent) {
            if (saved_frame_tree->parent->child_trees[0] == saved_frame_tree) {
                frame_tree = saved_frame_tree->parent->child_trees[1];
            } else {
                frame_tree = saved_frame_tree->parent->child_trees[0];
            }
        } else {
            frame_tree = saved_frame_tree;
        }
    }

    return frame_tree;
}

static void _add_frame_to_animate(int want_size, int speed, char *frame_name, int r_c, int s_l) {
    current_animation *curr_anim_it;
    current_animation  curr_anim;
    yed_frame          *frame;

    memset(&curr_anim, 0, sizeof(current_animation));
    curr_anim.want_size  = want_size;
    curr_anim.frame_name = frame_name;
    curr_anim.r_c        = r_c;
    curr_anim.s_l        = s_l;
    curr_anim.speed      = speed;
    curr_anim.done       = 0;

    frame = yed_find_frame_by_name(curr_anim.frame_name);
    if (!frame) { return; }

    array_traverse(current_animations, curr_anim_it) {
        if (curr_anim_it->frame_name == frame_name) {
            memcpy(curr_anim_it, &curr_anim, sizeof(current_animation));
            return;
        }
    }

    array_push(current_animations, curr_anim);
}

static void _start_frame_animate(void) {
    int                speed;
    current_animation *curr_anim;

    if (animating) { return; }

    epump_anim.kind = EVENT_PRE_PUMP;
    epump_anim.fn   = _frame_animate;

    if (array_len(current_animations) == 0) {
        save_hz = yed_get_update_hz();
    }

    speed = 0;

    array_traverse(current_animations, curr_anim) {
        if (curr_anim->speed > speed) {
            speed = curr_anim->speed;
        }
    }

    yed_set_update_hz(speed);
    yed_plugin_add_event_handler(Self, epump_anim);
}

static void _frame_animate(yed_event *event) {
    yed_frame         *frame;
    yed_frame_tree    *other;
    int                curr_size;
    current_animation *curr_anim;

    array_traverse(current_animations, curr_anim) {

        if (curr_anim->frame_name == NULL) {
            continue;
        }

        frame = yed_find_frame_by_name(curr_anim->frame_name);

        if (!frame) {
            continue;
        }

        if (curr_anim->r_c) {
            curr_size = frame->width;
        } else {
            curr_size = frame->height;
        }

        if ((!curr_anim->s_l && (curr_size <= curr_anim->want_size)) ||
            ( curr_anim->s_l && (curr_size >= curr_anim->want_size))) {
            curr_anim->done = 1;
            continue;
        }

        if (frame->tree->parent == NULL) {
            continue;
        }

        if (frame->tree->parent->child_trees[0] == frame->tree) {
            if (curr_anim->r_c) {
                yed_resize_frame(frame, 0, (curr_anim->want_size - curr_size) < 0 ? -1 : 1);
            } else {
                yed_resize_frame(frame, (curr_anim->want_size - curr_size) < 0 ? -1 : 1, 0);
            }
        } else {
            other = frame->tree->parent->child_trees[0];
            if (curr_anim->r_c) {
                yed_resize_frame_tree(other, 0, (curr_anim->want_size - curr_size) < 0 ? 1 : -1);
            } else {
                yed_resize_frame_tree(other, (curr_anim->want_size - curr_size) < 0 ? 1 : -1, 0);
            }
        }
    }

check_again:;
    int indx = 0;
    array_traverse(current_animations, curr_anim) {
        if (curr_anim->done == 1) {
            array_delete(current_animations, indx);
            goto check_again;
        }
        indx++;
    }

    if (array_len(current_animations) == 0) {
        yed_set_update_hz(save_hz);
        yed_delete_event_handler(epump_anim);
    }
}

static void _frame_animate_on_change(yed_event *event) {
    custom_buffer_data **data_it;
    yed_frame           *frame;
    current_animation   *curr_anim;

    if (!(ys->active_frame) || !event || !(event->frame)) { return; }

    if (event->frame->name && ys->active_frame->name && strcmp(event->frame->name, ys->active_frame->name) == 0) {
        if (event->frame->name) {
            array_traverse(custom_frame_buffers, data_it) {
                if (strcmp((*data_it)->frame_name, event->frame->name) == 0) {
                    if ((*data_it)->use_animation == 1) {
                        _start(data_it, 0);
                    }

                    if ((*data_it)->close_after) {
                        array_traverse(current_animations, curr_anim) {
                            frame = yed_find_frame_by_name(curr_anim->frame_name);
                        }
                        if (frame) { yed_delete_frame(frame); }
                    }
                    break;
                }
            }
        }
    } else {
        if (event->frame->name) {
            array_traverse(custom_frame_buffers, data_it) {
                if (strcmp((*data_it)->frame_name, event->frame->name) == 0) {
                    if ((*data_it)->use_animation == 1) {
                        _start(data_it, 0);
                    }

                    if ((*data_it)->close_after) {
                        array_traverse(current_animations, curr_anim) {
                            frame = yed_find_frame_by_name(curr_anim->frame_name);
                        }
                        if (frame) { yed_delete_frame(frame); }
                    }
                    break;
                }
            }
        }

        if (ys->active_frame->name) {
            array_traverse(custom_frame_buffers, data_it) {
                if (strcmp((*data_it)->frame_name, ys->active_frame->name) == 0) {
                    if ((*data_it)->use_animation == 1) {
                        _start(data_it, 1);
                    }

                    if ((*data_it)->close_after) {
                        array_traverse(current_animations, curr_anim) {
                            frame = yed_find_frame_by_name(curr_anim->frame_name);
                        }
                        if (frame) { yed_delete_frame(frame); }
                    }
                    break;
                }
            }
        }
    }

_start_frame_animate();
}

static void _start(custom_buffer_data **data_it, int min) {
    if (min) {
        if ((*data_it)->animation.r_c) {
            _add_frame_to_animate((*data_it)->min_frame_size.size * ys->term_cols, (*data_it)->animation.speed, (*data_it)->frame_name, 1, 0);
        } else {
            _add_frame_to_animate((*data_it)->min_frame_size.size * ys->term_rows, (*data_it)->animation.speed, (*data_it)->frame_name, 0, 0);
        }
    } else {
        if ((*data_it)->animation.r_c) {
            _add_frame_to_animate((*data_it)->max_frame_size.size * ys->term_cols, (*data_it)->animation.speed, (*data_it)->frame_name, 1, 1);
        } else {
            _add_frame_to_animate((*data_it)->max_frame_size.size * ys->term_rows, (*data_it)->animation.speed, (*data_it)->frame_name, 0, 1);
        }
    }
}

static void _unfocus(char *buffer) {
    custom_buffer_data **data_it;
    char               **data_it_inner;
    yed_frame           *frame;

    array_traverse(custom_frame_buffers, data_it) {
        array_traverse((*data_it)->buff_name, data_it_inner) {
            if (strcmp((*data_it_inner), buffer) == 0) {
                if ((*data_it)->use_animation == 0) {
                    if (save_frame != NULL) {
                        yed_activate_frame(save_frame);
                    }
                } else {
                    if (save_frame != NULL) {
                        yed_activate_frame(save_frame);
                    }
                }

                if ((*data_it)->close_after) {
                    frame = yed_find_frame_by_name((*data_it)->frame_name);
                    if (frame == NULL) {
                        return;
                    }
                    yed_delete_frame(frame);
                }
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

static void _create_new_frame(char loc, char use_root, float size, int heirarchy) {
    if (ys->active_frame == NULL) {
        YEXE("frame-new"); if (ys->active_frame == NULL) { return; }
    }
}

static void set_custom_buffer_frame(int n_args, char **args) {
    custom_buffer_data *data = malloc(sizeof(custom_buffer_data));
    char               *buff_arr;

/*     check for either split or float */
    if (n_args < 3) {
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

    data->close_after = atoi(args[2]);
    if (data->close_after < 0 || data->close_after > 1) {
        yed_cerr("Expected argument 3 to be either 0 or 1");
        free(data);
        return;
    }

    if (data->is_split == 'f') {
        if (n_args < 7) {
            yed_cerr("Expected at least 7 arguments, but got %d", n_args);
            free(data);
            return;
        }
        data->max_frame_size.f_x      = atof(args[3]);
        data->max_frame_size.f_y      = atof(args[4]);
        data->max_frame_size.f_width  = atof(args[5]);
        data->max_frame_size.f_height = atof(args[6]);

        if (data->max_frame_size.f_x < 0      || data->max_frame_size.f_x > 1     ||
            data->max_frame_size.f_y < 0      || data->max_frame_size.f_y > 1     ||
            data->max_frame_size.f_width < 0  || data->max_frame_size.f_width > 1 ||
            data->max_frame_size.f_height < 0 || data->max_frame_size.f_height > 1) {
            yed_cerr("Expected arguments 4 - 7 to be between 0 - 1");
            free(data);
            return;
        }

        if (n_args < 8) {
            yed_cerr("Expected at least 8 arguments, but got %d", n_args);
            free(data);
            return;
        }
        data->use_animation = atoi(args[7]);

        if (data->use_animation < 0 || data->use_animation > 1) {
            yed_cerr("Expected argument 8 to be either 0 or 1");
            free(data);
            return;
        }

        if (data->use_animation) {
            if (n_args < 15) {
                yed_cerr("Expected at least 15 arguments, but got %d", n_args);
                free(data);
                return;
            }
            data->min_frame_size.f_y      = atof(args[8]);
            data->min_frame_size.f_x      = atof(args[9]);
            data->min_frame_size.f_height = atof(args[10]);
            data->min_frame_size.f_width  = atof(args[11]);

            if (data->min_frame_size.f_x < 0      || data->min_frame_size.f_x > 1     ||
                data->min_frame_size.f_y < 0      || data->min_frame_size.f_y > 1     ||
                data->min_frame_size.f_width < 0  || data->min_frame_size.f_width > 1 ||
                data->min_frame_size.f_height < 0 || data->min_frame_size.f_height > 1) {
                yed_cerr("Expected arguments 9 - 12 to be between 0 - 1");
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

    } else {
        if (n_args < 8) {
            yed_cerr("Expected at least 7 arguments, but got %d", n_args);
            free(data);
            return;
        }
        data->max_frame_size.split_type = args[3][0];
        data->max_frame_size.loc        = args[4][0];

        if ((data->max_frame_size.split_type != 'h' && data->max_frame_size.split_type != 'v')                             ||
            (data->max_frame_size.split_type == 'h' && data->max_frame_size.loc != 't' && data->max_frame_size.loc != 'b') ||
            (data->max_frame_size.split_type == 'v' && data->max_frame_size.loc != 'l' && data->max_frame_size.loc != 'r')) {
            yed_cerr("Expected 4th argument to be h and 5th to be either t or b or \n 4th argument to be v and 5th to be l or r");
            free(data);
            return;
        }

        data->max_frame_size.use_root = args[5][0];
        if (data->max_frame_size.use_root != 'r' && data->max_frame_size.use_root != 'a') {
            yed_cerr("Expected 6th argument to be either r or a");
            free(data);
            return;
        }

        data->max_frame_size.size = atof(args[6]);
        if (data->max_frame_size.size < 0 || data->max_frame_size.size > 1) {
            yed_cerr("Expected 7th argument to be between 0 - 1");
            free(data);
            return;
        }

        data->max_frame_size.heirarchy = atoi(args[7]);

        if (n_args < 9) {
            yed_cerr("Expected at least 9 arguments, but got %d", n_args);
            free(data);
            return;
        }
        data->use_animation = atoi(args[8]);

        if (data->use_animation < 0 || data->use_animation > 1) {
            yed_cerr("Expected 9th argument to be either 0 or 1");
            free(data);
            return;
        }

        if (data->use_animation) {
            if (n_args < 12) {
                yed_cerr("Expected at least 12 arguments, but got %d", n_args);
                free(data);
                return;
            }

            data->min_frame_size.size = atof(args[9]);
            data->animation.speed     = atoi(args[10]);
            if (data->max_frame_size.split_type == 'h') {
                data->animation.r_c   = 0;
            } else {
                data->animation.r_c   = 1;
            }
            buff_arr                  = strdup(args[11]);
        } else {
            if (n_args < 10) {
                yed_cerr("Expected at least 10 arguments, but got %d", n_args);
                free(data);
                return;
            }
            buff_arr = strdup(args[9]);
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

static void _custom_frames_unload(yed_plugin *self) {
    while (array_len(custom_frame_buffers) > 0) {
        array_pop(custom_frame_buffers);
    }

    array_free(custom_frame_buffers);
}
