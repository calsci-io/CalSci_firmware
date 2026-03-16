/*****************************************************************************
 * MicroPython C module: hybrid_sim
 *
 * Mirrors ST7565 framebuffer writes into a shadow buffer for desktop hybrid
 * simulation.
 *
 * Keep this module on the REPL-poll path only. The host/extension owns the
 * single CDC port while hybrid mode is ON, and no unsolicited binary packets
 * are emitted on stdout in this design.
 ****************************************************************************/

#include "py/obj.h"
#include "py/mphal.h"
#include "py/runtime.h"

#include "hybrid_sim_capture.h"

static bool s_hybrid_mode = false;

static void hybrid_sim_write_mode_line(bool enabled) {
    if (enabled) {
        mp_hal_stdout_tx_strn("HYBRID_MODE:ON\r\n", sizeof("HYBRID_MODE:ON\r\n") - 1);
    } else {
        mp_hal_stdout_tx_strn("HYBRID_MODE:OFF\r\n", sizeof("HYBRID_MODE:OFF\r\n") - 1);
    }
}

static void hybrid_sim_store_status_dict(mp_obj_t dict_obj, const hybrid_sim_snapshot_t *snapshot) {
    hybrid_sim_display_state_t state = snapshot->display_state;

    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_mode), mp_obj_new_bool(s_hybrid_mode));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_capture_enabled), mp_obj_new_bool(hybrid_sim_capture_enabled()));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_frame_id), mp_obj_new_int_from_uint(snapshot->frame_id));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_display_on), mp_obj_new_bool(state.display_on));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_invert), mp_obj_new_bool(state.invert));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_all_points_on), mp_obj_new_bool(state.all_points_on));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_sleep_mode), mp_obj_new_bool(state.sleep_mode));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_adc_reverse), mp_obj_new_bool(state.adc_reverse));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_com_reverse), mp_obj_new_bool(state.com_reverse));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_start_line), MP_OBJ_NEW_SMALL_INT(state.start_line));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_contrast), MP_OBJ_NEW_SMALL_INT(state.contrast));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_v0_ratio), MP_OBJ_NEW_SMALL_INT(state.v0_ratio));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_power_ctrl), MP_OBJ_NEW_SMALL_INT(state.power_ctrl));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_booster_ratio), MP_OBJ_NEW_SMALL_INT(state.booster_ratio));
}

static void hybrid_sim_store_poll_dict(mp_obj_t dict_obj, const hybrid_sim_snapshot_t *snapshot, bool include_fb, bool changed) {
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_mode), mp_obj_new_bool(s_hybrid_mode));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_capture_enabled), mp_obj_new_bool(hybrid_sim_capture_enabled()));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_frame_id), mp_obj_new_int_from_uint(snapshot->frame_id));
    mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_changed), mp_obj_new_bool(changed));
    if (include_fb) {
        mp_obj_dict_store(dict_obj, MP_OBJ_NEW_QSTR(MP_QSTR_fb), mp_obj_new_bytes((const byte *)snapshot->fb, sizeof(snapshot->fb)));
    }
}

static mp_obj_t mp_hybrid_enable(size_t n_args, const mp_obj_t *args) {
    bool enabled = true;
    if (n_args == 1) {
        enabled = mp_obj_is_true(args[0]);
    }
    hybrid_sim_capture_enable(enabled);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_hybrid_enable_obj, 0, 1, mp_hybrid_enable);

static mp_obj_t mp_hybrid_mode(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        return mp_obj_new_bool(s_hybrid_mode);
    }

    s_hybrid_mode = mp_obj_is_true(args[0]);
    hybrid_sim_write_mode_line(s_hybrid_mode);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_hybrid_mode_obj, 0, 1, mp_hybrid_mode);

static mp_obj_t mp_hybrid_enabled(void) {
    return mp_obj_new_bool(hybrid_sim_capture_enabled());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_enabled_obj, mp_hybrid_enabled);

static mp_obj_t mp_hybrid_status(void) {
    hybrid_sim_snapshot_t snapshot;
    mp_obj_t out = mp_obj_new_dict(0);
    hybrid_sim_capture_read_snapshot(&snapshot);
    hybrid_sim_store_status_dict(out, &snapshot);
    return out;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_status_obj, mp_hybrid_status);

static mp_obj_t mp_hybrid_reset(void) {
    hybrid_sim_capture_reset();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_reset_obj, mp_hybrid_reset);

static mp_obj_t mp_hybrid_frame_id(void) {
    return mp_obj_new_int_from_uint(hybrid_sim_capture_frame_id());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_frame_id_obj, mp_hybrid_frame_id);

static mp_obj_t mp_hybrid_read_fb(void) {
    uint8_t fb[HYBRID_SIM_FB_LEN];
    (void)hybrid_sim_capture_read_fb(fb, sizeof(fb));
    return mp_obj_new_bytes((const byte *)fb, sizeof(fb));
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_read_fb_obj, mp_hybrid_read_fb);

static mp_obj_t mp_hybrid_changed_since(mp_obj_t last_frame_in) {
    mp_int_t last_frame = mp_obj_get_int(last_frame_in);
    uint32_t frame_id = hybrid_sim_capture_frame_id();
    if (last_frame < 0) {
        return mp_const_true;
    }
    return mp_obj_new_bool(frame_id != (uint32_t)last_frame);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_hybrid_changed_since_obj, mp_hybrid_changed_since);

static mp_obj_t mp_hybrid_poll_state(size_t n_args, const mp_obj_t *args) {
    mp_int_t last_frame = -1;
    if (n_args == 1) {
        last_frame = mp_obj_get_int(args[0]);
    }

    hybrid_sim_snapshot_t snapshot;
    hybrid_sim_capture_read_snapshot(&snapshot);

    bool changed = false;
    bool include_fb = false;
    if (s_hybrid_mode) {
        changed = last_frame < 0 || snapshot.frame_id != (uint32_t)last_frame;
        include_fb = changed;
    }

    mp_obj_t out = mp_obj_new_dict(0);
    hybrid_sim_store_poll_dict(out, &snapshot, include_fb, changed);
    return out;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_hybrid_poll_state_obj, 0, 1, mp_hybrid_poll_state);

static mp_obj_t mp_hybrid_pop_frame(void) {
    mp_obj_t out = mp_obj_new_dict(0);
    hybrid_sim_snapshot_t snapshot;
    hybrid_sim_capture_read_snapshot(&snapshot);
    mp_obj_dict_store(out, MP_OBJ_NEW_QSTR(MP_QSTR_frame_id), mp_obj_new_int_from_uint(snapshot.frame_id));
    mp_obj_dict_store(out, MP_OBJ_NEW_QSTR(MP_QSTR_fb), mp_obj_new_bytes((const byte *)snapshot.fb, sizeof(snapshot.fb)));
    return out;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_pop_frame_obj, mp_hybrid_pop_frame);

static const mp_rom_map_elem_t hybrid_sim_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_hybrid_sim) },

    { MP_ROM_QSTR(MP_QSTR_enable), MP_ROM_PTR(&mp_hybrid_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_enabled), MP_ROM_PTR(&mp_hybrid_enabled_obj) },
    { MP_ROM_QSTR(MP_QSTR_mode), MP_ROM_PTR(&mp_hybrid_mode_obj) },
    { MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&mp_hybrid_status_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&mp_hybrid_reset_obj) },

    { MP_ROM_QSTR(MP_QSTR_frame_id), MP_ROM_PTR(&mp_hybrid_frame_id_obj) },
    { MP_ROM_QSTR(MP_QSTR_changed_since), MP_ROM_PTR(&mp_hybrid_changed_since_obj) },
    { MP_ROM_QSTR(MP_QSTR_poll_state), MP_ROM_PTR(&mp_hybrid_poll_state_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_fb), MP_ROM_PTR(&mp_hybrid_read_fb_obj) },
    { MP_ROM_QSTR(MP_QSTR_pop_frame), MP_ROM_PTR(&mp_hybrid_pop_frame_obj) },

    { MP_ROM_QSTR(MP_QSTR_WIDTH), MP_ROM_INT(HYBRID_SIM_WIDTH) },
    { MP_ROM_QSTR(MP_QSTR_HEIGHT), MP_ROM_INT(HYBRID_SIM_HEIGHT) },
    { MP_ROM_QSTR(MP_QSTR_PAGES), MP_ROM_INT(HYBRID_SIM_PAGES) },
    { MP_ROM_QSTR(MP_QSTR_FB_LEN), MP_ROM_INT(HYBRID_SIM_FB_LEN) },
};
static MP_DEFINE_CONST_DICT(hybrid_sim_module_globals, hybrid_sim_module_globals_table);

const mp_obj_module_t hybrid_sim_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&hybrid_sim_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_hybrid_sim, hybrid_sim_user_cmodule);
